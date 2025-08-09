#include "vm_api.h"
#include "log.h"
#include <string.h>

/* helpers */
static inline int in_bounds_range(size_t base, size_t len, size_t off, size_t n) {
    return off <= len && n <= len - off;
}

cell_t vm_off(VM *vm, const void *p) {
    if (!vm || !vm->memory || !p) return -1;
    const uint8_t *bp = (const uint8_t *)vm->memory;
    const uint8_t *pp = (const uint8_t *)p;
    if (pp < bp || pp >= bp + VM_MEMORY_SIZE) return -1;
    return (cell_t)(pp - bp);
}

uint8_t* vm_addr(VM *vm, cell_t off) {
    if (!vm || !vm->memory) return NULL;
    if (off < 0) return NULL;
    size_t o = (size_t)off;
    if (o >= VM_MEMORY_SIZE) return NULL;
    return vm->memory + o;
}

/* cell ops (aligned) */
int vm_api_store_cell(VM *vm, cell_t addr, cell_t x) {
    uint8_t *p = vm_addr(vm, addr);
    if (!p) goto bounds_err;
    if (((uintptr_t)p % sizeof(cell_t)) != 0) goto align_err;
    if (!in_bounds_range(0, VM_MEMORY_SIZE, (size_t)(p - vm->memory), sizeof(cell_t))) goto bounds_err;
    *(cell_t*)p = x;
    return 0;
align_err:
    log_message(LOG_ERROR, "vm_api_store_cell: alignment error @%ld", (long)addr);
    vm->error = 1; return -1;
bounds_err:
    log_message(LOG_ERROR, "vm_api_store_cell: bounds error @%ld", (long)addr);
    vm->error = 1; return -1;
}

int vm_api_fetch_cell(VM *vm, cell_t addr, cell_t *out) {
    if (!out) { vm->error = 1; return -1; }
    uint8_t *p = vm_addr(vm, addr);
    if (!p) goto bounds_err;
    if (((uintptr_t)p % sizeof(cell_t)) != 0) goto align_err;
    if (!in_bounds_range(0, VM_MEMORY_SIZE, (size_t)(p - vm->memory), sizeof(cell_t))) goto bounds_err;
    *out = *(cell_t*)p;
    return 0;
align_err:
    log_message(LOG_ERROR, "vm_api_fetch_cell: alignment error @%ld", (long)addr);
    vm->error = 1; return -1;
bounds_err:
    log_message(LOG_ERROR, "vm_api_fetch_cell: bounds error @%ld", (long)addr);
    vm->error = 1; return -1;
}

int vm_api_add_cell(VM *vm, cell_t addr, cell_t delta) {
    uint8_t *p = vm_addr(vm, addr);
    if (!p) goto bounds_err;
    if (((uintptr_t)p % sizeof(cell_t)) != 0) goto align_err;
    if (!in_bounds_range(0, VM_MEMORY_SIZE, (size_t)(p - vm->memory), sizeof(cell_t))) goto bounds_err;
    *(cell_t*)p += delta;
    return 0;
align_err:
    log_message(LOG_ERROR, "vm_api_add_cell: alignment error @%ld", (long)addr);
    vm->error = 1; return -1;
bounds_err:
    log_message(LOG_ERROR, "vm_api_add_cell: bounds error @%ld", (long)addr);
    vm->error = 1; return -1;
}

/* byte ops */
int vm_api_store_byte(VM *vm, cell_t addr, uint8_t b) {
    uint8_t *p = vm_addr(vm, addr);
    if (!p) { log_message(LOG_ERROR, "vm_api_store_byte: bounds @%ld", (long)addr); vm->error=1; return -1; }
    *p = b; return 0;
}

int vm_api_fetch_byte(VM *vm, cell_t addr, uint8_t *out) {
    if (!out) { vm->error = 1; return -1; }
    uint8_t *p = vm_addr(vm, addr);
    if (!p) { log_message(LOG_ERROR, "vm_api_fetch_byte: bounds @%ld", (long)addr); vm->error=1; return -1; }
    *out = *p; return 0;
}

int vm_api_fill(VM *vm, cell_t addr, size_t n, uint8_t b) {
    if (n == 0) return 0;
    uint8_t *p = vm_addr(vm, addr);
    if (!p) { log_message(LOG_ERROR, "vm_api_fill: bounds @%ld", (long)addr); vm->error=1; return -1; }
    size_t off = (size_t)(p - vm->memory);
    if (!in_bounds_range(0, VM_MEMORY_SIZE, off, n)) {
        log_message(LOG_ERROR, "vm_api_fill: range exceeds memory @%ld..+%zu", (long)addr, n);
        vm->error=1; return -1;
    }
    memset(p, b, n); return 0;
}

int vm_api_erase(VM *vm, cell_t addr, size_t n) {
    return vm_api_fill(vm, addr, n, 0u);
}
