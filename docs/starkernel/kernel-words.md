# StarKernel Word Specifications

## Overview

**Kernel words** are the contract boundary between VM instances and StarKernel. They replace traditional syscalls with Forth-native language constructs.

**Key Principles:**
- Kernel words are invoked like any other Forth word
- Capability checks occur before execution
- Errors fault the VM (supervisor decides recovery)
- All kernel words are ISR-safe or explicitly documented otherwise

---

## Capability System

### Capability Tokens

Capabilities are unforgeable tokens held by VM instances. Each capability grants specific privileges.

```c
/* Capability definitions (src/platform/kernel/vm_arbiter/capabilities.h) */
typedef enum {
    CAP_NONE        = 0x0000,

    /* VM lifecycle */
    CAP_SPAWN       = 0x0001,  /* Can create new VM instances */
    CAP_KILL        = 0x0002,  /* Can terminate other VMs */
    CAP_SUSPEND     = 0x0004,  /* Can suspend/resume VMs */

    /* Communication */
    CAP_SEND_ANY    = 0x0010,  /* Can send to any VM */
    CAP_RECV        = 0x0020,  /* Can receive messages */
    CAP_BROADCAST   = 0x0040,  /* Can broadcast to all VMs */

    /* Memory */
    CAP_MAP_MEM     = 0x0100,  /* Can allocate/map memory */
    CAP_SHARE_MEM   = 0x0200,  /* Can share memory with other VMs */
    CAP_PHYS_MEM    = 0x0400,  /* Can access physical addresses */

    /* Hardware */
    CAP_MMIO        = 0x1000,  /* Can access memory-mapped I/O */
    CAP_IRQ         = 0x2000,  /* Can register interrupt handlers */
    CAP_PORT_IO     = 0x4000,  /* Can use port I/O (x86 IN/OUT) */

    /* System */
    CAP_TIME_ADMIN  = 0x8000,  /* Can adjust system time */
    CAP_SUPERVISOR  = 0x10000, /* Supervisor privileges (all caps) */
} Capability;

/* Capability vector (per VM) */
typedef struct {
    uint32_t caps;              /* Bitmask of capabilities */
    uint32_t send_targets[256]; /* Per-VM send permissions (attenuated) */
} CapabilityVector;
```

### Capability Check Macro

```c
#define REQUIRE_CAP(vm, cap) \
    do { \
        if (!capability_has(vm, cap)) { \
            vm_fault(vm, FAULT_MISSING_CAPABILITY, cap); \
            return; \
        } \
    } while (0)
```

---

## Time & Scheduling Words

### `TICKS ( -- u64 )`

Get monotonic time in nanoseconds since boot.

**Capabilities:** None (always allowed)
**ISR-Safe:** Yes
**Errors:** None

**Semantics:**
```forth
: SHOW-UPTIME
  TICKS 1000000000 / .  ( print seconds since boot )
  ." seconds uptime" CR ;
```

**Implementation:**
```c
void word_ticks(VM *vm) {
    uint64_t ns = hal_time_now_ns();
    vm_push_u64(vm, ns);
}
```

---

### `YIELD ( -- )`

Voluntarily give up remaining time quantum. VM transitions to runnable state and arbiter selects next VM.

**Capabilities:** None (always allowed)
**ISR-Safe:** No (triggers VM transition)
**Errors:** None

**Semantics:**
```forth
: BUSY-LOOP-WITH-YIELD
  BEGIN
    ( do some work )
    YIELD  ( allow other VMs to run )
  AGAIN ;
```

**Implementation:**
```c
void word_yield(VM *vm) {
    /* Mark VM as yielded */
    vm->state = VM_STATE_RUNNABLE;
    vm->yield_requested = 1;

    /* Trigger reschedule (will return from word via VM transition) */
    vm_arbiter_reschedule();

    /* When VM resumes, execution continues here */
}
```

---

### `SLEEP ( ns -- )`

Block VM for at least `ns` nanoseconds. VM transitions to blocked state and is woken by arbiter timer.

**Capabilities:** None (always allowed)
**ISR-Safe:** No (triggers VM transition)
**Errors:**
- `FAULT_INVALID_ARG` if `ns` > MAX_SLEEP_NS (1 second)

**Semantics:**
```forth
: DELAY-1MS
  1000000 SLEEP ;  ( 1ms = 1,000,000ns )
```

**Implementation:**
```c
void word_sleep(VM *vm) {
    uint64_t ns = vm_pop_u64(vm);

    if (ns > MAX_SLEEP_NS) {
        vm_fault(vm, FAULT_INVALID_ARG, ns);
        return;
    }

    /* Calculate wake time */
    uint64_t wake_time = hal_time_now_ns() + ns;
    vm->wake_time = wake_time;
    vm->state = VM_STATE_BLOCKED_SLEEP;

    /* Trigger reschedule */
    vm_arbiter_reschedule();
}
```

---

## VM Lifecycle Words

### `SPAWN-VM ( image caps -- vmid )`

Create a new VM instance from an image with specified capabilities.

**Capabilities:** `CAP_SPAWN`
**ISR-Safe:** No (allocates memory)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_SPAWN`
- `FAULT_INVALID_IMAGE` if image is malformed
- `FAULT_OUT_OF_MEMORY` if VM allocation fails
- `FAULT_CAP_ATTENUATION` if `caps` includes capabilities caller doesn't have

**Semantics:**
```forth
: SPAWN-WORKER-VM
  worker-image  ( address of VM image )
  CAP_RECV CAP_SEND_ANY OR  ( grant communication capabilities )
  SPAWN-VM  ( -- vmid )
  ." Spawned worker VM ID: " . CR ;
```

**Image Format:**
```c
typedef struct {
    uint32_t magic;           /* 0x53464D56 ("SFVM") */
    uint32_t version;         /* Image format version */
    uint64_t entry_point;     /* Initial instruction pointer */
    uint64_t dict_size;       /* Dictionary size in bytes */
    uint8_t  dict_data[];     /* Serialized dictionary */
} VMImage;
```

**Implementation:**
```c
void word_spawn_vm(VM *parent, VMImage *image, uint32_t caps) {
    REQUIRE_CAP(parent, CAP_SPAWN);

    /* Attenuation: child can't have more caps than parent */
    if (caps & ~parent->caps->caps) {
        vm_fault(parent, FAULT_CAP_ATTENUATION, caps);
        return;
    }

    /* Validate image */
    if (image->magic != VM_IMAGE_MAGIC) {
        vm_fault(parent, FAULT_INVALID_IMAGE, 0);
        return;
    }

    /* Allocate new VM instance */
    VMInstance *child = vm_create_from_image(image);
    if (!child) {
        vm_fault(parent, FAULT_OUT_OF_MEMORY, 0);
        return;
    }

    /* Grant capabilities */
    child->caps->caps = caps;

    /* Enqueue as runnable */
    vm_scheduler_enqueue(child);

    /* Return VM ID to parent */
    vm_push(parent, child->vmid);
}
```

---

### `KILL-VM ( vmid -- )`

Terminate a VM instance.

**Capabilities:** `CAP_KILL`
**ISR-Safe:** No (deallocates memory)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_KILL`
- `FAULT_INVALID_VMID` if VM does not exist
- `FAULT_PERMISSION_DENIED` if attempting to kill supervisor VM

**Semantics:**
```forth
: KILL-WORKER
  worker-vmid KILL-VM ;
```

**Implementation:**
```c
void word_kill_vm(VM *vm) {
    REQUIRE_CAP(vm, CAP_KILL);

    vmid_t target_vmid = (vmid_t)vm_pop(vm);
    VMInstance *target = vm_lookup(target_vmid);

    if (!target) {
        vm_fault(vm, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    /* Cannot kill supervisor */
    if (capability_has(target, CAP_SUPERVISOR)) {
        vm_fault(vm, FAULT_PERMISSION_DENIED, 0);
        return;
    }

    /* Mark VM as dead, arbiter will clean up */
    target->state = VM_STATE_DEAD;
}
```

---

### `VM-STATUS ( vmid -- status )`

Query run state of a VM.

**Capabilities:** None (always allowed)
**ISR-Safe:** Yes
**Errors:**
- `FAULT_INVALID_VMID` if VM does not exist

**Return Values:**
```c
#define VM_STATUS_RUNNABLE  0
#define VM_STATUS_RUNNING   1
#define VM_STATUS_BLOCKED   2
#define VM_STATUS_DEAD      3
```

**Semantics:**
```forth
: WAIT-FOR-VM-EXIT
  BEGIN
    target-vmid VM-STATUS
    VM_STATUS_DEAD =
  UNTIL ;
```

**Implementation:**
```c
void word_vm_status(VM *vm) {
    vmid_t target_vmid = (vmid_t)vm_pop(vm);
    VMInstance *target = vm_lookup(target_vmid);

    if (!target) {
        vm_fault(vm, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    vm_push(vm, target->state);
}
```

---

## Communication Words

### `SEND ( vmid msg -- )`

Send a message to another VM. Non-blocking (message queued).

**Capabilities:** `CAP_SEND_ANY` (or per-VM send capability)
**ISR-Safe:** No (allocates message buffer)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks send permission
- `FAULT_INVALID_VMID` if target VM does not exist
- `FAULT_QUEUE_FULL` if target's message queue is full

**Message Format:**
```c
typedef struct {
    vmid_t sender;       /* Sender VM ID (set by kernel) */
    uint64_t timestamp;  /* Send time (set by kernel) */
    uint32_t length;     /* Message length in bytes */
    uint8_t  data[MAX_MESSAGE_SIZE];  /* Opaque payload */
} Message;

#define MAX_MESSAGE_SIZE 4096  /* 4KB max message */
```

**Semantics:**
```forth
: SEND-GREETING
  target-vmid
  greeting-buffer  ( address of message data )
  greeting-len     ( length in bytes )
  SEND ;
```

**Implementation:**
```c
void word_send(VM *vm) {
    REQUIRE_CAP(vm, CAP_SEND_ANY);  /* TODO: check per-VM caps */

    vmid_t target_vmid = (vmid_t)vm_pop(vm);
    vaddr_t msg_addr = vm_pop(vm);
    uint32_t msg_len = (uint32_t)vm_pop(vm);

    if (msg_len > MAX_MESSAGE_SIZE) {
        vm_fault(vm, FAULT_INVALID_ARG, msg_len);
        return;
    }

    VMInstance *target = vm_lookup(target_vmid);
    if (!target) {
        vm_fault(vm, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    /* Allocate message */
    Message *msg = kmalloc(sizeof(Message));
    msg->sender = vm->vmid;
    msg->timestamp = hal_time_now_ns();
    msg->length = msg_len;

    /* Copy message data from sender's memory */
    vm_copy_from_vm(vm, msg->data, msg_addr, msg_len);

    /* Enqueue message */
    if (!message_queue_enqueue(target, msg)) {
        kfree(msg);
        vm_fault(vm, FAULT_QUEUE_FULL, 0);
        return;
    }

    /* Wake target if blocked on RECV */
    if (target->state == VM_STATE_BLOCKED_RECV) {
        target->state = VM_STATE_RUNNABLE;
        vm_scheduler_enqueue(target);
    }
}
```

---

### `RECV ( -- msg )`

Receive a message (blocking). VM blocks if queue is empty.

**Capabilities:** `CAP_RECV`
**ISR-Safe:** No (blocks)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_RECV`

**Semantics:**
```forth
: MESSAGE-LOOP
  BEGIN
    RECV  ( -- msg-addr )
    DUP process-message
    free-message
  AGAIN ;
```

**Implementation:**
```c
void word_recv(VM *vm) {
    REQUIRE_CAP(vm, CAP_RECV);

    /* Try to dequeue message */
    Message *msg = message_queue_dequeue(vm);

    if (!msg) {
        /* No messages, block until one arrives */
        vm->state = VM_STATE_BLOCKED_RECV;
        vm_arbiter_reschedule();
        /* When resumed, try again */
        msg = message_queue_dequeue(vm);
    }

    /* Copy message to VM's memory (allocate from VM heap) */
    vaddr_t msg_addr = vm_heap_alloc(vm, sizeof(Message));
    vm_copy_to_vm(vm, msg_addr, msg, sizeof(Message));

    /* Push message address to stack */
    vm_push(vm, msg_addr);

    /* Free kernel message buffer */
    kfree(msg);
}
```

---

### `RECV? ( -- msg f )`

Non-blocking receive. Returns message and true if available, else false.

**Capabilities:** `CAP_RECV`
**ISR-Safe:** Yes
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_RECV`

**Semantics:**
```forth
: POLL-MESSAGES
  RECV? IF
    ( msg on stack )
    process-message
  ELSE
    ( no messages )
    DROP
  THEN ;
```

**Implementation:**
```c
void word_recv_nonblock(VM *vm) {
    REQUIRE_CAP(vm, CAP_RECV);

    Message *msg = message_queue_dequeue(vm);

    if (msg) {
        /* Message available */
        vaddr_t msg_addr = vm_heap_alloc(vm, sizeof(Message));
        vm_copy_to_vm(vm, msg_addr, msg, sizeof(Message));
        vm_push(vm, msg_addr);
        vm_push(vm, 1);  /* true */
        kfree(msg);
    } else {
        /* No message */
        vm_push(vm, 0);  /* false */
    }
}
```

---

## Memory Words

### `MAP-MEM ( size flags -- addr )`

Allocate and map memory region.

**Capabilities:** `CAP_MAP_MEM`
**ISR-Safe:** No (allocates pages)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_MAP_MEM`
- `FAULT_OUT_OF_MEMORY` if allocation fails
- `FAULT_INVALID_ARG` if size exceeds VM envelope limit

**Flags:**
```c
#define MEM_READ   0x1
#define MEM_WRITE  0x2
#define MEM_EXEC   0x4
```

**Semantics:**
```forth
: ALLOC-BUFFER
  4096  ( 4KB )
  MEM_READ MEM_WRITE OR
  MAP-MEM  ( -- addr )
  buffer ! ;
```

**Implementation:**
```c
void word_map_mem(VM *vm) {
    REQUIRE_CAP(vm, CAP_MAP_MEM);

    size_t size = (size_t)vm_pop(vm);
    uint32_t flags = (uint32_t)vm_pop(vm);

    if (size > vm->envelope_limit) {
        vm_fault(vm, FAULT_INVALID_ARG, size);
        return;
    }

    /* Allocate physical pages */
    uint64_t paddr = hal_mem_alloc_pages(size / 4096);
    if (!paddr) {
        vm_fault(vm, FAULT_OUT_OF_MEMORY, 0);
        return;
    }

    /* Find free virtual address in VM's space */
    vaddr_t vaddr = vm_find_free_vaddr(vm, size);

    /* Map pages into VM's page tables */
    hal_mem_map(vaddr, paddr, size, flags);

    /* Track mapping */
    vm_track_mapping(vm, vaddr, paddr, size);

    vm_push(vm, vaddr);
}
```

---

### `UNMAP-MEM ( addr size -- )`

Release mapped memory region.

**Capabilities:** `CAP_MAP_MEM`
**ISR-Safe:** No (deallocates pages)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_MAP_MEM`
- `FAULT_INVALID_ARG` if region was not allocated by MAP-MEM

**Semantics:**
```forth
: FREE-BUFFER
  buffer @ 4096 UNMAP-MEM ;
```

---

### `SHARE-MEM ( addr size vmid -- )`

Share a memory region with another VM.

**Capabilities:** `CAP_SHARE_MEM`
**ISR-Safe:** No (modifies page tables)
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_SHARE_MEM`
- `FAULT_INVALID_VMID` if target VM does not exist
- `FAULT_INVALID_ARG` if addr not within caller's envelope

**Semantics:**
```forth
: SHARE-BUFFER-WITH-WORKER
  shared-buffer @ 4096 worker-vmid SHARE-MEM ;
```

**Implementation:**
```c
void word_share_mem(VM *vm) {
    REQUIRE_CAP(vm, CAP_SHARE_MEM);

    vaddr_t addr = vm_pop(vm);
    size_t size = (size_t)vm_pop(vm);
    vmid_t target_vmid = (vmid_t)vm_pop(vm);

    /* Verify addr is within caller's envelope */
    if (!vm_owns_region(vm, addr, size)) {
        vm_fault(vm, FAULT_INVALID_ARG, addr);
        return;
    }

    VMInstance *target = vm_lookup(target_vmid);
    if (!target) {
        vm_fault(vm, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    /* Get physical address */
    uint64_t paddr = vm_vaddr_to_paddr(vm, addr);

    /* Map into target's address space (same vaddr) */
    hal_mem_map(addr, paddr, size, MEM_READ | MEM_WRITE);

    /* Track shared region */
    vm_track_shared_region(vm, target, addr, size);
}
```

---

## Hardware Access Words

### `MMIO-READ ( paddr -- u64 )`

Read 64-bit value from memory-mapped I/O.

**Capabilities:** `CAP_MMIO`
**ISR-Safe:** Yes
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_MMIO`
- `FAULT_PAGE_FAULT` if paddr not mapped as MMIO

**Semantics:**
```forth
: READ-UART-STATUS
  0x3FD MMIO-READ  ( read UART line status register )
  0x20 AND 0<> IF  ( check if transmit ready )
    ." UART ready" CR
  THEN ;
```

**Implementation:**
```c
void word_mmio_read(VM *vm) {
    REQUIRE_CAP(vm, CAP_MMIO);

    uint64_t paddr = vm_pop_u64(vm);

    /* Verify paddr is in valid MMIO range */
    if (!is_valid_mmio(paddr)) {
        vm_fault(vm, FAULT_PAGE_FAULT, paddr);
        return;
    }

    /* Perform read (volatile) */
    uint64_t value = *(volatile uint64_t *)paddr;

    vm_push_u64(vm, value);
}
```

---

### `MMIO-WRITE ( u64 paddr -- )`

Write 64-bit value to memory-mapped I/O.

**Capabilities:** `CAP_MMIO`
**ISR-Safe:** Yes
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_MMIO`
- `FAULT_PAGE_FAULT` if paddr not mapped as MMIO

**Semantics:**
```forth
: WRITE-UART-DATA
  0x48 ( 'H' )
  0x3F8 MMIO-WRITE ;  ( write to UART data register )
```

---

### `IRQ-REGISTER ( irq handler -- )`

Register an interrupt handler for a hardware IRQ.

**Capabilities:** `CAP_IRQ`
**ISR-Safe:** No
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_IRQ`
- `FAULT_INVALID_ARG` if IRQ number invalid
- `FAULT_IRQ_IN_USE` if IRQ already registered

**Handler Signature:**
```forth
: IRQ-HANDLER  ( -- )
  \ ISR code here
  \ MUST be fast (< 10µs)
  \ MUST NOT block
;
```

**Semantics:**
```forth
: REGISTER-TIMER-IRQ
  32  ( APIC timer IRQ )
  ' timer-isr-handler
  IRQ-REGISTER ;
```

**Implementation:**
```c
void word_irq_register(VM *vm) {
    REQUIRE_CAP(vm, CAP_IRQ);

    unsigned int irq = (unsigned int)vm_pop(vm);
    vaddr_t handler_addr = vm_pop(vm);

    if (irq >= MAX_IRQ) {
        vm_fault(vm, FAULT_INVALID_ARG, irq);
        return;
    }

    /* Check if already registered */
    if (irq_table[irq].vm != NULL) {
        vm_fault(vm, FAULT_IRQ_IN_USE, irq);
        return;
    }

    /* Register handler */
    irq_table[irq].vm = vm;
    irq_table[irq].handler_addr = handler_addr;

    /* Configure interrupt controller */
    hal_irq_register(irq, vm_irq_trampoline, &irq_table[irq]);
}
```

---

## Capability Management Words

### `HAS-CAP? ( cap -- f )`

Check if current VM has a capability.

**Capabilities:** None (always allowed)
**ISR-Safe:** Yes
**Errors:** None

**Semantics:**
```forth
: CHECK-SPAWN-PERMISSION
  CAP_SPAWN HAS-CAP? IF
    ." Can spawn VMs" CR
  ELSE
    ." Cannot spawn VMs" CR
  THEN ;
```

**Implementation:**
```c
void word_has_cap(VM *vm) {
    uint32_t cap = (uint32_t)vm_pop(vm);
    int has = capability_has(vm, cap);
    vm_push(vm, has ? 1 : 0);
}
```

---

### `GRANT-CAP ( vmid cap -- )`

Grant a capability to another VM (attenuation: can only grant what you have).

**Capabilities:** Must have the capability being granted
**ISR-Safe:** No
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller doesn't have `cap`
- `FAULT_INVALID_VMID` if target VM does not exist

**Semantics:**
```forth
: GRANT-SEND-TO-WORKER
  worker-vmid CAP_SEND_ANY GRANT-CAP ;
```

---

### `REVOKE-CAP ( vmid cap -- )`

Revoke a capability from another VM.

**Capabilities:** `CAP_SUPERVISOR` (only supervisor can revoke)
**ISR-Safe:** No
**Errors:**
- `FAULT_MISSING_CAPABILITY` if caller lacks `CAP_SUPERVISOR`
- `FAULT_INVALID_VMID` if target VM does not exist

---

## Fault Handling

When a kernel word encounters an error, it invokes `vm_fault()`:

```c
typedef enum {
    FAULT_MISSING_CAPABILITY,
    FAULT_INVALID_VMID,
    FAULT_INVALID_IMAGE,
    FAULT_OUT_OF_MEMORY,
    FAULT_CAP_ATTENUATION,
    FAULT_INVALID_ARG,
    FAULT_QUEUE_FULL,
    FAULT_PERMISSION_DENIED,
    FAULT_PAGE_FAULT,
    FAULT_IRQ_IN_USE,
} FaultCode;

void vm_fault(VM *vm, FaultCode fault, uint64_t info) {
    /* Log fault */
    uart_puts("VM fault: ");
    uart_put_hex(fault);
    uart_puts("\r\n");

    /* Check if VM has fault handler */
    if (vm->fault_handler) {
        /* Push fault info to VM stack */
        vm_push(vm, fault);
        vm_push_u64(vm, info);

        /* Jump to fault handler */
        vm->ip = vm->fault_handler;
    } else {
        /* No handler, kill VM */
        vm->state = VM_STATE_DEAD;

        /* Notify supervisor (VM[0]) */
        Message msg;
        msg.sender = vm->vmid;
        msg.length = sizeof(FaultReport);
        FaultReport *report = (FaultReport *)msg.data;
        report->fault = fault;
        report->info = info;

        VMInstance *supervisor = vm_lookup(0);
        message_queue_enqueue(supervisor, &msg);
    }
}
```

---

## Summary Table

| Word | Stack Effect | Capabilities | ISR-Safe |
|------|-------------|--------------|----------|
| `TICKS` | `( -- u64 )` | None | ✅ |
| `YIELD` | `( -- )` | None | ❌ |
| `SLEEP` | `( ns -- )` | None | ❌ |
| `SPAWN-VM` | `( image caps -- vmid )` | `CAP_SPAWN` | ❌ |
| `KILL-VM` | `( vmid -- )` | `CAP_KILL` | ❌ |
| `VM-STATUS` | `( vmid -- status )` | None | ✅ |
| `SEND` | `( vmid msg -- )` | `CAP_SEND_ANY` | ❌ |
| `RECV` | `( -- msg )` | `CAP_RECV` | ❌ |
| `RECV?` | `( -- msg f )` | `CAP_RECV` | ✅ |
| `MAP-MEM` | `( size flags -- addr )` | `CAP_MAP_MEM` | ❌ |
| `UNMAP-MEM` | `( addr size -- )` | `CAP_MAP_MEM` | ❌ |
| `SHARE-MEM` | `( addr size vmid -- )` | `CAP_SHARE_MEM` | ❌ |
| `MMIO-READ` | `( paddr -- u64 )` | `CAP_MMIO` | ✅ |
| `MMIO-WRITE` | `( u64 paddr -- )` | `CAP_MMIO` | ✅ |
| `IRQ-REGISTER` | `( irq handler -- )` | `CAP_IRQ` | ❌ |
| `HAS-CAP?` | `( cap -- f )` | None | ✅ |
| `GRANT-CAP` | `( vmid cap -- )` | Must have `cap` | ❌ |
| `REVOKE-CAP` | `( vmid cap -- )` | `CAP_SUPERVISOR` | ❌ |

---

## Next Steps

See `StarKernel-Outline-part-II.md` for:
- VM control block structure
- VM arbiter scheduling algorithm
- Message queue implementation details
- VM transition mechanics (context switch)

---

*Document Version: 1.0*
*Date: 2025-12-17*
*Status: Complete specification, pending implementation*
