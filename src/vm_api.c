#include "vm_api.h"
#include "vm.h"       // internal details, not re-exposed to word modules
#include "log.h"

/* Stack wrappers */
void vm_api_push(VM *vm, intptr_t v)            { vm_push(vm, v); }
intptr_t vm_api_pop(VM *vm)                      { return vm_pop(vm); }
void vm_api_rpush(VM *vm, intptr_t v)           { vm_rpush(vm, v); }
intptr_t vm_api_rpop(VM *vm)                     { return vm_rpop(vm); }

/* Dictionary wrappers */
void* vm_api_create_word(VM *vm, const char *n, size_t len, word_func_t fn) {
    return (void*)vm_create_word(vm, n, len, (void(*)(struct VM*))fn);
}
void* vm_api_find_word(VM *vm, const char *n, size_t len) {
    return (void*)vm_find_word(vm, n, len);
}
void vm_api_make_immediate(VM *vm) { vm_make_immediate(vm); }
void vm_api_hide_word(VM *vm)      { vm_hide_word(vm); }
void vm_api_smudge_word(VM *vm)    { vm_smudge_word(vm); }
