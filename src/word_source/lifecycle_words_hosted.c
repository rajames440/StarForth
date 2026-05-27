/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0
*/

/**
 * lifecycle_words_hosted.c - BIRTH KILL PAUSE RESUME USE for hosted Linux build
 *
 * Hosted-build counterpart to src/starkernel/capsule/lifecycle_words.c.
 * All five words are ( c-addr u -- ), stack-clean, errors logged not pushed.
 *
 * In the hosted build there is no capsule blob or kernel VM registry, so
 * these implementations consume their arguments and log intent.  The kernel
 * build (compiled with -D__STARKERNEL__) skips this file entirely because
 * the Makefile glob does not include src/word_source/ for that target.
 */

#ifndef __STARKERNEL__

#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

#include <string.h>

#define NAME_MAX_LEN 64

static void extract_name(VM *vm, cell_t caddr, cell_t u, char *out, int max) {
    int len = (int)u;
    if (len >= max) len = max - 1;
    if (len < 0)    len = 0;
    if (len > 0 && caddr < (cell_t)VM_MEMORY_SIZE)
        memcpy(out, (const char *)vm->memory + caddr, (size_t)len);
    out[len] = '\0';
}

static void hosted_lifecycle_birth(VM *vm) {
    cell_t u     = vm_pop(vm);
    cell_t caddr = vm_pop(vm);
    char name[NAME_MAX_LEN];
    extract_name(vm, caddr, u, name, NAME_MAX_LEN);
    log_message(LOG_INFO, "BIRTH %s (hosted)", name);
}

static void hosted_lifecycle_kill(VM *vm) {
    cell_t u     = vm_pop(vm);
    cell_t caddr = vm_pop(vm);
    char name[NAME_MAX_LEN];
    extract_name(vm, caddr, u, name, NAME_MAX_LEN);
    log_message(LOG_INFO, "KILL %s (hosted)", name);
}

static void hosted_lifecycle_pause(VM *vm) {
    cell_t u     = vm_pop(vm);
    cell_t caddr = vm_pop(vm);
    char name[NAME_MAX_LEN];
    extract_name(vm, caddr, u, name, NAME_MAX_LEN);
    log_message(LOG_INFO, "PAUSE %s (hosted)", name);
}

static void hosted_lifecycle_resume(VM *vm) {
    cell_t u     = vm_pop(vm);
    cell_t caddr = vm_pop(vm);
    char name[NAME_MAX_LEN];
    extract_name(vm, caddr, u, name, NAME_MAX_LEN);
    log_message(LOG_INFO, "RESUME %s (hosted)", name);
}

static void hosted_lifecycle_use(VM *vm) {
    cell_t u     = vm_pop(vm);
    cell_t caddr = vm_pop(vm);
    char name[NAME_MAX_LEN];
    extract_name(vm, caddr, u, name, NAME_MAX_LEN);
    log_message(LOG_INFO, "USE %s (hosted)", name);
}

void register_lifecycle_words(VM *vm) {
    register_word(vm, "BIRTH",  hosted_lifecycle_birth);
    register_word(vm, "KILL",   hosted_lifecycle_kill);
    register_word(vm, "PAUSE",  hosted_lifecycle_pause);
    register_word(vm, "RESUME", hosted_lifecycle_resume);
    register_word(vm, "USE",    hosted_lifecycle_use);
}

#endif /* !__STARKERNEL__ */
