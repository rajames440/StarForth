#ifndef VM_API_H
#define VM_API_H

#include "vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== VM input API (TIB, >IN, SPAN) ===== */
int vm_input_ensure(VM * vm); /* alloc/init once */
unsigned char *vm_input_tib(VM * vm); /* VM addr of TIB buffer */
cell_t *vm_input_in(VM * vm); /* VM addr of >IN cell */
cell_t *vm_input_span(VM * vm); /* VM addr of SPAN cell */
void vm_input_load_line(VM *vm, const char *src, size_t n); /* copy into TIB, set SPAN, reset >IN */
void vm_input_source(VM * vm, cell_t * out_addr, cell_t * out_len); /* SOURCE pair (addr,u) */
cell_t vm_addr_from_ptr(VM *vm, void *p); /* convert host ptr -> Forth address (offset into vm->memory) */

#ifdef __cplusplus
}
#endif

#endif /* VM_API_H */