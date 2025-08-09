#include "vm.h"
#include "log.h"

/* Data stack */
void vm_push(VM *vm, cell_t value) {
    if (vm->dsp >= STACK_SIZE - 1) {
        log_message(LOG_ERROR, "Stack overflow");
        vm->error = 1;
        return;
    }
    vm->data_stack[++vm->dsp] = value;
    log_message(LOG_DEBUG, "PUSH: %d (dsp=%d)", (int)value, vm->dsp);
}

cell_t vm_pop(VM *vm) {
    if (vm->dsp < 0) {
        log_message(LOG_ERROR, "Stack underflow (dsp=%d)", vm->dsp);
        vm->error = 1;
        return 0;
    }
    cell_t value = vm->data_stack[vm->dsp--];
    log_message(LOG_DEBUG, "POP: %d (dsp=%d)", (int)value, vm->dsp);
    return value;
}

/* Return stack */
void vm_rpush(VM *vm, cell_t value) {
    if (vm->rsp >= STACK_SIZE - 1) {
        vm->error = 1;  /* Return stack overflow */
        return;
    }
    vm->return_stack[++vm->rsp] = value;
}

cell_t vm_rpop(VM *vm) {
    if (vm->rsp < 0) {
        vm->error = 1;  /* Return stack underflow */
        return 0;
    }
    cell_t value = vm->return_stack[vm->rsp--];
    return value;
}

