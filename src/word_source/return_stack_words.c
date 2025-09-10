/*
                                 ***   StarForth   ***
  return_stack_words.c - FORTH-79 Standard and ANSI C99 ONLY
  Last modified - 2025-09-10
  Public domain / CC0 — No warranty.

  FORTH-79 compliance:
  - Provide only >R, R>, R@ for return stack transfer.
  - No direct stack addressing (no RP!, RP@) — forbidden by FORTH-79.
*/

#include "include/return_stack_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"
#include "../../include/vm.h"

/* >R  ( x -- )  move TOS from data stack to return stack */
static void return_stack_word_to_r(VM *vm) {
    if (vm->dsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, ">R: DSP underflow");
        return;
    }
    if (vm->rsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, ">R: RSP overflow");
        return;
    }
    cell_t x = vm_pop(vm);
    vm_rpush(vm, x);
}

/* R>  ( -- x )  move TOS from return stack back to data stack */
static void return_stack_word_r_from(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "R>: RSP underflow");
        return;
    }
    if (vm->dsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "R>: DSP overflow");
        return;
    }
    cell_t x = vm_rpop(vm);
    vm_push(vm, x);
}

/* R@  ( -- x )  copy TOS of return stack to data stack */
static void return_stack_word_r_fetch(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "R@: RSP underflow");
        return;
    }
    if (vm->dsp + 1 >= STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "R@: DSP overflow");
        return;
    }
    cell_t x = vm->return_stack[vm->rsp];
    vm_push(vm, x);
}

void register_return_stack_words(VM *vm) {
    if (!vm) return;
    log_message(LOG_INFO, "Registering FORTH-79 return stack words...");
    register_word(vm, ">R", return_stack_word_to_r);
    register_word(vm, "R>", return_stack_word_r_from);
    register_word(vm, "R@", return_stack_word_r_fetch);

    /* FORTH-79 forbids direct stack addressing; RP! / RP@ are deliberately omitted. */
    log_message(LOG_INFO, "Return stack words registered (no RP!/RP@; FORTH-79 compliant).");
}