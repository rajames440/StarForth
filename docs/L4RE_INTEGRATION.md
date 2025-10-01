# StarForth L4Re Integration Guide

## Overview

This guide covers integrating StarForth into the L4Re Operating System Framework, including:

- Building as an L4Re package
- Memory management with L4Re dataspaces
- IPC communication between VMs
- Integration with StarshipOS
- Kernel-level integration (if needed)

## Table of Contents

1. [L4Re Basics](#l4re-basics)
2. [Package Structure](#package-structure)
3. [Build System Integration](#build-system-integration)
4. [Memory Management](#memory-management)
5. [IPC Communication](#ipc-communication)
6. [Multi-VM Architecture](#multi-vm-architecture)
7. [Kernel Integration](#kernel-integration)
8. [StarshipOS Specifics](#starshipos-specifics)

## L4Re Basics

### What is L4Re?

L4Re (L4 Runtime Environment) is a user-level infrastructure for building systems on top of the L4 microkernel (
Fiasco.OC). It provides:

- Memory management (dataspaces)
- Task/thread management
- Inter-Process Communication (IPC)
- Device drivers
- Runtime libraries

### Key Concepts

| Concept            | Description                                            |
|--------------------|--------------------------------------------------------|
| **Task**           | Address space + threads                                |
| **Dataspace**      | Memory object (like file or anonymous memory)          |
| **Capability**     | Reference to kernel object (task, dataspace, IPC gate) |
| **IPC Gate**       | Communication endpoint                                 |
| **Region Manager** | Virtual memory management                              |
| **Name Server**    | Service discovery                                      |

## Package Structure

### Directory Layout

Create this structure in your L4Re source tree:

```
l4/pkg/starforth/
├── Control                 # Package metadata
├── Makefile               # Top-level build
├── server/                # Main StarForth server
│   ├── Makefile
│   └── src/
│       ├── main.cc        # L4Re entry point
│       ├── l4_vm.cc       # L4Re-specific VM wrapper
│       └── l4_vm.h
├── lib/                   # StarForth as library
│   ├── Makefile
│   └── src/              # Your existing src/ files
├── include/              # Your existing include/ files
└── examples/             # Example clients
    ├── Makefile
    └── forth_client.cc
```

### Control File

Create `l4/pkg/starforth/Control`:

```
provides: starforth
requires: libc libstdc++ l4re-core
maintainer: rajames
description: StarForth VM for L4Re
license: CC0
```

## Build System Integration

### Top-Level Makefile

Create `l4/pkg/starforth/Makefile`:

```makefile
PKGDIR  ?= .
L4DIR   ?= $(PKGDIR)/../..

# Subdirectories to build
TARGET  = lib server examples

include $(L4DIR)/mk/subdir.mk
```

### Library Makefile

Create `l4/pkg/starforth/lib/Makefile`:

```makefile
PKGDIR  ?= ..
L4DIR   ?= $(PKGDIR)/../..

TARGET          = libstarforth.a libstarforth.so
PC_FILENAME     = libstarforth

# Architecture detection
ifeq ($(ARCH),amd64)
  ARCH_FLAGS    = -march=x86-64-v2
  ARCH_DEFINES  = -DARCH_X86_64=1
else ifeq ($(ARCH),arm64)
  ARCH_FLAGS    = -march=armv8-a+crc+simd
  ARCH_DEFINES  = -DARCH_ARM64=1
endif

# Compiler flags
CFLAGS          = -std=c99 -O3 $(ARCH_FLAGS) $(ARCH_DEFINES) \
                  -DUSE_ASM_OPT=1 -DUSE_DIRECT_THREADING=1 \
                  -DL4RE_BUILD=1
CXXFLAGS        = -std=c++17 -O3 $(ARCH_FLAGS)

# Include paths
PRIVATE_INCDIR  = $(PKGDIR)/include $(PKGDIR)/lib/src

# Source files (adapt to your structure)
SRC_C           = vm.c stack_management.c memory_management.c \
                  dictionary_management.c io.c log.c profiler.c \
                  repl.c vm_api.c vm_debug.c word_registry.c \
                  word_source/arithmetic_words.c \
                  word_source/stack_words.c \
                  word_source/logical_words.c \
                  word_source/memory_words.c \
                  word_source/control_words.c \
                  word_source/defining_words.c \
                  word_source/dictionary_words.c \
                  word_source/double_words.c \
                  word_source/mixed_arithmetic_words.c \
                  word_source/return_stack_words.c \
                  word_source/string_words.c \
                  word_source/system_words.c \
                  word_source/vocabulary_words.c \
                  word_source/io_words.c \
                  word_source/format_words.c \
                  word_source/block_words.c \
                  word_source/editor_words.c \
                  word_source/starforth_words.c

# L4Re-specific wrapper
SRC_CC          = l4_vm.cc

# Dependencies
REQUIRES_LIBS   = libc libstdc++

include $(L4DIR)/mk/lib.mk
```

### Server Makefile

Create `l4/pkg/starforth/server/Makefile`:

```makefile
PKGDIR  ?= ..
L4DIR   ?= $(PKGDIR)/../..

TARGET          = starforth_server
MODE            = static

# Architecture detection
ifeq ($(ARCH),amd64)
  ARCH_FLAGS    = -march=x86-64-v2
  ARCH_DEFINES  = -DARCH_X86_64=1
else ifeq ($(ARCH),arm64)
  ARCH_FLAGS    = -march=armv8-a+crc+simd
  ARCH_DEFINES  = -DARCH_ARM64=1
endif

CXXFLAGS        = -std=c++17 -O3 $(ARCH_FLAGS) $(ARCH_DEFINES)

PRIVATE_INCDIR  = $(PKGDIR)/include

SRC_CC          = main.cc l4_vm.cc

REQUIRES_LIBS   = libstarforth l4re_c l4re_c-util libstdc++ libc

include $(L4DIR)/mk/prog.mk
```

## Memory Management

### L4Re Dataspace Integration

Create `l4/pkg/starforth/server/src/l4_vm.h`:

```cpp
#pragma once

#include <l4/re/c/dataspace.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>

extern "C" {
#include "vm.h"
}

namespace StarForth {

/**
 * L4Re-specific VM wrapper
 *
 * Uses L4Re dataspaces for memory management instead of malloc()
 */
class L4VM {
public:
    L4VM();
    ~L4VM();

    // Initialize VM with L4Re dataspaces
    bool init(size_t memory_size = VM_MEMORY_SIZE);

    // Get underlying VM structure
    VM* get_vm() { return &vm_; }

    // Cleanup
    void cleanup();

    // Memory management
    void* map_dataspace(l4_cap_idx_t ds, size_t size);
    bool unmap_dataspace(void* addr, size_t size);

private:
    VM vm_;
    l4_cap_idx_t memory_ds_;    // Dataspace for VM memory
    void* memory_addr_;         // Mapped address
    size_t memory_size_;
    bool initialized_;
};

} // namespace StarForth
```

Create `l4/pkg/starforth/server/src/l4_vm.cc`:

```cpp
#include "l4_vm.h"
#include <l4/re/env>
#include <l4/sys/err.h>
#include <cstdio>
#include <cstring>

extern "C" {
#include "log.h"
}

namespace StarForth {

L4VM::L4VM()
    : memory_ds_(L4_INVALID_CAP),
      memory_addr_(nullptr),
      memory_size_(0),
      initialized_(false)
{
    memset(&vm_, 0, sizeof(vm_));
}

L4VM::~L4VM() {
    cleanup();
}

bool L4VM::init(size_t memory_size) {
    if (initialized_) {
        return false;
    }

    memory_size_ = memory_size;

    // Allocate capability slot
    memory_ds_ = l4re_util_cap_alloc();
    if (l4_is_invalid_cap(memory_ds_)) {
        log_message(LOG_ERROR, "L4VM: Failed to allocate capability");
        return false;
    }

    // Allocate dataspace
    long ret = l4re_ma_alloc(memory_size_, memory_ds_, 0);
    if (ret) {
        log_message(LOG_ERROR, "L4VM: Failed to allocate dataspace: %ld", ret);
        l4re_util_cap_free(memory_ds_);
        return false;
    }

    // Map dataspace into our address space
    ret = l4re_rm_attach((void**)&memory_addr_, memory_size_,
                         L4RE_RM_F_SEARCH_ADDR | L4RE_RM_F_RW,
                         memory_ds_, 0, L4_PAGESHIFT);
    if (ret) {
        log_message(LOG_ERROR, "L4VM: Failed to map dataspace: %ld", ret);
        l4re_util_cap_free(memory_ds_);
        return false;
    }

    log_message(LOG_INFO, "L4VM: Allocated %zu bytes at %p",
                memory_size_, memory_addr_);

    // Initialize VM structure
    vm_.memory = static_cast<uint8_t*>(memory_addr_);
    vm_.dsp = -1;
    vm_.rsp = -1;
    vm_.here = 0;
    vm_.exit_colon = 0;
    vm_.error = 0;
    vm_.halted = 0;

    // Initialize VM subsystems (from your vm_init function)
    vm_align(&vm_);

    // Allocate SCR
    void *p = vm_allot(&vm_, sizeof(cell_t));
    if (!p) {
        log_message(LOG_ERROR, "L4VM: SCR allot failed");
        cleanup();
        return false;
    }
    vm_.scr_addr = (vaddr_t)((uint8_t*)p - vm_.memory);
    vm_store_cell(&vm_, vm_.scr_addr, 0);

    // Allocate STATE
    p = vm_allot(&vm_, sizeof(cell_t));
    if (!p) {
        log_message(LOG_ERROR, "L4VM: STATE allot failed");
        cleanup();
        return false;
    }
    vm_.state_addr = (vaddr_t)((uint8_t*)p - vm_.memory);
    vm_store_cell(&vm_, vm_.state_addr, 0);
    vm_.state_var = 0;

    // Allocate BASE
    p = vm_allot(&vm_, sizeof(cell_t));
    if (!p) {
        log_message(LOG_ERROR, "L4VM: BASE allot failed");
        cleanup();
        return false;
    }
    vm_.base_addr = (vaddr_t)((uint8_t*)p - vm_.memory);
    vm_store_cell(&vm_, vm_.base_addr, 10);
    vm_.base = 10;

    initialized_ = true;
    log_message(LOG_INFO, "L4VM: Initialization complete");
    return true;
}

void L4VM::cleanup() {
    if (memory_addr_) {
        l4re_rm_detach(memory_addr_);
        memory_addr_ = nullptr;
    }

    if (l4_is_valid_cap(memory_ds_)) {
        // Note: dataspace will be freed when capability is released
        l4re_util_cap_free(memory_ds_);
        memory_ds_ = L4_INVALID_CAP;
    }

    initialized_ = false;
}

void* L4VM::map_dataspace(l4_cap_idx_t ds, size_t size) {
    void* addr = nullptr;
    long ret = l4re_rm_attach(&addr, size,
                              L4RE_RM_F_SEARCH_ADDR | L4RE_RM_F_RW,
                              ds, 0, L4_PAGESHIFT);
    if (ret) {
        log_message(LOG_ERROR, "L4VM: Failed to map dataspace: %ld", ret);
        return nullptr;
    }
    return addr;
}

bool L4VM::unmap_dataspace(void* addr, size_t size) {
    (void)size; // L4Re tracks size internally
    long ret = l4re_rm_detach(addr);
    if (ret) {
        log_message(LOG_ERROR, "L4VM: Failed to unmap dataspace: %ld", ret);
        return false;
    }
    return true;
}

} // namespace StarForth
```

## IPC Communication

### Server Main Entry Point

Create `l4/pkg/starforth/server/src/main.cc`:

```cpp
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/cxx/ipc_server>
#include <l4/sys/cxx/ipc_epiface>
#include <cstdio>

#include "l4_vm.h"

extern "C" {
#include "vm.h"
#include "log.h"
#include "word_registry.h"
}

// StarForth IPC protocol opcodes
enum {
    OP_INTERPRET = 0,
    OP_PUSH      = 1,
    OP_POP       = 2,
    OP_GET_STATE = 3,
    OP_RESET     = 4,
};

/**
 * StarForth server implementing IPC interface
 */
class StarForth_server : public L4::Epiface_t<StarForth_server, L4::Kobject>
{
public:
    StarForth_server() {
        if (!l4vm_.init()) {
            printf("Failed to initialize L4VM\n");
            return;
        }

        // Register standard Forth words
        VM* vm = l4vm_.get_vm();
        register_stack_words(vm);
        register_arithmetic_words(vm);
        register_logical_words(vm);
        register_memory_words(vm);
        register_control_words(vm);
        // ... register other word sets ...

        printf("StarForth server initialized\n");
    }

    // IPC dispatch - handle incoming messages
    long op_dispatch(l4_umword_t obj, L4::Ipc::Iostream &ios) {
        l4_msgtag_t tag;
        ios >> tag;

        if (tag.label() == 0) {
            // Handle our custom protocol
            l4_umword_t opcode;
            ios >> opcode;

            switch (opcode) {
                case OP_INTERPRET:
                    return handle_interpret(ios);
                case OP_PUSH:
                    return handle_push(ios);
                case OP_POP:
                    return handle_pop(ios);
                case OP_GET_STATE:
                    return handle_get_state(ios);
                case OP_RESET:
                    return handle_reset(ios);
                default:
                    return -L4_EINVAL;
            }
        }

        return -L4_ENOSYS;
    }

private:
    long handle_interpret(L4::Ipc::Iostream &ios) {
        char buffer[256];
        unsigned long len;

        ios >> L4::Ipc::buf_cp_in(buffer, sizeof(buffer), len);
        buffer[len < sizeof(buffer) ? len : sizeof(buffer)-1] = '\0';

        VM* vm = l4vm_.get_vm();
        vm_interpret(vm, buffer);

        // Send reply with error status
        ios << L4::Ipc::Small_buf(&vm->error, sizeof(vm->error));
        return L4_EOK;
    }

    long handle_push(L4::Ipc::Iostream &ios) {
        cell_t value;
        ios >> value;

        VM* vm = l4vm_.get_vm();
        vm_push(vm, value);

        ios << L4::Ipc::Small_buf(&vm->error, sizeof(vm->error));
        return L4_EOK;
    }

    long handle_pop(L4::Ipc::Iostream &ios) {
        VM* vm = l4vm_.get_vm();
        cell_t value = vm_pop(vm);

        ios << value;
        ios << L4::Ipc::Small_buf(&vm->error, sizeof(vm->error));
        return L4_EOK;
    }

    long handle_get_state(L4::Ipc::Iostream &ios) {
        VM* vm = l4vm_.get_vm();

        struct {
            int dsp;
            int rsp;
            int error;
            int halted;
        } state;

        state.dsp = vm->dsp;
        state.rsp = vm->rsp;
        state.error = vm->error;
        state.halted = vm->halted;

        ios << L4::Ipc::buf_cp_out(L4::Ipc::Small_buf(&state, sizeof(state)));
        return L4_EOK;
    }

    long handle_reset(L4::Ipc::Iostream &ios) {
        l4vm_.cleanup();
        if (!l4vm_.init()) {
            int error = 1;
            ios << error;
            return -L4_ENOMEM;
        }

        int success = 0;
        ios << success;
        return L4_EOK;
    }

    StarForth::L4VM l4vm_;
};

int main() {
    printf("StarForth L4Re Server starting...\n");

    // Create server object
    static StarForth_server server;

    // Register with name server
    L4Re::Env const *env = L4Re::Env::env();

    // Get capability to registry
    L4::Cap<void> registry = env->get_cap<void>("starforth");
    if (!registry.is_valid()) {
        printf("Failed to get starforth capability\n");
        return 1;
    }

    // Create object registry for IPC
    static L4Re::Util::Registry_server<> registry_server(
        L4::cap_reinterpret_cast<L4::Thread>(env->main_thread()),
        env->factory());

    // Register server
    if (!registry_server.registry()->register_obj(&server, "starforth").is_valid()) {
        printf("Failed to register server object\n");
        return 1;
    }

    printf("StarForth server ready\n");

    // Enter server loop
    registry_server.loop();

    return 0;
}
```

### Client Example

Create `l4/pkg/starforth/examples/forth_client.cc`:

```cpp
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/ipc.h>
#include <l4/sys/types.h>
#include <cstdio>

extern "C" {
#include "vm.h"
}

// Match server opcodes
enum {
    OP_INTERPRET = 0,
    OP_PUSH      = 1,
    OP_POP       = 2,
    OP_GET_STATE = 3,
    OP_RESET     = 4,
};

class StarForthClient {
public:
    StarForthClient() : server_cap_(L4_INVALID_CAP) {
        // Get capability to StarForth server
        L4Re::Env const *env = L4Re::Env::env();
        server_cap_ = env->get_cap<void>("starforth");

        if (!server_cap_.is_valid()) {
            printf("Failed to get starforth server capability\n");
        }
    }

    bool interpret(const char* code) {
        if (!server_cap_.is_valid()) return false;

        l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
        l4_msg_regs_t *mr = l4_utcb_mr();

        // Send opcode
        mr->mr[0] = OP_INTERPRET;

        // Send code string
        size_t len = strlen(code);
        memcpy(&mr->mr[1], code, len);

        tag = l4_msgtag(0, 1 + (len + sizeof(l4_umword_t) - 1) / sizeof(l4_umword_t),
                        0, 0);
        tag = l4_ipc_call(server_cap_.cap(), l4_utcb(), tag, L4_IPC_NEVER);

        if (l4_ipc_error(tag, l4_utcb())) {
            printf("IPC error: %ld\n", l4_ipc_error(tag, l4_utcb()));
            return false;
        }

        // Get error status from reply
        int error;
        memcpy(&error, &mr->mr[0], sizeof(error));
        return error == 0;
    }

    bool push(cell_t value) {
        if (!server_cap_.is_valid()) return false;

        l4_msgtag_t tag = l4_msgtag(0, 2, 0, 0);
        l4_msg_regs_t *mr = l4_utcb_mr();

        mr->mr[0] = OP_PUSH;
        memcpy(&mr->mr[1], &value, sizeof(value));

        tag = l4_ipc_call(server_cap_.cap(), l4_utcb(), tag, L4_IPC_NEVER);

        if (l4_ipc_error(tag, l4_utcb())) {
            return false;
        }

        int error;
        memcpy(&error, &mr->mr[0], sizeof(error));
        return error == 0;
    }

    cell_t pop(bool* success = nullptr) {
        if (!server_cap_.is_valid()) {
            if (success) *success = false;
            return 0;
        }

        l4_msgtag_t tag = l4_msgtag(0, 1, 0, 0);
        l4_msg_regs_t *mr = l4_utcb_mr();

        mr->mr[0] = OP_POP;

        tag = l4_ipc_call(server_cap_.cap(), l4_utcb(), tag, L4_IPC_NEVER);

        if (l4_ipc_error(tag, l4_utcb())) {
            if (success) *success = false;
            return 0;
        }

        cell_t value;
        memcpy(&value, &mr->mr[0], sizeof(value));

        int error;
        memcpy(&error, &mr->mr[1], sizeof(error));

        if (success) *success = (error == 0);
        return value;
    }

private:
    L4::Cap<void> server_cap_;
};

int main() {
    printf("StarForth Client Example\n");

    StarForthClient client;

    // Test pushing and popping
    printf("Pushing 42...\n");
    if (client.push(42)) {
        printf("Success\n");
    }

    printf("Pushing 17...\n");
    client.push(17);

    // Pop and print
    bool success;
    cell_t val = client.pop(&success);
    if (success) {
        printf("Popped: %ld\n", val);
    }

    val = client.pop(&success);
    if (success) {
        printf("Popped: %ld\n", val);
    }

    // Interpret some Forth code
    printf("\nInterpreting: 10 20 + .\n");
    if (client.interpret("10 20 + .")) {
        printf("Interpretation successful\n");
    }

    printf("\nInterpreting: : SQUARE DUP * ;\n");
    client.interpret(": SQUARE DUP * ;");

    printf("Interpreting: 5 SQUARE .\n");
    client.interpret("5 SQUARE .");

    return 0;
}
```

## Multi-VM Architecture

### Design Pattern: Multiple Forth VMs

```
┌─────────────────┐
│   Ned (Root)    │
│   - Name Server │
│   - VM Manager  │
└────────┬────────┘
         │
    ┌────┴─────┬─────────────┐
    │          │             │
┌───▼────┐ ┌──▼─────┐ ┌─────▼────┐
│  VM 1  │ │  VM 2  │ │   VM 3   │
│ Forth  │ │ Forth  │ │  Forth   │
│ Tasks  │ │ Server │ │  REPL    │
└────────┘ └────────┘ └──────────┘
```

### VM Manager (Ned Configuration)

Create `l4/conf/modules.list` entry:

```lua
-- StarForth VMs
entry {
  name = "starforth-server",
  cmdline = "rom/starforth_server",
  caps = {
    starforth = L4.default_loader:new_channel(),
  },
}

entry {
  name = "starforth-client",
  cmdline = "rom/forth_client",
  caps = {
    starforth = L4.Env.starforth_server:svr(),
  },
}
```

## Kernel Integration

### For StarshipOS: Forth in Kernel Context

⚠️ **Warning**: Running Forth in kernel requires extreme care!

#### Use Cases

1. **Kernel configuration** - Runtime kernel tunables
2. **Device driver scripting** - Hotpatch device drivers
3. **Debugging** - Interactive kernel debugging

#### Limitations

- No malloc/free (use static memory pools)
- No syscalls
- Limited stack space
- Must be interrupt-safe
- Must not block

#### Example: Kernel Module

Create `l4/pkg/starforth/kernel/starforth_kernel.cc`:

```cpp
#include <l4/sys/kernel_object.h>
#include <l4/sys/kip.h>

extern "C" {
#include "vm.h"
}

// Static memory pool for kernel VM
static uint8_t kernel_vm_memory[256 * 1024] __attribute__((aligned(4096)));
static VM kernel_vm;
static bool kernel_vm_initialized = false;

/**
 * Initialize kernel-mode Forth VM
 *
 * Called during kernel initialization
 */
extern "C" void starforth_kernel_init(void) {
    if (kernel_vm_initialized) return;

    // Initialize VM with static memory
    kernel_vm.memory = kernel_vm_memory;
    kernel_vm.dsp = -1;
    kernel_vm.rsp = -1;
    kernel_vm.here = 0;
    kernel_vm.error = 0;
    kernel_vm.halted = 0;

    // Register minimal word set (safe for kernel)
    // NO I/O words, NO blocking operations
    register_arithmetic_words(&kernel_vm);
    register_logical_words(&kernel_vm);
    register_stack_words(&kernel_vm);

    // Add kernel-specific words
    // e.g., words to read/write kernel data structures

    kernel_vm_initialized = true;
}

/**
 * Execute Forth code in kernel context
 *
 * DANGEROUS: Only call from trusted sources!
 */
extern "C" int starforth_kernel_exec(const char* code) {
    if (!kernel_vm_initialized) return -1;

    vm_interpret(&kernel_vm, code);

    return kernel_vm.error;
}

/**
 * Kernel debugger integration
 */
extern "C" void starforth_kernel_repl(void) {
    // Interactive REPL for kernel debugging
    // Use polling serial I/O, no interrupts

    char buffer[256];

    while (1) {
        // Read line from serial (implement polling version)
        kernel_serial_read_line(buffer, sizeof(buffer));

        if (strcmp(buffer, "exit") == 0) break;

        vm_interpret(&kernel_vm, buffer);

        if (kernel_vm.error) {
            kernel_serial_write("Error\n");
            kernel_vm.error = 0;
        }
    }
}
```

## StarshipOS Specifics

### Integration Points

1. **Boot Service** - Start StarForth server during boot
2. **System Configuration** - Use Forth for configuration
3. **Hot-patching** - Runtime system updates
4. **Interactive Debugging** - REPL for live system

### Boot Integration

Modify StarshipOS boot sequence:

```cpp
// In your StarshipOS init task
void starship_boot() {
    // ... other initialization ...

    // Start StarForth server
    L4Re::Util::Env_ns ns;
    ns.register_obj("starforth",
                    L4Re::Env::env()->get_cap<void>("starforth_server"));

    // Load boot scripts
    starforth_load_script("/boot/init.fth");

    // ... continue boot ...
}
```

### System Configuration Example

Create `/boot/init.fth`:

```forth
\ StarshipOS Boot Configuration

." StarshipOS initializing..." CR

\ Configure kernel parameters
: SET-KERNEL-PARAM ( value param-id -- )
  \ ... syscall to set kernel parameter ...
;

\ Example: Set scheduler quantum
100 1 SET-KERNEL-PARAM

\ Start system services
: START-SERVICE ( service-name -- )
  \ ... load and start service ...
;

" network-stack" START-SERVICE
" file-system" START-SERVICE

." Boot complete!" CR
```

## Building and Testing

### Build Commands

```bash
# From L4Re source root
cd l4/pkg/starforth

# Build for x86_64
make ARCH=amd64 O=build_amd64
make ARCH=amd64 O=build_amd64 install

# Build for ARM64
make ARCH=arm64 O=build_arm64
make ARCH=arm64 O=build_arm64 install

# Cross-build from x86_64
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- O=build_arm64_cross
```

### Creating Boot Image

```bash
# Create modules list
cd l4/conf
cat > myconf.list << EOF
modaddr 0x02000000

entry starforth_server
kernel fiasco -serial_esc
roottask moe rom/myconf.cfg
module l4re
module ned rom/myconf.lua
module starforth_server
module forth_client
EOF

# Build image
make O=mybuild qemu E=myconf
```

### Running in QEMU

```bash
# x86_64
make O=mybuild qemu E=myconf

# ARM64 (for Raspberry Pi 4 testing)
make O=mybuild ARCH=arm64 PLATFORM_TYPE=rv_pbx qemu E=myconf
```

## Performance Considerations

### Memory Layout Optimization

```cpp
// Align VM memory to huge pages (2MB on x86_64, ARM64)
#define VM_MEMORY_SIZE (2 * 1024 * 1024)

// Request huge page when allocating dataspace
l4re_ds_flags_t flags = L4RE_DS_F_NORMAL | L4RE_DS_F_ALIGN(21); // 2^21 = 2MB
long ret = l4re_ma_alloc_align(VM_MEMORY_SIZE, memory_ds_, flags, 21);
```

### IPC Optimization

```cpp
// Use shared memory for large data transfers
class FastForthClient {
public:
    bool interpret_large(const char* code, size_t len) {
        if (len < 1024) {
            return interpret_small(code, len);  // Use IPC
        }

        // Use shared dataspace for large code
        l4_cap_idx_t ds = create_shared_buffer(len);
        memcpy(shared_addr_, code, len);

        // Send dataspace capability + offset
        send_interpret_ds(ds, 0, len);

        return true;
    }
};
```

### CPU Affinity

```cpp
// Pin Forth VM to specific CPU for better cache locality
l4_sched_param_t sp = l4_sched_param(255, 0);  // Priority 255, CPU 0
sp.affinity = l4_sched_cpu_set(0, 0);  // CPU 0 only

l4_scheduler()->run_thread(L4Re::Env::env()->main_thread(), sp);
```

## Debugging

### GDB with L4Re

```bash
# Start QEMU with GDB server
make O=mybuild qemu E=myconf QEMU_OPTIONS="-s -S"

# In another terminal
aarch64-linux-gnu-gdb build_arm64/pkg/starforth/server/starforth_server
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

### JDB (Fiasco Kernel Debugger)

```
# Enter JDB
Ctrl+^ (or configured escape sequence)

# Show tasks
l t

# Switch to StarForth server task
t <task_id>

# Show threads
l T

# Backtrace
i s
```

### Logging

```cpp
// Use L4Re logging
#include <l4/re/util/debug>

L4Re::Util::Dbg log(L4Re::Util::Dbg::Info, "starforth");

log.printf("VM state: dsp=%d error=%d\n", vm->dsp, vm->error);
```

## Security Considerations

### Capability-based Security

```cpp
// Restrict VM capabilities
class RestrictedVM {
public:
    RestrictedVM() {
        // Only grant specific capabilities
        vm_grant_cap(CAP_MEMORY_READ);
        vm_grant_cap(CAP_COMPUTE);
        // Do NOT grant CAP_MEMORY_WRITE or CAP_IPC
    }
};
```

### Sandboxing

```lua
-- In Ned configuration
local vm = L4.default_loader:start({
  caps = {
    -- Only grant necessary capabilities
    icu = L4.Env.icu,
    log = L4.Env.log,
    -- No scheduler, no memory allocator
  },
  log = {"vm", "yellow"},
}, "rom/starforth_server")
```

## Troubleshooting

### Common Issues

**Problem**: Dataspace allocation fails

```cpp
// Check quota
l4re_ma_query_quota();

// Increase quota in Ned config
entry {
  mem = 8 * 1024 * 1024,  -- 8MB quota
}
```

**Problem**: IPC timeout

```cpp
// Increase timeout
l4_timeout_t timeout = l4_timeout(L4_IPC_TIMEOUT_NEVER);
tag = l4_ipc_call(cap, utcb, tag, timeout);
```

**Problem**: Capability not found

```lua
-- Check Ned configuration
caps = {
  starforth = L4.default_loader:new_channel(),  -- Create channel
}
```

## References

- [L4Re Documentation](https://l4re.org/doc/)
- [Fiasco.OC Reference Manual](https://l4re.org/fiasco/)
- [L4Re Tutorial](https://l4re.org/doc/tutorial.html)
- [TU Dresden L4 Research](https://os.inf.tu-dresden.de/L4/)

## Next Steps

1. Port your existing StarForth code to L4Re package structure
2. Test in QEMU with L4Re
3. Deploy to actual hardware (x86_64 or Raspberry Pi 4)
4. Integrate with StarshipOS services
5. Add security policies and sandboxing
6. Performance tuning with real workloads

Good luck with your L4Re integration! 🚀
