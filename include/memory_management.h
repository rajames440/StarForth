#ifndef VM_MEMORY_H
#define VM_MEMORY_H
#include "vm.h"
void* vm_allot(VM *vm, size_t bytes);
void  vm_align(VM *vm);
#endif
