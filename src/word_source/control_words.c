/*

                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

/* control_words.c - FORTH-79 Control Flow Words */
#include "include/control_words.h"
#include "../../include/word_registry.h"
#include "../../include/log.h"

/* Forward declarations for runtime functions */
static void control_forth_runtime_do(VM *vm);
static void control_forth_runtime_loop(VM *vm);
static void control_forth_runtime_plus_loop(VM *vm);

/* Loop parameter stack for DO/LOOP constructs - FORTH-79 spec allows this */
#define LOOP_STACK_SIZE 16
typedef struct {
    cell_t limit;
    cell_t index;
    void *loop_start_ip;  /* Store the loop start address */
} loop_params_t;
static loop_params_t loop_stack[LOOP_STACK_SIZE];
static int loop_sp = -1;

/* Loop stack operations */
static void loop_push(cell_t limit, cell_t index, void *loop_start) {
    if (loop_sp >= LOOP_STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Loop stack overflow");
        return;
    }
    loop_sp++;
    loop_stack[loop_sp].limit = limit;
    loop_stack[loop_sp].index = index;
    loop_stack[loop_sp].loop_start_ip = loop_start;
}

static void loop_pop(void) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Loop stack underflow");
        return;
    }
    loop_sp--;
}

/* BRANCH ( -- ) Unconditional branch - runtime primitive */
static void control_forth_branch(VM *vm) {
    cell_t *ip;
    cell_t offset;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "BRANCH: Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Get current instruction pointer from return stack */
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    /* Read branch offset */
    offset = *ip;

    /* Update IP with new address */
    ip = (cell_t*)((char*)ip + offset);
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;

    log_message(LOG_DEBUG, "BRANCH: Branched by %ld", (long)offset);
}

/* 0BRANCH ( flag -- ) Branch if flag is zero */
static void control_forth_0branch(VM *vm) {
    cell_t *ip;
    cell_t flag, offset;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0BRANCH: Data stack underflow");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "0BRANCH: Return stack underflow");
        vm->error = 1;
        return;
    }

    flag = vm_pop(vm);
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (flag == 0) {
        /* Branch - read offset and jump */
        offset = *ip;
        ip = (cell_t*)((char*)ip + offset);
    } else {
        /* Don't branch - skip over offset */
        ip++;
    }

    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;

    log_message(LOG_DEBUG, "0BRANCH: flag=%ld", (long)flag);
}

/* (LIT) ( -- n ) Push inline literal - runtime primitive */
static void control_forth_lit(VM *vm) {
    cell_t *ip;
    cell_t literal_value;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "(LIT): Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Get current instruction pointer from return stack */
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    /* Read the literal value */
    literal_value = *ip;

    /* Advance IP past the literal */
    ip++;
    vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;

    /* Push the literal onto data stack */
    vm_push(vm, literal_value);

    log_message(LOG_DEBUG, "(LIT): Pushed literal %ld", (long)literal_value);
}

/* IF ( flag -- ) Conditional execution - compile-time */
static void control_forth_if(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "IF: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Compile 0BRANCH with placeholder address */
    vm_compile_call(vm, control_forth_0branch);

    /* Reserve space for branch offset and push address on return stack for THEN */
    size_t placeholder_addr = vm->here;
    vm_compile_literal(vm, 0);  /* Placeholder for branch target */

    /* Push placeholder address on return stack - FORTH-79 way */
    if (vm->rsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "IF: Return stack overflow");
        vm->error = 1;
        return;
    }
    vm->return_stack[++vm->rsp] = placeholder_addr;

    log_message(LOG_DEBUG, "IF: Compiled 0BRANCH with placeholder at %zu", placeholder_addr);
}

/* THEN ( -- ) End of IF - compile-time */
static void control_forth_then(VM *vm) {
    cell_t placeholder_addr;
    cell_t target_addr, branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "THEN: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "THEN: No matching IF");
        vm->error = 1;
        return;
    }

    /* Get the placeholder address from return stack */
    placeholder_addr = vm->return_stack[vm->rsp--];

    /* Calculate branch offset */
    target_addr = vm->here;
    branch_offset = target_addr - placeholder_addr - sizeof(cell_t);

    /* Patch the placeholder */
    *(cell_t*)(vm->memory + placeholder_addr) = branch_offset;

    log_message(LOG_DEBUG, "THEN: Patched branch offset %ld at %ld",
                (long)branch_offset, (long)placeholder_addr);
}

/* ELSE ( -- ) Alternative execution - compile-time */
static void control_forth_else(VM *vm) {
    cell_t if_placeholder, else_target, if_offset;
    size_t else_branch_addr;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "ELSE: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "ELSE: No matching IF");
        vm->error = 1;
        return;
    }

    /* Compile unconditional branch to skip ELSE part */
    vm_compile_call(vm, control_forth_branch);
    else_branch_addr = vm->here;
    vm_compile_literal(vm, 0);  /* Placeholder for THEN target */

    /* Patch the IF's 0BRANCH to jump here */
    if_placeholder = vm->return_stack[vm->rsp];
    else_target = vm->here;
    if_offset = else_target - if_placeholder - sizeof(cell_t);
    *(cell_t*)(vm->memory + if_placeholder) = if_offset;

    /* Replace IF's placeholder with ELSE's placeholder on return stack */
    vm->return_stack[vm->rsp] = else_branch_addr;

    log_message(LOG_DEBUG, "ELSE: Patched IF offset %ld, new placeholder at %zu",
                (long)if_offset, else_branch_addr);
}

/* BEGIN ( -- ) Start indefinite loop - compile-time */
static void control_forth_begin(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "BEGIN: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Push current position on return stack as loop target */
    if (vm->rsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "BEGIN: Return stack overflow");
        vm->error = 1;
        return;
    }
    vm->return_stack[++vm->rsp] = vm->here;

    log_message(LOG_DEBUG, "BEGIN: Loop target at %zu", vm->here);
}

/* UNTIL ( flag -- ) End loop if flag true - compile-time */
static void control_forth_until(VM *vm) {
    cell_t begin_addr, branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "UNTIL: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "UNTIL: No matching BEGIN");
        vm->error = 1;
        return;
    }

    /* Compile 0BRANCH back to BEGIN */
    vm_compile_call(vm, control_forth_0branch);

    begin_addr = vm->return_stack[vm->rsp--];
    branch_offset = begin_addr - vm->here - sizeof(cell_t);
    vm_compile_literal(vm, branch_offset);

    log_message(LOG_DEBUG, "UNTIL: Branch offset %ld to BEGIN at %ld",
                (long)branch_offset, (long)begin_addr);
}

/* AGAIN ( -- ) End infinite loop - compile-time */
static void control_forth_again(VM *vm) {
    cell_t begin_addr, branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "AGAIN: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "AGAIN: No matching BEGIN");
        vm->error = 1;
        return;
    }

    /* Compile unconditional branch back to BEGIN */
    vm_compile_call(vm, control_forth_branch);

    begin_addr = vm->return_stack[vm->rsp--];
    branch_offset = begin_addr - vm->here - sizeof(cell_t);
    vm_compile_literal(vm, branch_offset);

    log_message(LOG_DEBUG, "AGAIN: Infinite loop branch offset %ld", (long)branch_offset);
}

/* DO ( limit start -- ) Start definite loop - compile-time only! */
static void control_forth_do(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "DO: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Compile runtime DO which will handle limit/start */
    vm_compile_call(vm, control_forth_runtime_do);

    /* Push current position for LOOP to branch back to */
    if (vm->rsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "DO: Return stack overflow");
        vm->error = 1;
        return;
    }
    vm->return_stack[++vm->rsp] = vm->here;

    log_message(LOG_DEBUG, "DO: Loop start at %zu", vm->here);
}

/* Runtime DO ( limit start -- ) */
static void control_forth_runtime_do(VM *vm) {
    cell_t start, limit;
    void *loop_start_ip;

    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "Runtime DO: Data stack underflow");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime DO: Return stack underflow");
        vm->error = 1;
        return;
    }

    start = vm_pop(vm);
    limit = vm_pop(vm);

    /* Get current IP from return stack - this is where we'll loop back to */
    loop_start_ip = (void*)(uintptr_t)vm->return_stack[vm->rsp];

    /* Push loop parameters onto loop stack */
    loop_push(limit, start, loop_start_ip);

    log_message(LOG_DEBUG, "Runtime DO: limit=%ld start=%ld", (long)limit, (long)start);
}

/* LOOP ( -- ) End DO loop, increment by 1 - compile-time */
static void control_forth_loop(VM *vm) {
    cell_t do_addr;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "LOOP: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "LOOP: No matching DO");
        vm->error = 1;
        return;
    }

    /* Compile runtime LOOP */
    vm_compile_call(vm, control_forth_runtime_loop);

    /* Pop the DO address from compile-time return stack */
    do_addr = vm->return_stack[vm->rsp--];

    log_message(LOG_DEBUG, "LOOP: Compiled for DO at %ld", (long)do_addr);
}

/* Runtime LOOP ( -- ) */
static void control_forth_runtime_loop(VM *vm) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Runtime LOOP: No matching DO");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime LOOP: Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Increment index */
    loop_stack[loop_sp].index++;

    /* Check if loop should continue */
    if (loop_stack[loop_sp].index < loop_stack[loop_sp].limit) {
        /* Continue loop - set IP back to loop start */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)loop_stack[loop_sp].loop_start_ip;
        log_message(LOG_DEBUG, "Runtime LOOP: Continuing, index=%ld", (long)loop_stack[loop_sp].index);
    } else {
        /* Exit loop - pop loop parameters and continue normally */
        loop_pop();
        log_message(LOG_DEBUG, "Runtime LOOP: Exiting loop");
    }
}

/* +LOOP ( n -- ) End DO loop, increment by n - compile-time */
static void control_forth_plus_loop(VM *vm) {
    cell_t do_addr;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "+LOOP: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "+LOOP: No matching DO");
        vm->error = 1;
        return;
    }

    /* Compile runtime +LOOP */
    vm_compile_call(vm, control_forth_runtime_plus_loop);

    /* Pop the DO address from compile-time return stack */
    do_addr = vm->return_stack[vm->rsp--];

    log_message(LOG_DEBUG, "+LOOP: Compiled for DO at %ld", (long)do_addr);
}

/* Runtime +LOOP ( n -- ) */
static void control_forth_runtime_plus_loop(VM *vm) {
    cell_t increment;
    int should_continue;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: Data stack underflow");
        vm->error = 1;
        return;
    }

    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: No matching DO");
        vm->error = 1;
        return;
    }

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: Return stack underflow");
        vm->error = 1;
        return;
    }

    increment = vm_pop(vm);

    /* Increment index by n */
    loop_stack[loop_sp].index += increment;

    /* Check if loop should continue (handles negative increments) */
    if (increment >= 0) {
        should_continue = (loop_stack[loop_sp].index < loop_stack[loop_sp].limit);
    } else {
        should_continue = (loop_stack[loop_sp].index >= loop_stack[loop_sp].limit);
    }

    if (should_continue) {
        /* Continue loop - set IP back to loop start */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)loop_stack[loop_sp].loop_start_ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: Continuing, increment=%ld, index=%ld",
                    (long)increment, (long)loop_stack[loop_sp].index);
    } else {
        /* Exit loop - pop loop parameters and continue normally */
        loop_pop();
        log_message(LOG_DEBUG, "Runtime +LOOP: Exiting loop");
    }
}

/* I ( -- n ) Current loop index */
static void control_forth_i(VM *vm) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "I: No active loop");
        vm->error = 1;
        return;
    }

    vm_push(vm, loop_stack[loop_sp].index);
    log_message(LOG_DEBUG, "I: Pushed index %ld", (long)loop_stack[loop_sp].index);
}

/* J ( -- n ) Outer loop index */
static void control_forth_j(VM *vm) {
    if (loop_sp < 1) {
        log_message(LOG_ERROR, "J: No outer loop");
        vm->error = 1;
        return;
    }

    vm_push(vm, loop_stack[loop_sp - 1].index);
    log_message(LOG_DEBUG, "J: Pushed outer index %ld", (long)loop_stack[loop_sp - 1].index);
}

/* LEAVE ( -- ) Exit current loop */
static void forth_leave(VM *vm) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "LEAVE: No active loop");
        vm->error = 1;
        return;
    }

    /* Force loop exit by setting index = limit */
    loop_stack[loop_sp].index = loop_stack[loop_sp].limit;
    log_message(LOG_DEBUG, "LEAVE: Forced loop exit");
}

/* UNLOOP ( -- ) Discard loop parameters */
static void forth_unloop(VM *vm) {
    if (loop_sp < 0) {
        log_message(LOG_ERROR, "UNLOOP: No active loop");
        vm->error = 1;
        return;
    }

    loop_pop();
    log_message(LOG_DEBUG, "UNLOOP: Discarded loop parameters");
}

/* EXECUTE ( xt -- ) Execute word at address */
static void control_forth_execute(VM *vm) {
    cell_t xt;
    word_func_t func;

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "EXECUTE: Data stack underflow");
        vm->error = 1;
        return;
    }

    xt = vm_pop(vm);
    func = (word_func_t)(uintptr_t)xt;

    if (func) {
        func(vm);
        log_message(LOG_DEBUG, "EXECUTE: Called function at %p", (void*)func);
    } else {
        log_message(LOG_ERROR, "EXECUTE: NULL execution token");
        vm->error = 1;
    }
}

/* ABORT ( -- ) Abort execution */
static void control_forth_abort(VM *vm) {
    /* Clear stacks and reset state */
    vm->dsp = -1;
    vm->rsp = -1;
    loop_sp = -1;
    vm->mode = MODE_INTERPRET;
    vm->error = 1;
    vm->halted = 1;

    log_message(LOG_INFO, "ABORT: Execution aborted");
}

/* QUIT ( -- ) Return to interpreter */
static void control_forth_quit(VM *vm) {
    /* Clear return stack but preserve data stack */
    vm->rsp = -1;
    loop_sp = -1;
    vm->mode = MODE_INTERPRET;
    vm->error = 0;

    log_message(LOG_INFO, "QUIT: Returned to interpreter");
}

void register_control_words(VM *vm) {
    log_message(LOG_INFO, "Registering FORTH-79 control flow words...");

    /* Runtime primitives */
    register_word(vm, "BRANCH", control_forth_branch);
    register_word(vm, "0BRANCH", control_forth_0branch);
    register_word(vm, "(LIT)", control_forth_lit);
    register_word(vm, "LIT", control_forth_lit);

    /* Conditional execution */
    register_word(vm, "IF", control_forth_if);
    register_word(vm, "THEN", control_forth_then);
    register_word(vm, "ELSE", control_forth_else);

    /* Indefinite loops */
    register_word(vm, "BEGIN", control_forth_begin);
    register_word(vm, "UNTIL", control_forth_until);
    register_word(vm, "AGAIN", control_forth_again);

    /* Definite loops - compile-time + runtime pairs */
    register_word(vm, "DO", control_forth_do);
    register_word(vm, "(DO)", control_forth_runtime_do);
    register_word(vm, "LOOP", control_forth_loop);
    register_word(vm, "(LOOP)", control_forth_runtime_loop);
    register_word(vm, "+LOOP", control_forth_plus_loop);
    register_word(vm, "(+LOOP)", control_forth_runtime_plus_loop);

    register_word(vm, "I", control_forth_i);
    register_word(vm, "J", control_forth_j);
    register_word(vm, "LEAVE", forth_leave);
    register_word(vm, "UNLOOP", forth_unloop);

    /* Execution control */
    register_word(vm, "EXECUTE", control_forth_execute);
    register_word(vm, "ABORT", control_forth_abort);
    register_word(vm, "QUIT", control_forth_quit);

    /* Mark compile-only words as immediate so they execute during compilation */
    DictEntry *if_word = vm_find_word(vm, "IF", 2);
    DictEntry *then_word = vm_find_word(vm, "THEN", 4);
    DictEntry *else_word = vm_find_word(vm, "ELSE", 4);
    DictEntry *begin_word = vm_find_word(vm, "BEGIN", 5);
    DictEntry *until_word = vm_find_word(vm, "UNTIL", 5);
    DictEntry *again_word = vm_find_word(vm, "AGAIN", 5);
    DictEntry *do_word = vm_find_word(vm, "DO", 2);
    DictEntry *loop_word = vm_find_word(vm, "LOOP", 4);
    DictEntry *plus_loop_word = vm_find_word(vm, "+LOOP", 5);

    if (if_word) if_word->flags |= WORD_IMMEDIATE;
    if (then_word) then_word->flags |= WORD_IMMEDIATE;
    if (else_word) else_word->flags |= WORD_IMMEDIATE;
    if (begin_word) begin_word->flags |= WORD_IMMEDIATE;
    if (until_word) until_word->flags |= WORD_IMMEDIATE;
    if (again_word) again_word->flags |= WORD_IMMEDIATE;
    if (do_word) do_word->flags |= WORD_IMMEDIATE;
    if (loop_word) loop_word->flags |= WORD_IMMEDIATE;
    if (plus_loop_word) plus_loop_word->flags |= WORD_IMMEDIATE;

    log_message(LOG_INFO, "Control flow words registered successfully");
}
