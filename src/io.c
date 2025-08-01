#include "io.h"
#include "log.h"
#include <string.h>

/* Private static block buffer */
static unsigned char blocks[MEMORY_SIZE];

void io_init(VM *vm) {
    (void)vm;  /* vm unused */
    memset(blocks, 0, MEMORY_SIZE);
    log_message(LOG_INFO, "IO subsystem initialized with %d blocks of %d bytes", BLOCK_COUNT, BLOCK_SIZE);
}

int io_read_block(VM *vm, int block_num, unsigned char *buffer) {
    (void)vm;
    if (block_num < 0 || block_num >= BLOCK_COUNT) {
        log_message(LOG_ERROR, "io_read_block: invalid block number %d", block_num);
        return -1;
    }
    memcpy(buffer, &blocks[block_num * BLOCK_SIZE], BLOCK_SIZE);
    log_message(LOG_DEBUG, "io_read_block: read block %d", block_num);
    return 0;
}

int io_write_block(VM *vm, int block_num, const unsigned char *buffer) {
    (void)vm;
    if (block_num < 0 || block_num >= BLOCK_COUNT) {
        log_message(LOG_ERROR, "io_write_block: invalid block number %d", block_num);
        return -1;
    }
    memcpy(&blocks[block_num * BLOCK_SIZE], buffer, BLOCK_SIZE);
    log_message(LOG_DEBUG, "io_write_block: wrote block %d", block_num);
    return 0;
}
