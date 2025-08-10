/*

                                 ***   StarForth   ***
  control_words.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
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

/* --- SAFETY WRAPPERS: return stack --- */
static inline int rs_push(VM *vm, cell_t v) {
    if (vm->rsp + 1 >= RETURN_STACK_SIZE) {
        vm->error = 1;
        log_message(LOG_ERROR, "RSP overflow (rsp=%d)", vm->rsp);
        return 0;
    }
    vm->return_stack[++vm->rsp] = v;
    return 1;
}
static inline int rs_pop(VM *vm, cell_t *out) {
    if (vm->rsp < 0) {
        vm->error = 1;
        log_message(LOG_ERROR, "RSP underflow (rsp=%d)", vm->rsp);
        return 0;
    }
    *out = vm->return_stack[vm->rsp--];
    return 1;
}

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
static void loop_push(cell_t limit, cell_t index, void *ip) {
    if (loop_sp >= LOOP_STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Loop stack overflow");
        return;
    }
    loop_sp++;
    loop_stack[loop_sp].limit = limit;
    loop_stack[loop_sp].index = index;
    loop_stack[loop_sp].loop_start_ip = ip;
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

/* 0BRANCH ( flag -- ) Conditional branch - runtime primitive */
static void control_forth_0branch(VM *vm) {
    cell_t *ip;
    cell_t offset, flag;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "0BRANCH: Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Get current instruction pointer */
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    /* Read flag from data stack */
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "0BRANCH: Data stack underflow");
        vm->error = 1;
        return;
    }
    flag = vm_pop(vm);

    if (flag == 0) {
        /* Read branch offset and update IP */
        offset = *ip;
        ip = (cell_t*)((char*)ip + offset);
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "0BRANCH: Condition true, branched by %ld", (long)offset);
    } else {
        /* Skip over the offset */
        ip++;
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "0BRANCH: Condition false, no branch");
    }
}

/* (LIT) ( -- n ) Push literal - runtime primitive */
static void control_forth_lit(VM *vm) {
    cell_t *ip;
    cell_t literal_value;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "(LIT): Return stack underflow");
        vm->error = 1;
        return;
    }

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
    if (!rs_push(vm, (cell_t)(placeholder_addr))) { return; }

    log_message(LOG_DEBUG, "IF: Compiled 0BRANCH with placeholder at %zu", placeholder_addr);
}

/* ELSE ( -- ) Alternate execution - compile-time */
static void control_forth_else(VM *vm) {
    cell_t placeholder_addr;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "ELSE: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Compile BRANCH to skip the ELSE part */
    vm_compile_call(vm, control_forth_branch);

    /* Reserve space for branch offset */
    size_t new_placeholder = vm->here;
    vm_compile_literal(vm, 0);

    /* Patch previous 0BRANCH */
    if (!rs_pop(vm, &placeholder_addr)) { return; }

    cell_t branch_offset = (cell_t)(vm->here - (placeholder_addr + sizeof(cell_t)));
    vm_patch_literal(vm, (size_t)placeholder_addr, branch_offset);

    /* Push new placeholder for THEN */
    if (!rs_push(vm, (cell_t)(new_placeholder))) { return; }

    log_message(LOG_DEBUG, "ELSE: Patched 0BRANCH at %ld, new placeholder at %zu",
                (long)placeholder_addr, new_placeholder);
}

/* THEN ( -- ) End conditional - compile-time */
static void control_forth_then(VM *vm) {
    cell_t placeholder_addr;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "THEN: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Patch previous branch to here */
    if (!rs_pop(vm, &placeholder_addr)) { return; }

    cell_t branch_offset = (cell_t)(vm->here - (placeholder_addr + sizeof(cell_t)));
    vm_patch_literal(vm, (size_t)placeholder_addr, branch_offset);

    log_message(LOG_DEBUG, "THEN: Patched branch at %ld to here %zu",
                (long)placeholder_addr, vm->here);
}

/* BEGIN ( -- ) Start loop - compile-time */
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
    if (!rs_push(vm, (cell_t)(vm->here))) { return; }

    log_message(LOG_DEBUG, "BEGIN: Loop target at %zu", vm->here);
}

/* UNTIL ( flag -- ) End loop if flag true - compile-time */
static void control_forth_until(VM *vm) {
    cell_t begin_addr,  branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "UNTIL: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (!rs_pop(vm, &begin_addr)) { return; }

    /* Compile 0BRANCH back to BEGIN */
    vm_compile_call(vm, control_forth_0branch);

    branch_offset = begin_addr - vm->here - sizeof(cell_t);

    vm_compile_literal(vm, branch_offset);
    log_message(LOG_DEBUG, "UNTIL: Patched to branch back to %ld with offset %ld",
                (long)begin_addr, (long)branch_offset);
}

/* AGAIN ( -- ) Infinite loop - compile-time */
static void control_forth_again(VM *vm) {
    cell_t begin_addr,  branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "AGAIN: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (!rs_pop(vm, &begin_addr)) { return; }

    /* Compile BRANCH back to BEGIN */
    vm_compile_call(vm, control_forth_branch);

    branch_offset = begin_addr - vm->here - sizeof(cell_t);

    vm_compile_literal(vm, branch_offset);
    log_message(LOG_DEBUG, "AGAIN: Patched to branch back to %ld with offset %ld",
                (long)begin_addr, (long)branch_offset);
}

/* DO ( limit index -- ) Start counted loop - compile-time */
static void control_forth_do(VM *vm) {
    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "DO: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    /* Compile runtime DO primitive */
    vm_compile_call(vm, control_forth_runtime_do);

    /* Push placeholder for loop start address */
    if (!rs_push(vm, (cell_t)(vm->here))) { return; }

    log_message(LOG_DEBUG, "DO: Compiled runtime DO, placeholder at %zu", vm->here);
}

/* LOOP ( -- ) End counted loop - compile-time */
static void control_forth_loop(VM *vm) {
    cell_t do_addr,  branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "LOOP: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (!rs_pop(vm, &do_addr)) { return; }

    /* Compile runtime LOOP primitive */
    vm_compile_call(vm, control_forth_runtime_loop);

    /* Back-branch to DO */
    branch_offset = do_addr - vm->here - sizeof(cell_t);

    vm_compile_literal(vm, branch_offset);
    log_message(LOG_DEBUG, "LOOP: Patched to branch back to %ld with offset %ld",
                (long)do_addr, (long)branch_offset);
}

/* +LOOP ( n -- ) End counted loop with increment - compile-time */
static void control_forth_plus_loop(VM *vm) {
    cell_t do_addr,  branch_offset;

    if (vm->mode != MODE_COMPILE) {
        log_message(LOG_ERROR, "+LOOP: Only valid in compile mode");
        vm->error = 1;
        return;
    }

    if (!rs_pop(vm, &do_addr)) { return; }

    /* Compile runtime +LOOP primitive */
    vm_compile_call(vm, control_forth_runtime_plus_loop);

    /* Back-branch to DO */
    branch_offset = do_addr - vm->here - sizeof(cell_t);

    vm_compile_literal(vm, branch_offset);
    log_message(LOG_DEBUG, "+LOOP: Patched to branch back to %ld with offset %ld",
                (long)do_addr, (long)branch_offset);
}

/* Runtime DO primitive:captures limit and index, stores loop start IP */
static void control_forth_runtime_do(VM *vm) {
    cell_t *ip;
    cell_t limit, index;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime DO: Return stack underflow");
        vm->error = 1;
        return;
    }

    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    /* Pop limit and index from the data stack */
    if (vm->dsp < 1) {
        log_message(LOG_ERROR, "Runtime DO: Data stack underflow");
        vm->error = 1;
        return;
    }

    index = vm_pop(vm);
    limit = vm_pop(vm);

    /* Push loop parameters onto loop stack */
    loop_push(limit, index, ip);

    log_message(LOG_DEBUG, "Runtime DO: index=%ld limit=%ld", (long)index, (long)limit);
}

/* Runtime LOOP primitive: increments index, checks limit, branches back */
static void control_forth_runtime_loop(VM *vm) {
    cell_t *ip;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime LOOP: Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Get current IP */
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Runtime LOOP: Loop stack underflow");
        vm->error = 1;
        return;
    }

    /* Increment index */
    loop_stack[loop_sp].index++;

    /* Check if loop should continue */
    if (loop_stack[loop_sp].index < loop_stack[loop_sp].limit) {
        /* Branch back to loop start */
        ip = (cell_t*)loop_stack[loop_sp].loop_start_ip;
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime LOOP: Continue, index=%ld", (long)loop_stack[loop_sp].index);
    } else {
        /* Exit loop */
        loop_pop();
        ip++; /* Skip over the runtime LOOP primitive in code stream */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime LOOP: Exit");
    }
}

/* Runtime +LOOP primitive: adds N to index and checks limit, branches back */
static void control_forth_runtime_plus_loop(VM *vm) {
    cell_t *ip;
    cell_t n;

    if (vm->rsp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: Return stack underflow");
        vm->error = 1;
        return;
    }

    /* Get current IP */
    ip = (cell_t*)(uintptr_t)vm->return_stack[vm->rsp];

    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: Data stack underflow");
        vm->error = 1;
        return;
    }
    n = vm_pop(vm);

    if (loop_sp < 0) {
        log_message(LOG_ERROR, "Runtime +LOOP: Loop stack underflow");
        vm->error = 1;
        return;
    }

    /* Update index */
    loop_stack[loop_sp].index += n;

    /* Check if loop should continue */
    if ((n > 0 && loop_stack[loop_sp].index < loop_stack[loop_sp].limit) ||
        (n < 0 && loop_stack[loop_sp].index > loop_stack[loop_sp].limit)) {
        /* Branch back to loop start */
        ip = (cell_t*)loop_stack[loop_sp].loop_start_ip;
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: Continue, index=%ld", (long)loop_stack[loop_sp].index);
    } else {
        /* Exit loop */
        loop_pop();
        ip++; /* Skip over the runtime +LOOP primitive */
        vm->return_stack[vm->rsp] = (cell_t)(uintptr_t)ip;
        log_message(LOG_DEBUG, "Runtime +LOOP: Exit");
    }
}

/* Registration of control flow words */
void register_control_words(VM *vm) {
    register_word(vm, "BRANCH", control_forth_branch);
    register_word(vm, "0BRANCH", control_forth_0branch);
    register_word(vm, "(LIT)", control_forth_lit);

    register_word(vm, "IF", control_forth_if);          vm_make_immediate(vm);
    register_word(vm, "ELSE", control_forth_else);      vm_make_immediate(vm);
    register_word(vm, "THEN", control_forth_then);      vm_make_immediate(vm);

    register_word(vm, "BEGIN", control_forth_begin);    vm_make_immediate(vm);
    register_word(vm, "UNTIL", control_forth_until);    vm_make_immediate(vm);
    register_word(vm, "AGAIN", control_forth_again);    vm_make_immediate(vm);

    register_word(vm, "DO", control_forth_do);          vm_make_immediate(vm);
    register_word(vm, "LOOP", control_forth_loop);      vm_make_immediate(vm);
    register_word(vm, "+LOOP", control_forth_plus_loop);vm_make_immediate(vm);

    log_message(LOG_INFO, "Registering FORTH-79 control flow words...");
    log_message(LOG_INFO, "Control flow words registered successfully");
}
