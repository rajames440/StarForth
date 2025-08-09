#ifndef VM_API_H
#define VM_API_H

#include "vm.h"
#include <stddef.h>
#include <stdint.h>

/* Offset <-> pointer helpers */
cell_t   vm_off(VM *vm, const void *p);        /* ptr -> offset (or -1 on error) */
uint8_t* vm_addr(VM *vm, cell_t off);          /* offset -> ptr (or NULL on error) */

/* Safe memory ops in offset space */
int vm_api_store_cell(VM *vm, cell_t addr, cell_t x);
int vm_api_fetch_cell(VM *vm, cell_t addr, cell_t *out);
int vm_api_add_cell  (VM *vm, cell_t addr, cell_t delta);
int vm_api_store_byte(VM *vm, cell_t addr, uint8_t b);
int vm_api_fetch_byte(VM *vm, cell_t addr, uint8_t *out);
int vm_api_fill      (VM *vm, cell_t addr, size_t n, uint8_t b);
int vm_api_erase     (VM *vm, cell_t addr, size_t n);

#endif /* VM_API_H */
