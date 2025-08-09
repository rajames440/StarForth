/*

                                 ***   StarForth   ***
  vm_api.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

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
