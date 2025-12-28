#ifndef VM_INTERNAL_H
#define VM_INTERNAL_H

#include "../include/vm.h"
#include "../include/vm_host.h"

#ifndef PARITY_MODE
#define PARITY_MODE 0
#endif

#ifndef SK_PARITY_DEBUG
#define SK_PARITY_DEBUG 1
#endif

#if HEARTBEAT_THREAD_ENABLED && !defined(L4RE_TARGET)
#include <pthread.h>
#define HEARTBEAT_HAS_THREADS 1
#else
#define HEARTBEAT_HAS_THREADS 0
#endif

#define HEARTBEAT_DECAY_BATCH 64u

typedef struct HeartbeatWorker {
#if HEARTBEAT_HAS_THREADS
    pthread_t thread;
#endif
    uint64_t tick_ns;
    int running;
    int stop_requested;
} HeartbeatWorker;

const VMHostServices* vm_default_host_services(void);
const VMHostServices* vm_host(const VM *vm);
void* vm_host_alloc(VM *vm, size_t size, size_t align);
void* vm_host_calloc(VM *vm, size_t n, size_t size);
void vm_host_free(VM *vm, void *ptr);
#ifndef __STARKERNEL__
void vm_reset_hosted_fake_ns(VM *vm);
#endif

uint64_t vm_monotonic_ns(VM *vm);
unsigned vm_get_base(const VM* vm);
void vm_set_base(VM* vm, unsigned b);
uint32_t heartbeat_snapshot_index_load(const volatile uint32_t *ptr);
void heartbeat_snapshot_index_store(volatile uint32_t *ptr, uint32_t value);
void heartbeat_publish_snapshot(VM *vm);
void vm_heartbeat_run_cycle(VM *vm);
#if HEARTBEAT_HAS_THREADS
void* heartbeat_thread_main(void *arg);
#endif

#endif /* VM_INTERNAL_H */
