/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  Licensed under the StarForth License, Version 1.0.
*/

/*
 * DEFER / IS / DEFER@ primitives — late-binding deferred words.
 *
 * DEFER  name  ( -- )     Create a deferred word, data field = 0 (unset).
 * IS     name  ( xt -- )  Store xt into the data field of a DEFER word.
 * DEFER@ name  ( -- xt )  Fetch the current action xt of a DEFER word.
 *
 * When a deferred word executes, defer_runtime reads its data field
 * (a raw DictEntry*) and calls that entry's func directly.  This is
 * essential because the inner interpreter stores DictEntry* pointers
 * in compiled bodies at compile time — redefining a colon word after
 * compilation does not update callers.  DEFER/IS gives late binding
 * without any name-lookup overhead at execution time.
 *
 * IS enforces that the named word was created by DEFER (func ==
 * defer_runtime).  Attempting IS on any other word sets vm->error.
 */

#include "include/defer_words.h"
#include "../../include/vm.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/dictionary_management.h"

#include <stdint.h>

/* ── defer_runtime ─────────────────────────────────────────────────────
 * The func stored in every DEFER-created word.
 * Data field = 0 means unset → error.  Non-zero = DictEntry* to call.
 */
static void defer_runtime(VM *vm)
{
    if (!vm || !vm->current_executing_entry) { if (vm) vm->error = 1; return; }

    DictEntry *self = vm->current_executing_entry;
    cell_t    *df   = vm_dictionary_get_data_field(self);
    if (!df || *df == 0)
    {
        log_message(LOG_ERROR, "DEFER runtime: no target set for '%.*s'",
                    (int)self->name_len, self->name);
        vm->error = 1;
        return;
    }

    DictEntry *target = (DictEntry *)(uintptr_t)(*df);
    if (!target || !target->func)
    {
        log_message(LOG_ERROR, "DEFER runtime: invalid target for '%.*s'",
                    (int)self->name_len, self->name);
        vm->error = 1;
        return;
    }

    vm->current_executing_entry = target;
    target->func(vm);
}

/* ── DEFER ─────────────────────────────────────────────────────────────
 * DEFER name  ( -- )
 * Creates a new dictionary entry with defer_runtime as its func.
 * Data field initialised to 0 (unset).  Calling the word before IS
 * has been used will set vm->error.
 */
static void word_defer(VM *vm)
{
    char name[WORD_NAME_MAX + 1];
    int  len = vm_parse_word(vm, name, sizeof(name));
    if (len <= 0)
    {
        log_message(LOG_ERROR, "DEFER: missing name");
        vm->error = 1;
        return;
    }

    DictEntry *de = vm_create_word(vm, name, (size_t)len, defer_runtime);
    if (!de)
    {
        log_message(LOG_ERROR, "DEFER: vm_create_word failed for '%.*s'", len, name);
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(de);
    if (!df)
    {
        log_message(LOG_ERROR, "DEFER: no data field for '%.*s'", len, name);
        vm->error = 1;
        return;
    }

    *df = 0;  /* unset — defer_runtime will error if called before IS */

    log_message(LOG_DEBUG, "DEFER: created '%.*s' (unset)", len, name);
}

/* ── IS ────────────────────────────────────────────────────────────────
 * IS name  ( xt -- )
 * Pops an execution token (DictEntry*) from the stack, parses the next
 * word from input, and stores the xt into that word's data field.
 * The named word MUST have been created by DEFER; any other word causes
 * an error.  This prevents corrupting non-DEFER words' memory.
 */
static void word_is(VM *vm)
{
    if (vm->dsp < 0)
    {
        log_message(LOG_ERROR, "IS: stack underflow");
        vm->error = 1;
        return;
    }

    cell_t xt = vm_pop(vm);

    char name[WORD_NAME_MAX + 1];
    int  len = vm_parse_word(vm, name, sizeof(name));
    if (len <= 0)
    {
        log_message(LOG_ERROR, "IS: missing name");
        vm->error = 1;
        return;
    }

    DictEntry *de = vm_find_word(vm, name, (size_t)len);
    if (!de)
    {
        log_message(LOG_ERROR, "IS: word '%.*s' not found", len, name);
        vm->error = 1;
        return;
    }

    if (de->func != defer_runtime)
    {
        log_message(LOG_ERROR, "IS: '%.*s' was not created by DEFER", len, name);
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(de);
    if (!df)
    {
        log_message(LOG_ERROR, "IS: no data field for '%.*s'", len, name);
        vm->error = 1;
        return;
    }

    *df = xt;
    log_message(LOG_DEBUG, "IS: stored xt into '%.*s'", len, name);
}

/* ── DEFER@ ────────────────────────────────────────────────────────────
 * DEFER@ name  ( -- xt )
 * Parses the next word from input and pushes its current action xt.
 * The named word must have been created by DEFER.
 */
static void word_defer_fetch(VM *vm)
{
    char name[WORD_NAME_MAX + 1];
    int  len = vm_parse_word(vm, name, sizeof(name));
    if (len <= 0)
    {
        log_message(LOG_ERROR, "DEFER@: missing name");
        vm->error = 1;
        return;
    }

    DictEntry *de = vm_find_word(vm, name, (size_t)len);
    if (!de)
    {
        log_message(LOG_ERROR, "DEFER@: word '%.*s' not found", len, name);
        vm->error = 1;
        return;
    }

    if (de->func != defer_runtime)
    {
        log_message(LOG_ERROR, "DEFER@: '%.*s' was not created by DEFER", len, name);
        vm->error = 1;
        return;
    }

    cell_t *df = vm_dictionary_get_data_field(de);
    if (!df)
    {
        log_message(LOG_ERROR, "DEFER@: no data field for '%.*s'", len, name);
        vm->error = 1;
        return;
    }

    vm_push(vm, *df);
    log_message(LOG_DEBUG, "DEFER@: fetched xt from '%.*s'", len, name);
}

/* ── Registration ──────────────────────────────────────────────────────*/
void register_defer_words(VM *vm)
{
    register_word(vm, "DEFER",  word_defer);
    register_word(vm, "IS",     word_is);
    register_word(vm, "DEFER@", word_defer_fetch);
}
