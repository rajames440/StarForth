#ifndef VM_API_H
#define VM_API_H

#include <stddef.h>
#include <stdint.h>

/* Opaque VM type to API consumers */
struct VM;
typedef struct VM VM;

/* Stack ops */
void vm_api_push(VM *vm, intptr_t v);
intptr_t vm_api_pop(VM *vm);
void vm_api_rpush(VM *vm, intptr_t v);
intptr_t vm_api_rpop(VM *vm);

/* Dictionary ops needed by word registration */
typedef void (*word_func_t)(VM *vm);  /* mirror vm.h, but no struct exposure */

void*      vm_api_create_word(VM *vm, const char *name, size_t len, word_func_t fn);
void*      vm_api_find_word  (VM *vm, const char *name, size_t len);
void       vm_api_make_immediate(VM *vm);
void       vm_api_hide_word     (VM *vm);
void       vm_api_smudge_word   (VM *vm);

#endif /* VM_API_H */
