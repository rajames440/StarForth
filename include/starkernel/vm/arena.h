/**
 * arena.h - VM arena management for StarKernel builds.
 *
 * Hosted builds never include this header. Kernel integration layers use it
 * to provision the PMM-backed VM arena before handing memory to the VM core.
 */

#ifndef STARKERNEL_VM_ARENA_H
#define STARKERNEL_VM_ARENA_H

#ifdef __STARKERNEL__

#include <stddef.h>
#include <stdint.h>

uint64_t sk_vm_arena_alloc(void);
void     sk_vm_arena_free(void);
uint8_t *sk_vm_arena_ptr(void);
size_t   sk_vm_arena_size(void);
int      sk_vm_arena_is_initialized(void);
void     sk_vm_arena_assert_guards(const char *tag);

#endif /* __STARKERNEL__ */

#endif /* STARKERNEL_VM_ARENA_H */
