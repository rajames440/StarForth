# StarKernel Architecture Outline - Part II

## Overview

This document covers the **implementation details** for StarKernel's core subsystems:
- VM control block structure
- **Security model (Capabilities + ACLs)**
- VM arbiter scheduling algorithm
- Message queue implementation
- VM lifecycle state machine
- VM transition mechanics (context switching)
- Interrupt handling in VM context

**Prerequisite:** Read `StarKernel-Outline-part-I.md` for architectural principles.

---

## VM Control Block

### VMInstance Structure

```c
/* src/platform/kernel/vm_arbiter/vm_instance.h */

typedef uint32_t vmid_t;

typedef enum {
    VM_STATE_RUNNABLE,       /* Ready to run */
    VM_STATE_RUNNING,        /* Currently executing */
    VM_STATE_BLOCKED_RECV,   /* Blocked on RECV */
    VM_STATE_BLOCKED_SLEEP,  /* Blocked on SLEEP */
    VM_STATE_DEAD,           /* Terminated */
} VMState;

typedef struct VMInstance {
    /* Identity */
    vmid_t vmid;             /* Unique VM ID */
    vmid_t parent_vmid;      /* Parent VM (spawner) */

    /* Run state */
    VMState state;           /* Current state */
    uint64_t wake_time;      /* Wake time (if BLOCKED_SLEEP) */
    int yield_requested;     /* YIELD invoked flag */

    /* Scheduling */
    uint64_t quantum_start;  /* When current quantum started */
    uint64_t quantum_ns;     /* Time quantum in nanoseconds */
    uint64_t total_runtime;  /* Total CPU time consumed */
    int priority;            /* Scheduling priority (0 = highest) */

    /* VM execution state (saved/restored on transition) */
    vaddr_t ip;              /* Instruction pointer */
    vaddr_t sp;              /* Stack pointer */
    vaddr_t rp;              /* Return stack pointer */
    uint64_t registers[16];  /* General-purpose registers (if needed) */

    /* Memory envelope */
    vaddr_t vm_base;         /* Base virtual address */
    size_t vm_size;          /* Total VM memory size */
    size_t envelope_limit;   /* Max memory VM can allocate */
    void *page_table;        /* VM's page table root */

    /* StarForth VM state */
    VM *forth_vm;            /* Pointer to embedded StarForth VM struct */

    /* Capabilities */
    CapabilityVector *caps;  /* Capability vector */

    /* Message queue */
    MessageQueue *msg_queue; /* Incoming message queue */

    /* Physics state (per-VM) */
    uint64_t execution_heat; /* Total execution heat */
    RollingWindow *window;   /* Per-VM rolling window of truth */
    HeartbeatState *heartbeat; /* Per-VM heartbeat state */

    /* Fault handling */
    vaddr_t fault_handler;   /* Fault handler entry point (0 = none) */

    /* Linked list (for scheduler queues) */
    struct VMInstance *next;
    struct VMInstance *prev;
} VMInstance;
```

---

## Security Model: Capabilities + ACLs

### Overview

StarKernel employs a **two-layer security model**:

1. **Capabilities** - Coarse-grained permissions (what operations are possible)
2. **ACLs (Access Control Lists)** - Fine-grained permissions (access to specific resources)

**Design Principle:**
> **Capabilities grant the right to attempt an operation.**
>
> **ACLs grant the right to access a specific resource.**
>
> Both must succeed for an operation to proceed.

**Example:**
- VM has `CAP_SEND_ANY` (capability)
- But VM[3] has ACL denying messages from this VM (ACL)
- Result: SEND to VM[3] **fails** (capability satisfied, ACL denied)

---

### Layer 1: Capabilities (Already Defined)

See `StarKernel-Outline-part-I.md` for full capability definitions.

**Quick Recap:**
```c
typedef enum {
    CAP_SPAWN       = 0x0001,  /* Can create VMs */
    CAP_KILL        = 0x0002,  /* Can terminate VMs */
    CAP_SEND_ANY    = 0x0010,  /* Can send to any VM */
    CAP_RECV        = 0x0020,  /* Can receive messages */
    CAP_MAP_MEM     = 0x0100,  /* Can allocate memory */
    CAP_SHARE_MEM   = 0x0200,  /* Can share memory */
    CAP_MMIO        = 0x1000,  /* Can access MMIO */
    CAP_IRQ         = 0x2000,  /* Can register IRQs */
    CAP_SUPERVISOR  = 0x10000, /* All capabilities */
} Capability;
```

**Capability checks are fast** (single bitwise AND):
```c
static inline int capability_has(VMInstance *vm, Capability cap) {
    return (vm->caps->caps & cap) != 0;
}
```

---

### Layer 2: ACLs (Resource-Level Permissions)

ACLs control access to **specific resources**:
- **Words** (Forth dictionary entries)
- **VM instances** (specific VMs)
- **Block devices** (storage blocks)
- **Memory regions** (shared pages)
- **IRQs** (interrupt vectors)

---

### ACL Data Structures

#### ACL Entry

```c
/* src/platform/kernel/vm_arbiter/acl.h */

typedef enum {
    ACL_READ    = 0x01,
    ACL_WRITE   = 0x02,
    ACL_EXEC    = 0x04,
    ACL_DELETE  = 0x08,
    ACL_GRANT   = 0x10,  /* Can grant access to others */
} ACLPermission;

typedef struct ACLEntry {
    vmid_t subject;          /* VM being granted/denied access */
    uint8_t permissions;     /* Bitmask of ACLPermission */
    struct ACLEntry *next;   /* Linked list */
} ACLEntry;

typedef struct ACL {
    vmid_t owner;            /* Owner VM (full access) */
    ACLEntry *entries;       /* Access control list */
    uint8_t default_perms;   /* Default for unlisted VMs */
} ACL;
```

**Example ACL:**
```
Resource: Word "REBOOT"
Owner: VM[0]
Entries:
  - VM[0]: READ | WRITE | EXEC | DELETE | GRANT (owner, implicit)
  - VM[1]: EXEC (allowed to invoke REBOOT)
Default: 0 (deny all)
```

---

### ACL on Words (Dictionary Entries)

Each word in the dictionary has an associated ACL.

#### Extended DictEntry Structure

```c
/* include/vm.h */

typedef struct DictEntry {
    char name[32];
    WordFunc func;
    int flags;

    /* Physics metadata */
    uint64_t execution_heat;
    PhysicsMetadata physics;
    TransitionMetrics *transition_metrics;

    /* ACL (new) */
    ACL *acl;  /* Access control list for this word */

    struct DictEntry *next;
} DictEntry;
```

#### Word Invocation with ACL Check

```c
/* Modified word execution in VM interpreter loop */

void forth_execute_word(VM *vm, DictEntry *word) {
    VMInstance *vm_inst = vm->vm_instance;  /* Back-pointer to VMInstance */

    /* Check ACL */
    if (!acl_check(word->acl, vm_inst->vmid, ACL_EXEC)) {
        vm_fault(vm_inst, FAULT_ACL_DENIED, (uint64_t)word->name);
        return;
    }

    /* Capability check (if word requires caps) */
    if (word->required_capability) {
        if (!capability_has(vm_inst, word->required_capability)) {
            vm_fault(vm_inst, FAULT_MISSING_CAPABILITY, word->required_capability);
            return;
        }
    }

    /* Execute word */
    word->func(vm);
}
```

#### Creating Words with ACLs

```forth
\ VM[0] creates a privileged word
: DEFINE-PRIVILEGED-WORD
  : REBOOT  ( -- )
    ." Rebooting system..." CR
    0x64 0xFE MMIO-WRITE  ( write to keyboard controller reset )
  ;

  \ Set ACL: only VM[0] can execute
  ' REBOOT word>acl  ( get word's ACL )
  VM[0] ACL_EXEC acl-grant  ( grant EXEC to VM[0] only )
  0 acl-set-default  ( deny all others )
;
```

**Performance:** ACL check adds ~20-50 cycles per word invocation (acceptable overhead).

---

### ACL on VM Instances

Each VM has an ACL controlling what other VMs can do to it.

#### VM ACL Permissions

```c
typedef enum {
    VM_ACL_SEND     = 0x01,  /* Can send messages to this VM */
    VM_ACL_INSPECT  = 0x02,  /* Can query VM status */
    VM_ACL_KILL     = 0x04,  /* Can terminate this VM */
    VM_ACL_SUSPEND  = 0x08,  /* Can suspend/resume this VM */
    VM_ACL_SHARE    = 0x10,  /* Can share memory with this VM */
} VMACLPermission;
```

#### Updated VMInstance Structure

```c
typedef struct VMInstance {
    /* ... existing fields ... */

    /* VM-level ACL (new) */
    ACL *vm_acl;  /* Controls access to this VM instance */
} VMInstance;
```

#### SEND with ACL Check

```c
void word_send(VM *vm) {
    VMInstance *sender = vm->vm_instance;
    REQUIRE_CAP(sender, CAP_SEND_ANY);

    vmid_t target_vmid = (vmid_t)vm_pop(vm);
    VMInstance *target = vm_lookup(target_vmid);

    if (!target) {
        vm_fault(sender, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    /* ACL Check: Can sender send to target? */
    if (!acl_check(target->vm_acl, sender->vmid, VM_ACL_SEND)) {
        vm_fault(sender, FAULT_ACL_DENIED, target_vmid);
        return;
    }

    /* ... rest of SEND implementation ... */
}
```

#### Example: VM Isolation

```forth
\ VM[0] spawns isolated worker VM
: CREATE-ISOLATED-WORKER
  worker-image CAP_RECV SPAWN-VM  ( -- worker-vmid )

  \ Configure worker's ACL: only VM[0] can send to it
  dup vm>acl  ( get worker's VM ACL )
  VM[0] VM_ACL_SEND acl-grant  ( grant SEND to VM[0] )
  0 acl-set-default  ( deny all others )

  worker-vmid ! ;
```

---

### ACL on Block Devices

Block devices have ACLs specifying which VMs can read/write which blocks.

#### Block Device ACL Structure

```c
/* src/platform/kernel/block/block_acl.h */

typedef struct BlockRange {
    uint32_t start_block;
    uint32_t end_block;
    uint8_t permissions;  /* ACL_READ | ACL_WRITE */
    struct BlockRange *next;
} BlockRange;

typedef struct BlockDeviceACL {
    vmid_t owner;
    BlockRange *ranges;  /* Per-VM block ranges */
    uint8_t default_perms;
} BlockDeviceACL;

typedef struct BlockDevice {
    char name[16];
    uint64_t block_count;
    uint32_t block_size;

    /* ACL */
    BlockDeviceACL *acl;

    /* Device operations */
    int (*read_block)(uint32_t block, void *buf);
    int (*write_block)(uint32_block, const void *buf);
} BlockDevice;
```

#### Block I/O with ACL Check

```c
int block_device_read(BlockDevice *dev, VMInstance *vm, uint32_t block, void *buf) {
    /* Check if VM has read permission for this block */
    if (!block_acl_check(dev->acl, vm->vmid, block, ACL_READ)) {
        return -EACCES;  /* Access denied */
    }

    /* Perform read */
    return dev->read_block(block, buf);
}

int block_device_write(BlockDevice *dev, VMInstance *vm, uint32_t block, const void *buf) {
    /* Check if VM has write permission for this block */
    if (!block_acl_check(dev->acl, vm->vmid, block, ACL_WRITE)) {
        return -EACCES;  /* Access denied */
    }

    /* Perform write */
    return dev->write_block(block, buf);
}
```

#### Block ACL Check Implementation

```c
int block_acl_check(BlockDeviceACL *acl, vmid_t vm, uint32_t block, ACLPermission perm) {
    /* Owner has full access */
    if (vm == acl->owner) {
        return 1;
    }

    /* Search for matching range */
    BlockRange *range = acl->ranges;
    while (range) {
        if (range->start_block <= block && block <= range->end_block) {
            return (range->permissions & perm) != 0;
        }
        range = range->next;
    }

    /* No match, check default */
    return (acl->default_perms & perm) != 0;
}
```

#### Example: Partitioned Block Device

```forth
\ VM[0] partitions a block device for two worker VMs

: PARTITION-DISK
  disk0  ( block device )

  \ Grant VM[1] access to blocks 0-1023 (read/write)
  dup block-acl>
  VM[1] 0 1023 ACL_READ ACL_WRITE OR block-acl-grant-range

  \ Grant VM[2] access to blocks 1024-2047 (read/write)
  dup block-acl>
  VM[2] 1024 2047 ACL_READ ACL_WRITE OR block-acl-grant-range

  \ Deny all by default
  0 block-acl-set-default
  drop ;
```

#### Kernel Words for Block I/O

```forth
BLOCK-READ  ( device block buffer -- status )
  \ Reads a block from device into buffer
  \ Returns 0 on success, error code on failure
  \ ACL checked: requires ACL_READ on block

BLOCK-WRITE ( device block buffer -- status )
  \ Writes buffer to device block
  \ Returns 0 on success, error code on failure
  \ ACL checked: requires ACL_WRITE on block

BLOCK-ACL-GRANT-RANGE ( device vmid start end perms -- )
  \ Grants permissions to VM for block range
  \ Requires: CAP_SUPERVISOR or device ownership
```

---

### ACL on Memory Regions (Shared Pages)

When VMs share memory, ACLs control which VMs can access the shared region.

#### Shared Memory ACL

```c
typedef struct SharedRegion {
    vaddr_t addr;
    size_t size;
    ACL *acl;  /* Controls access to this region */
    vmid_t *sharers;  /* List of VMs sharing this region */
    int sharer_count;
} SharedRegion;
```

#### SHARE-MEM with ACL

```c
void word_share_mem(VM *vm) {
    VMInstance *owner = vm->vm_instance;
    REQUIRE_CAP(owner, CAP_SHARE_MEM);

    vaddr_t addr = vm_pop(vm);
    size_t size = (size_t)vm_pop(vm);
    vmid_t target_vmid = (vmid_t)vm_pop(vm);

    /* Verify ownership */
    if (!vm_owns_region(owner, addr, size)) {
        vm_fault(owner, FAULT_INVALID_ARG, addr);
        return;
    }

    VMInstance *target = vm_lookup(target_vmid);
    if (!target) {
        vm_fault(owner, FAULT_INVALID_VMID, target_vmid);
        return;
    }

    /* Check target's VM ACL: can we share with them? */
    if (!acl_check(target->vm_acl, owner->vmid, VM_ACL_SHARE)) {
        vm_fault(owner, FAULT_ACL_DENIED, target_vmid);
        return;
    }

    /* Create shared region with ACL */
    SharedRegion *region = kmalloc(sizeof(SharedRegion));
    region->addr = addr;
    region->size = size;
    region->acl = acl_create(owner->vmid);  /* Owner is creator */

    /* Grant target read/write access */
    acl_grant(region->acl, target_vmid, ACL_READ | ACL_WRITE);

    /* Map into target's address space */
    uint64_t paddr = vm_vaddr_to_paddr(owner, addr);
    hal_mem_map(addr, paddr, size, MEM_READ | MEM_WRITE);

    /* Track shared region */
    vm_track_shared_region(owner, target, region);
}
```

---

### ACL on IRQs (Interrupt Vectors)

IRQs have ACLs to prevent unauthorized VM from registering handlers.

#### IRQ ACL

```c
typedef struct IRQDescriptor {
    unsigned int irq;
    VMInstance *owner;  /* VM that registered handler */
    vaddr_t handler_addr;
    ACL *acl;  /* Who can register handler for this IRQ */
} IRQDescriptor;

static IRQDescriptor irq_table[256];
```

#### IRQ-REGISTER with ACL

```c
void word_irq_register(VM *vm) {
    VMInstance *vm_inst = vm->vm_instance;
    REQUIRE_CAP(vm_inst, CAP_IRQ);

    unsigned int irq = (unsigned int)vm_pop(vm);
    vaddr_t handler_addr = vm_pop(vm);

    if (irq >= 256) {
        vm_fault(vm_inst, FAULT_INVALID_ARG, irq);
        return;
    }

    /* ACL Check: Can this VM register handler for this IRQ? */
    if (!acl_check(irq_table[irq].acl, vm_inst->vmid, ACL_WRITE)) {
        vm_fault(vm_inst, FAULT_ACL_DENIED, irq);
        return;
    }

    /* Register handler */
    irq_table[irq].owner = vm_inst;
    irq_table[irq].handler_addr = handler_addr;

    /* Configure APIC */
    hal_irq_register(irq, vm_irq_trampoline, &irq_table[irq]);
}
```

---

### ACL Management Kernel Words

```forth
\ ACL creation and manipulation

ACL-CREATE ( owner -- acl )
  \ Creates a new ACL with specified owner
  \ Owner has full permissions implicitly

ACL-GRANT ( acl subject perms -- )
  \ Grants permissions to subject
  \ Requires: ACL_GRANT permission or ownership

ACL-REVOKE ( acl subject perms -- )
  \ Revokes permissions from subject
  \ Requires: ownership

ACL-CHECK ( acl subject perms -- f )
  \ Checks if subject has permissions
  \ Returns true if all specified perms are granted

ACL-SET-DEFAULT ( acl perms -- )
  \ Sets default permissions for unlisted subjects
  \ Requires: ownership

ACL-LIST ( acl -- entries )
  \ Returns list of ACL entries (for inspection)
  \ Requires: ACL_READ permission

\ Specialized ACL words

WORD-ACL ( xt -- acl )
  \ Get ACL for a word (execution token)

VM-ACL ( vmid -- acl )
  \ Get ACL for a VM instance

BLOCK-ACL ( device -- acl )
  \ Get ACL for block device

IRQ-ACL ( irq -- acl )
  \ Get ACL for IRQ vector
```

---

### ACL Caching with TTL (Time To Live)

**Critical Optimization:** Don't check ACL on EVERY word invocation. Use TTL-based caching.

#### Design Choice: Absolute TTL vs. Sliding TTL

**Option 1: Sliding TTL (Reset on use)**
- Each use resets TTL to full duration
- Hot words never expire
- **Problem:** VM could maintain access indefinitely even if ACL is revoked

**Option 2: Absolute TTL (Fixed window)** ✅ **RECOMMENDED**
- TTL counts down regardless of usage
- ACL revocations take effect within bounded time
- Still get 99%+ cache hit rate on hot words
- **Security property:** "Revocation latency ≤ TTL_MAX"

**Decision: Use Absolute TTL for security.**

---

#### ACL Cache Structure with TTL

```c
/* Per-VM ACL cache */
#define ACL_CACHE_SIZE 256
#define ACL_TTL_NS (1000000000ULL)  /* 1 second TTL */

typedef struct ACLCacheEntry {
    void *resource;      /* Word, VM, device, etc. */
    uint8_t permissions; /* Cached permissions */
    uint64_t expire_time; /* Absolute expiration time (NOT reset on use) */
    uint32_t hit_count;  /* Diagnostic: how many times used */
} ACLCacheEntry;

typedef struct ACLCache {
    ACLCacheEntry entries[ACL_CACHE_SIZE];
    uint64_t hits;       /* Total cache hits */
    uint64_t misses;     /* Total cache misses */
    uint64_t expires;    /* Total TTL expirations */
} ACLCache;

/* Add to VMInstance */
typedef struct VMInstance {
    /* ... existing fields ... */

    ACLCache *acl_cache;  /* Per-VM ACL cache with TTL */
} VMInstance;
```

---

#### TTL-Based ACL Check

```c
int acl_check_cached(VMInstance *vm, ACL *acl, ACLPermission perm) {
    uint64_t now = hal_time_now_ns();
    uint32_t hash = hash_ptr(acl) % ACL_CACHE_SIZE;
    ACLCacheEntry *entry = &vm->acl_cache->entries[hash];

    /* Check if cache entry is valid AND not expired */
    if (entry->resource == acl && now < entry->expire_time) {
        /* Cache hit (TTL not expired) */
        vm->acl_cache->hits++;
        entry->hit_count++;  /* Track usage for diagnostics */

        /* NOTE: Do NOT reset expire_time here (absolute TTL) */

        return (entry->permissions & perm) != 0;
    }

    /* Cache miss or TTL expired */
    if (entry->resource == acl && now >= entry->expire_time) {
        vm->acl_cache->expires++;  /* TTL expiration */
    } else {
        vm->acl_cache->misses++;   /* True cache miss */
    }

    /* Perform full ACL check */
    int result = acl_check(acl, vm->vmid, perm);

    /* Update cache with new TTL */
    entry->resource = acl;
    entry->permissions = acl_get_permissions(acl, vm->vmid);
    entry->expire_time = now + ACL_TTL_NS;  /* Absolute expiration */
    entry->hit_count = 0;  /* Reset hit counter */

    return result;
}
```

---

#### Cache Invalidation (Explicit Revocation)

When ACL is modified, **optionally** invalidate caches immediately (don't wait for TTL):

```c
void acl_grant(ACL *acl, vmid_t subject, ACLPermission perm) {
    /* ... update ACL ... */

    /* Option 1: Immediate invalidation (strong consistency) */
    #ifdef ACL_IMMEDIATE_INVALIDATION
    for (int i = 0; i < MAX_VMS; i++) {
        if (arbiter.vm_table[i]) {
            acl_cache_invalidate(arbiter.vm_table[i]->acl_cache, acl);
        }
    }
    #endif

    /* Option 2: Lazy invalidation (wait for TTL) */
    /* No action needed - TTL will expire naturally within ACL_TTL_NS */
}
```

**Trade-off:**
- **Immediate invalidation:** Strong consistency, but requires iterating all VMs (expensive)
- **Lazy invalidation:** Eventual consistency (bounded by TTL), zero overhead

**Recommendation:** Use lazy invalidation unless ACL change is critical (e.g., security breach response).

---

#### Tunable TTL Parameters

```c
/* ACL TTL configuration (tunable per resource type) */
#define ACL_WORD_TTL_NS     (1000000000ULL)   /* 1 second (words change rarely) */
#define ACL_VM_TTL_NS       (100000000ULL)    /* 100ms (VMs more dynamic) */
#define ACL_BLOCK_TTL_NS    (500000000ULL)    /* 500ms (medium) */
#define ACL_IRQ_TTL_NS      (2000000000ULL)   /* 2 seconds (IRQs very static) */

/* TTL can be adjusted based on resource type */
uint64_t acl_get_ttl(void *resource) {
    if (is_word(resource)) return ACL_WORD_TTL_NS;
    if (is_vm(resource))   return ACL_VM_TTL_NS;
    if (is_block(resource)) return ACL_BLOCK_TTL_NS;
    if (is_irq(resource))  return ACL_IRQ_TTL_NS;
    return ACL_TTL_NS;  /* Default */
}
```

---

#### Performance Characteristics

**Without TTL (check every invocation):**
- Word invocation overhead: ~50-100 cycles
- Hot word (1000 invocations/sec): 50,000-100,000 cycles/sec overhead

**With TTL (1 second absolute):**
- First invocation: ~50-100 cycles (cache miss)
- Next 999 invocations: ~5-10 cycles (cache hit)
- TTL expires: ~50-100 cycles (recheck)
- Total overhead: ~5,000-10,000 cycles/sec (10-20x reduction!)

**Hit rate:**
- Hot words (>10 invocations/sec): >99% hit rate
- Warm words (1-10 invocations/sec): ~90% hit rate
- Cold words (<1 invocation/sec): Cache miss each time (acceptable)

---

#### Security Property (Formal)

```isabelle
(* Isabelle/HOL theorem: Revocation latency is bounded by TTL *)
theorem acl_revocation_bounded:
  "∀acl vm perm t_revoke.
    acl_revoke acl vm perm t_revoke ⟹
    (∀t. t > t_revoke + ACL_TTL_NS ⟹ ¬acl_check_cached vm acl perm t)"
  by (simp add: ttl_expiration)
```

**Interpretation:**
- ACL revocation at time `t_revoke`
- All cached entries expire by `t_revoke + ACL_TTL_NS`
- Revocation is **guaranteed** effective within 1 second (or whatever TTL is set to)

This is a **provable security property** - perfect for Isabelle/HOL!

---

#### Diagnostics: Cache Hit Rate Monitoring

```forth
\ Forth word to inspect ACL cache statistics
: ACL-CACHE-STATS ( vmid -- )
  vm>acl-cache  ( get VM's ACL cache )
  dup hits @ . ." hits, "
  dup misses @ . ." misses, "
  expires @ . ." expirations" CR

  \ Calculate hit rate
  dup hits @ over misses @ + dup 0> IF
    swap hits @ 100 * swap / ." Hit rate: " . ." %" CR
  ELSE
    2drop ." No cache activity yet" CR
  THEN ;

\ Example output:
\ 1523 hits, 12 misses, 8 expirations
\ Hit rate: 99%
```

---

#### Alternative: Per-Word TTL (Advanced)

For very fine-grained control, store TTL in the word itself:

```c
typedef struct DictEntry {
    char name[32];
    WordFunc func;
    int flags;

    /* ACL */
    ACL *acl;
    uint64_t acl_ttl_ns;  /* Per-word TTL override (0 = use default) */

    /* ... existing fields ... */
} DictEntry;

/* Hot word with long TTL */
DictEntry *hot_word = dict_lookup("+");
hot_word->acl_ttl_ns = 10000000000ULL;  /* 10 seconds */

/* Critical word with short TTL (frequent recheck) */
DictEntry *critical_word = dict_lookup("REBOOT");
critical_word->acl_ttl_ns = 100000000ULL;  /* 100ms */
```

**Use case:** Security-critical words get short TTL, performance-critical words get long TTL.

---

#### Summary: TTL-Based Caching

**Performance:**
- Cache hit: ~5-10 cycles (TTL valid)
- Cache miss: ~50-100 cycles (TTL expired or cold)
- Expected hit rate: >95% (hot words), ~90% (warm words)

**Security:**
- Revocation latency: ≤ TTL_MAX (provable)
- No indefinite access (absolute TTL prevents)
- Tunable per resource type

**Implementation:**
- Simple: Add `expire_time` field to cache entry
- No need to reset TTL on use (absolute)
- Lazy invalidation (let TTL expire naturally)

---

### Security Model Summary

| Security Layer | Granularity | Check Cost | Example |
|----------------|-------------|------------|---------|
| **Capabilities** | Coarse (operation types) | ~5 cycles (bitwise AND) | CAP_SEND_ANY |
| **ACLs** | Fine (specific resources) | ~10 cycles (cached) | Can send to VM[3]? |
| **Combined** | Defense in depth | ~15 cycles total | CAP_SEND_ANY + VM[3] ACL |

**Design Philosophy:**
- **Capabilities:** "What can you do in general?"
- **ACLs:** "Can you access THIS specific thing?"
- **Both required:** Defense in depth, no single point of failure

---

### Example: Fully Secured System

```forth
\ VM[0] (supervisor) sets up secure system

: SETUP-SECURE-SYSTEM
  \ Create worker VM with limited capabilities
  worker-image CAP_RECV CAP_SEND_ANY OR SPAWN-VM
  worker-vmid !

  \ Configure worker's VM ACL: only supervisor can send to it
  worker-vmid @ vm>acl
  VM[0] VM_ACL_SEND acl-grant
  0 acl-set-default

  \ Create privileged word for worker
  : WORKER-REBOOT
    ." Worker requesting reboot..." CR
    VM[0] reboot-request-msg SEND  ( ask supervisor )
  ;

  \ Set word ACL: only worker can execute
  ' WORKER-REBOOT word>acl
  worker-vmid @ ACL_EXEC acl-grant
  0 acl-set-default

  \ Partition block device
  disk0 block-acl>
  worker-vmid @ 0 1023 ACL_READ acl-grant-range
  0 acl-set-default

  ." Secure system initialized" CR ;
```

**Result:**
- Worker can only receive messages from supervisor
- Worker can only execute `WORKER-REBOOT`, not real `REBOOT`
- Worker can only read blocks 0-1023, no writes
- Worker can send messages (has CAP_SEND_ANY) but recipients control who can send to them

---

### ACL Implementation Checklist

1. **Core ACL Data Structures**
   - [ ] `ACL` struct
   - [ ] `ACLEntry` linked list
   - [ ] `acl_create()`, `acl_destroy()`

2. **ACL Operations**
   - [ ] `acl_grant()`, `acl_revoke()`
   - [ ] `acl_check()`
   - [ ] `acl_set_default()`

3. **ACL Integration**
   - [ ] Add ACL to `DictEntry` (words)
   - [ ] Add ACL to `VMInstance` (VMs)
   - [ ] Add ACL to `BlockDevice` (block devices)
   - [ ] Add ACL to `IRQDescriptor` (IRQs)
   - [ ] Add ACL to `SharedRegion` (memory)

4. **ACL Caching**
   - [ ] `ACLCache` per VM
   - [ ] `acl_check_cached()`
   - [ ] Cache invalidation on ACL modification

5. **Kernel Words**
   - [ ] `ACL-CREATE`, `ACL-GRANT`, `ACL-REVOKE`
   - [ ] `ACL-CHECK`, `ACL-SET-DEFAULT`, `ACL-LIST`
   - [ ] `WORD-ACL`, `VM-ACL`, `BLOCK-ACL`, `IRQ-ACL`

6. **Testing**
   - [ ] Word execution with ACL denial
   - [ ] VM message sending with ACL denial
   - [ ] Block I/O with range restrictions
   - [ ] IRQ registration with ACL enforcement
   - [ ] ACL cache hit rate measurement

---

## VM Arbiter Initialization

### `vm_arbiter_init()`

Called once during kernel boot (after HAL init, before VM[0] instantiation).

```c
/* src/platform/kernel/vm_arbiter/scheduler.c */

typedef struct {
    VMInstance *runnable_head;   /* Head of runnable queue */
    VMInstance *runnable_tail;   /* Tail of runnable queue */
    VMInstance *running;         /* Currently executing VM */
    VMInstance *blocked_head;    /* Head of blocked queue */

    vmid_t next_vmid;            /* Next available VM ID */
    VMInstance *vm_table[MAX_VMS]; /* VM lookup table */

    uint64_t quantum_default_ns; /* Default time quantum */
    uint64_t schedule_count;     /* Total schedules performed */
} VMArbiter;

static VMArbiter arbiter;

void vm_arbiter_init(void) {
    memset(&arbiter, 0, sizeof(arbiter));

    arbiter.quantum_default_ns = 10000000;  /* 10ms default */
    arbiter.next_vmid = 1;  /* VM[0] reserved for primordial */

    /* Set up quantum timer (APIC periodic interrupt) */
    hal_timer_periodic(arbiter.quantum_default_ns, vm_arbiter_timer_isr, NULL);
}
```

---

## VM Lifecycle Management

### Creating the Primordial VM

```c
/* src/platform/kernel/vm_arbiter/vm_instance.c */

VMInstance *vm_create_primordial(void) {
    VMInstance *vm = kmalloc(sizeof(VMInstance));
    memset(vm, 0, sizeof(VMInstance));

    vm->vmid = 0;  /* Primordial VM is always ID 0 */
    vm->parent_vmid = 0;  /* No parent */
    vm->state = VM_STATE_RUNNABLE;

    /* Allocate memory envelope (16MB for VM[0]) */
    vm->vm_size = 16 * 1024 * 1024;
    vm->vm_base = 0x10000000;  /* 256MB virtual address */
    vm->envelope_limit = vm->vm_size;

    /* Allocate page table */
    vm->page_table = vmm_create_page_table();

    /* Map VM memory */
    uint64_t paddr = hal_mem_alloc_pages(vm->vm_size / 4096);
    hal_mem_map(vm->vm_base, paddr, vm->vm_size, MEM_READ | MEM_WRITE | MEM_EXEC);

    /* Create embedded StarForth VM */
    vm->forth_vm = vm_create_forth_instance(vm->vm_base, vm->vm_size);

    /* Initialize capabilities (all caps for VM[0]) */
    vm->caps = capability_vector_create();
    vm->caps->caps = CAP_SUPERVISOR;  /* Implicit: all capabilities */

    /* Initialize message queue */
    vm->msg_queue = message_queue_create(MAX_MESSAGE_QUEUE_DEPTH);

    /* Initialize physics state */
    vm->window = rolling_window_create(DEFAULT_WINDOW_SIZE);
    vm->heartbeat = heartbeat_state_create();

    /* Set scheduling parameters */
    vm->quantum_ns = arbiter.quantum_default_ns;
    vm->priority = 0;  /* Highest priority */

    /* Register in VM table */
    arbiter.vm_table[vm->vmid] = vm;

    return vm;
}
```

---

### Spawning Child VMs

```c
VMInstance *vm_spawn(VMInstance *parent, VMImage *image, uint32_t caps) {
    /* Allocate VM instance */
    VMInstance *vm = kmalloc(sizeof(VMInstance));
    memset(vm, 0, sizeof(VMInstance));

    vm->vmid = arbiter.next_vmid++;
    vm->parent_vmid = parent->vmid;
    vm->state = VM_STATE_RUNNABLE;

    /* Allocate memory envelope (smaller for child VMs) */
    vm->vm_size = image->dict_size + (4 * 1024 * 1024);  /* image + 4MB heap */
    vm->envelope_limit = 64 * 1024 * 1024;  /* 64MB max */

    /* Allocate and map memory */
    uint64_t paddr = hal_mem_alloc_pages(vm->vm_size / 4096);
    vm->vm_base = vm_find_free_vaddr_range(vm->vm_size);
    hal_mem_map(vm->vm_base, paddr, vm->vm_size, MEM_READ | MEM_WRITE | MEM_EXEC);

    /* Deserialize VM image into memory */
    vm_deserialize_image(vm, image);

    /* Create embedded StarForth VM */
    vm->forth_vm = vm_create_forth_from_image(vm->vm_base, image);

    /* Set entry point */
    vm->ip = image->entry_point;

    /* Initialize capabilities (attenuated from parent) */
    vm->caps = capability_vector_create();
    vm->caps->caps = caps & parent->caps->caps;  /* Attenuation */

    /* Initialize message queue */
    vm->msg_queue = message_queue_create(MAX_MESSAGE_QUEUE_DEPTH);

    /* Initialize physics state */
    vm->window = rolling_window_create(DEFAULT_WINDOW_SIZE);
    vm->heartbeat = heartbeat_state_create();

    /* Set scheduling parameters */
    vm->quantum_ns = arbiter.quantum_default_ns;
    vm->priority = parent->priority + 1;  /* Lower priority than parent */

    /* Register in VM table */
    arbiter.vm_table[vm->vmid] = vm;

    return vm;
}
```

---

## VM Scheduler

### Runnable Queue Management

```c
/* Add VM to runnable queue (tail) */
void vm_scheduler_enqueue(VMInstance *vm) {
    vm->next = NULL;
    vm->prev = arbiter.runnable_tail;

    if (arbiter.runnable_tail) {
        arbiter.runnable_tail->next = vm;
    } else {
        arbiter.runnable_head = vm;  /* First in queue */
    }

    arbiter.runnable_tail = vm;
    vm->state = VM_STATE_RUNNABLE;
}

/* Remove VM from runnable queue */
void vm_scheduler_dequeue(VMInstance *vm) {
    if (vm->prev) {
        vm->prev->next = vm->next;
    } else {
        arbiter.runnable_head = vm->next;  /* Was head */
    }

    if (vm->next) {
        vm->next->prev = vm->prev;
    } else {
        arbiter.runnable_tail = vm->prev;  /* Was tail */
    }

    vm->next = vm->prev = NULL;
}
```

---

### Scheduling Algorithm (Round-Robin with Priority)

```c
VMInstance *vm_arbiter_next(void) {
    /* Wake blocked VMs if ready */
    vm_arbiter_wake_ready_vms();

    /* Simple round-robin: select head of runnable queue */
    VMInstance *vm = arbiter.runnable_head;

    if (!vm) {
        /* No runnable VMs, idle */
        return NULL;
    }

    /* Remove from runnable queue */
    vm_scheduler_dequeue(vm);

    /* Mark as running */
    vm->state = VM_STATE_RUNNING;
    arbiter.running = vm;
    arbiter.schedule_count++;

    /* Record quantum start time */
    vm->quantum_start = hal_time_now_ns();

    return vm;
}
```

**Future Enhancement:** Priority-based scheduling
```c
/* Priority queues (0 = highest, 3 = lowest) */
#define NUM_PRIORITIES 4
VMInstance *runnable_queues[NUM_PRIORITIES];

VMInstance *vm_arbiter_next_priority(void) {
    for (int prio = 0; prio < NUM_PRIORITIES; prio++) {
        if (runnable_queues[prio]) {
            VMInstance *vm = runnable_queues[prio];
            /* Round-robin within priority */
            runnable_queues[prio] = vm->next;
            return vm;
        }
    }
    return NULL;  /* No runnable VMs */
}
```

---

### Waking Blocked VMs

```c
void vm_arbiter_wake_ready_vms(void) {
    uint64_t now = hal_time_now_ns();

    VMInstance *vm = arbiter.blocked_head;
    while (vm) {
        VMInstance *next = vm->next;

        if (vm->state == VM_STATE_BLOCKED_SLEEP && now >= vm->wake_time) {
            /* Sleep expired, wake VM */
            vm_scheduler_dequeue(vm);  /* Remove from blocked queue */
            vm_scheduler_enqueue(vm);  /* Add to runnable queue */
        }

        vm = next;
    }
}
```

---

## VM Transition (Context Switch)

### `vm_transition()`

Switches CPU from current VM to target VM.

```c
/* src/platform/kernel/vm_arbiter/scheduler.c */

void vm_transition(VMInstance *to) {
    VMInstance *from = arbiter.running;

    /* Save state of current VM (if any) */
    if (from) {
        vm_save_state(from);

        /* Update runtime accounting */
        uint64_t elapsed = hal_time_now_ns() - from->quantum_start;
        from->total_runtime += elapsed;
    }

    /* Load state of target VM */
    vm_load_state(to);

    /* Switch page tables */
    vmm_switch_page_table(to->page_table);

    /* Mark new VM as running */
    arbiter.running = to;
    to->state = VM_STATE_RUNNING;

    /* Jump to VM execution */
    vm_execute(to);
}
```

---

### `vm_save_state()` and `vm_load_state()`

```c
void vm_save_state(VMInstance *vm) {
    /* Save StarForth VM state */
    vm->ip = vm->forth_vm->ip;
    vm->sp = vm->forth_vm->sp;
    vm->rp = vm->forth_vm->rp;

    /* Save CPU registers (if using them) */
    /* ... architecture-specific register save ... */
}

void vm_load_state(VMInstance *vm) {
    /* Restore StarForth VM state */
    vm->forth_vm->ip = vm->ip;
    vm->forth_vm->sp = vm->sp;
    vm->forth_vm->rp = vm->rp;

    /* Restore CPU registers */
    /* ... architecture-specific register restore ... */
}
```

---

### `vm_execute()`

Executes the VM until preemption.

```c
void vm_execute(VMInstance *vm) {
    /* Clear yield flag */
    vm->yield_requested = 0;

    /* Execute StarForth VM inner loop */
    while (1) {
        /* Check for preemption */
        if (vm->yield_requested) {
            /* YIELD invoked, reschedule */
            break;
        }

        if (vm->state != VM_STATE_RUNNING) {
            /* VM blocked or killed, reschedule */
            break;
        }

        /* Execute one Forth word */
        forth_step(vm->forth_vm);

        /* Note: Quantum expiration is handled by timer ISR */
    }

    /* VM yielded or blocked, return to arbiter loop */
}
```

---

## VM Arbiter Loop

```c
void vm_arbiter_loop(void) {
    while (1) {
        /* Select next VM to run */
        VMInstance *vm = vm_arbiter_next();

        if (!vm) {
            /* No runnable VMs, idle */
            hal_cpu_halt();  /* Wait for interrupt */
            continue;
        }

        /* Transition to VM */
        vm_transition(vm);

        /* VM yielded/blocked, loop continues */
    }
}
```

---

## Quantum Timer ISR

```c
/* Called by HAL when APIC timer fires (every quantum_default_ns) */
void vm_arbiter_timer_isr(void *ctx) {
    (void)ctx;

    VMInstance *vm = arbiter.running;
    if (!vm) return;  /* No VM running (idle) */

    /* Check if quantum expired */
    uint64_t elapsed = hal_time_now_ns() - vm->quantum_start;
    if (elapsed >= vm->quantum_ns) {
        /* Quantum expired, preempt VM */
        vm->state = VM_STATE_RUNNABLE;
        vm->yield_requested = 1;  /* Signal VM to stop */

        /* Re-enqueue VM */
        vm_scheduler_enqueue(vm);

        /* Note: Actual context switch happens when vm_execute() returns */
    }
}
```

---

## Message Queue Implementation

### MessageQueue Structure

```c
/* src/platform/kernel/vm_arbiter/message_queue.c */

#define MAX_MESSAGE_QUEUE_DEPTH 64

typedef struct {
    Message *messages[MAX_MESSAGE_QUEUE_DEPTH];
    int head;  /* Index of next message to dequeue */
    int tail;  /* Index where next message will be enqueued */
    int count; /* Number of messages in queue */
} MessageQueue;

MessageQueue *message_queue_create(int max_depth) {
    MessageQueue *q = kmalloc(sizeof(MessageQueue));
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    return q;
}
```

---

### Enqueue / Dequeue

```c
int message_queue_enqueue(VMInstance *vm, Message *msg) {
    MessageQueue *q = vm->msg_queue;

    if (q->count >= MAX_MESSAGE_QUEUE_DEPTH) {
        return 0;  /* Queue full */
    }

    /* Copy message into queue */
    q->messages[q->tail] = kmalloc(sizeof(Message));
    memcpy(q->messages[q->tail], msg, sizeof(Message));

    q->tail = (q->tail + 1) % MAX_MESSAGE_QUEUE_DEPTH;
    q->count++;

    return 1;
}

Message *message_queue_dequeue(VMInstance *vm) {
    MessageQueue *q = vm->msg_queue;

    if (q->count == 0) {
        return NULL;  /* Queue empty */
    }

    Message *msg = q->messages[q->head];
    q->head = (q->head + 1) % MAX_MESSAGE_QUEUE_DEPTH;
    q->count--;

    return msg;  /* Caller must free */
}
```

---

## Interrupt Handling in VM Context

### Interrupt Dispatch

When a hardware interrupt fires (e.g., UART, timer, disk):

```c
/* Called from IDT stub (src/platform/kernel/cpu/isr.S) */
void common_isr_handler(uint64_t vector) {
    VMInstance *vm = arbiter.running;

    /* Check if VM registered a handler for this IRQ */
    if (irq_table[vector].vm == vm) {
        /* Call VM's ISR handler */
        vm_call_isr_handler(vm, vector);
    } else {
        /* Kernel-level handler (e.g., quantum timer) */
        kernel_isr_dispatch(vector);
    }

    /* Send EOI to APIC */
    apic_eoi();
}
```

---

### VM ISR Invocation

```c
void vm_call_isr_handler(VMInstance *vm, uint64_t vector) {
    vaddr_t handler_addr = irq_table[vector].handler_addr;

    /* Save current IP (will resume here after ISR) */
    vm->saved_ip = vm->ip;

    /* Jump to VM's ISR handler */
    vm->ip = handler_addr;

    /* Execute ISR (must be fast, non-blocking) */
    forth_execute_word(vm->forth_vm, handler_addr);

    /* Restore IP */
    vm->ip = vm->saved_ip;
}
```

**Critical Constraint:** VM ISR handlers MUST be fast (< 10µs) and MUST NOT invoke blocking kernel words (RECV, SLEEP, etc.).

---

## VM State Machine

```
                    SPAWN-VM
       DEAD  <──────────────────┐
         │                      │
         │                      │
         ↓                      │
    RUNNABLE ←──────────────────┤
         │            ↑         │
         │  vm_arbiter_next()   │
         │            │         │
         ↓            │         │
      RUNNING ────────┘         │
         │                      │
         ├──────→ BLOCKED_RECV  │
         │                      │
         ├──────→ BLOCKED_SLEEP │
         │                      │
         └──────→ DEAD ─────────┘
              (KILL-VM or fault)
```

**State Transitions:**
- `RUNNABLE → RUNNING`: arbiter selects VM
- `RUNNING → RUNNABLE`: quantum expires or YIELD
- `RUNNING → BLOCKED_RECV`: RECV with empty queue
- `RUNNING → BLOCKED_SLEEP`: SLEEP invoked
- `BLOCKED_RECV → RUNNABLE`: message arrives
- `BLOCKED_SLEEP → RUNNABLE`: wake_time reached
- `ANY → DEAD`: KILL-VM or unhandled fault

---

## VM Destruction and Cleanup

```c
void vm_destroy(VMInstance *vm) {
    /* Remove from all queues */
    if (vm->state == VM_STATE_RUNNABLE) {
        vm_scheduler_dequeue(vm);
    }

    /* Free message queue */
    message_queue_destroy(vm->msg_queue);

    /* Free physics state */
    rolling_window_destroy(vm->window);
    heartbeat_state_destroy(vm->heartbeat);

    /* Free capability vector */
    capability_vector_destroy(vm->caps);

    /* Unmap and free memory */
    hal_mem_free_pages(vm_vaddr_to_paddr(vm, vm->vm_base), vm->vm_size / 4096);

    /* Free page table */
    vmm_destroy_page_table(vm->page_table);

    /* Free StarForth VM */
    vm_destroy_forth_instance(vm->forth_vm);

    /* Remove from VM table */
    arbiter.vm_table[vm->vmid] = NULL;

    /* Free VM instance */
    kfree(vm);
}
```

---

## Performance Metrics

### VM Accounting

Each VM tracks execution metrics:

```c
typedef struct {
    uint64_t total_runtime;      /* Total CPU time (ns) */
    uint64_t schedule_count;     /* Times scheduled */
    uint64_t yield_count;        /* Times yielded */
    uint64_t block_count;        /* Times blocked */
    uint64_t msg_sent;           /* Messages sent */
    uint64_t msg_recv;           /* Messages received */
    uint64_t faults;             /* Faults encountered */
} VMMetrics;
```

Exposed via kernel word:
```forth
: VM-METRICS ( vmid -- metrics-addr )
  \ Returns pointer to VMMetrics struct
;
```

---

## Dictionary Hydration

### Core Words + Kernel Words

VM[0] dictionary includes:

1. **Core Forth words** (arithmetic, stack, control flow, etc.)
   - Already implemented in `src/word_source/`

2. **Kernel words** (SPAWN-VM, SEND, RECV, etc.)
   - New implementations in `src/platform/kernel/vm_arbiter/kernel_words.c`

```c
void vm_hydrate_dictionary(VMInstance *vm) {
    VM *forth_vm = vm->forth_vm;

    /* Load core Forth words (existing) */
    init_arithmetic_words(forth_vm);
    init_stack_words(forth_vm);
    init_control_words(forth_vm);
    /* ... etc ... */

    /* Load kernel words (new) */
    init_kernel_words(forth_vm);
}

void init_kernel_words(VM *vm) {
    /* Time & scheduling */
    vm_define_word(vm, "TICKS", word_ticks, 0);
    vm_define_word(vm, "YIELD", word_yield, 0);
    vm_define_word(vm, "SLEEP", word_sleep, 0);

    /* VM lifecycle */
    vm_define_word(vm, "SPAWN-VM", word_spawn_vm, 0);
    vm_define_word(vm, "KILL-VM", word_kill_vm, 0);
    vm_define_word(vm, "VM-STATUS", word_vm_status, 0);

    /* Communication */
    vm_define_word(vm, "SEND", word_send, 0);
    vm_define_word(vm, "RECV", word_recv, 0);
    vm_define_word(vm, "RECV?", word_recv_nonblock, 0);

    /* Memory */
    vm_define_word(vm, "MAP-MEM", word_map_mem, 0);
    vm_define_word(vm, "UNMAP-MEM", word_unmap_mem, 0);
    vm_define_word(vm, "SHARE-MEM", word_share_mem, 0);

    /* Hardware */
    vm_define_word(vm, "MMIO-READ", word_mmio_read, 0);
    vm_define_word(vm, "MMIO-WRITE", word_mmio_write, 0);
    vm_define_word(vm, "IRQ-REGISTER", word_irq_register, 0);

    /* Capabilities */
    vm_define_word(vm, "HAS-CAP?", word_has_cap, 0);
    vm_define_word(vm, "GRANT-CAP", word_grant_cap, 0);
    vm_define_word(vm, "REVOKE-CAP", word_revoke_cap, 0);
}
```

---

## Example: VM[0] Spawning a Worker VM

```forth
\ VM[0] code (supervisor)

: WORKER-MAIN
  BEGIN
    RECV  ( -- msg )
    dup process-work
    free-message
  AGAIN ;

: CREATE-WORKER
  \ Load worker image from block storage
  worker-image-addr

  \ Grant worker send/recv capabilities
  CAP_RECV CAP_SEND_ANY OR

  \ Spawn worker VM
  SPAWN-VM  ( -- worker-vmid )

  \ Store worker ID
  worker-vmid ! ;

CREATE-WORKER

\ Send work to worker
worker-vmid @ work-msg SEND
```

---

## Critical Implementation Notes

### 1. **Reentrancy in Kernel Words**

Kernel words may be interrupted. Use spinlocks for critical sections:

```c
static spinlock_t vm_table_lock = SPINLOCK_INIT;

void word_spawn_vm(VM *vm) {
    /* ... */

    spinlock_acquire(&vm_table_lock);
    arbiter.vm_table[new_vm->vmid] = new_vm;
    spinlock_release(&vm_table_lock);

    /* ... */
}
```

### 2. **VM Transition Must Be Atomic**

`vm_transition()` must disable interrupts during state save/restore:

```c
void vm_transition(VMInstance *to) {
    unsigned long flags = hal_irq_disable();

    /* Save/load state */
    vm_save_state(arbiter.running);
    vm_load_state(to);
    vmm_switch_page_table(to->page_table);

    hal_irq_restore(flags);

    /* Now safe to execute */
    vm_execute(to);
}
```

### 3. **Message Queue Overflow**

If a VM's message queue fills, sender faults. Supervisor (VM[0]) must handle this:

```forth
: HANDLE-FAULT  ( fault info -- )
  fault FAULT_QUEUE_FULL = IF
    ." Message queue full, dropping message" CR
  THEN ;
```

---

## Next Steps (Implementation Order)

1. **Implement VM control block** (`vm_instance.c`)
2. **Implement scheduler** (`scheduler.c`)
3. **Implement message queue** (`message_queue.c`)
4. **Implement capability system** (`capabilities.c`)
5. **Implement kernel words** (`kernel_words.c`)
6. **Test single-VM execution** (VM[0] boots to `ok`)
7. **Test multi-VM spawning** (VM[0] spawns VM[1])
8. **Test message passing** (VM[0] ↔ VM[1] communication)
9. **Test IRQ handling** (VM registers UART IRQ)
10. **Validate determinism** (physics subsystems per-VM)

---

## References

- **Scheduling Algorithms:** Tanenbaum, Modern Operating Systems, Ch. 2
- **Context Switching:** Intel SDM Vol. 3A, Ch. 7 (Task Management)
- **Message Passing:** Erlang OTP Design Principles
- **Capability Systems:** Levy, "Capability-Based Computer Systems"
- **Forth Threading:** Brad Rodriguez, "Moving Forth" series

---

*Document Version: 1.0*
*Date: 2025-12-17*
*Status: Complete design, ready for implementation*
