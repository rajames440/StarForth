/*

                                 ***   StarForth   ***
  vm_api.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

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
