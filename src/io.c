/*

                                 ***   StarForth ***
  io.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/9/25, 1:07 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "io.h"
#include "log.h"

/* Custom memset/memcpy - no string.h dependency */
static void io_memset(void *ptr, int value, size_t num) {
    unsigned char *p = (unsigned char*)ptr;
    size_t i;
    for (i = 0; i < num; i++) {
        p[i] = (unsigned char)value;
    }
}

static void io_memcpy(void *dest, const void *src, size_t num) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    size_t i;
    for (i = 0; i < num; i++) {
        d[i] = s[i];
    }
}

/* Private static block buffer */
static unsigned char blocks[MEMORY_SIZE];

void io_init(VM *vm) {
    vm->blocks = blocks;  /* Connect VM to our static buffer */
    io_memset(blocks, 0, MEMORY_SIZE);
    log_message(LOG_INFO, "IO subsystem initialized with %d blocks of %d bytes", BLOCK_COUNT, BLOCK_SIZE);
}

int io_read_block(VM *vm, int block_num, unsigned char *buffer) {
    (void)vm;
    if (block_num < 0 || block_num >= BLOCK_COUNT) {
        log_message(LOG_ERROR, "io_read_block: invalid block number %d", block_num);
        return -1;
    }
    io_memcpy(buffer, &blocks[block_num * BLOCK_SIZE], BLOCK_SIZE);
    log_message(LOG_DEBUG, "io_read_block: read block %d", block_num);
    return 0;
}

int io_write_block(VM *vm, int block_num, const unsigned char *buffer) {
    (void)vm;
    if (block_num < 0 || block_num >= BLOCK_COUNT) {
        log_message(LOG_ERROR, "io_write_block: invalid block number %d", block_num);
        return -1;
    }
    io_memcpy(&blocks[block_num * BLOCK_SIZE], buffer, BLOCK_SIZE);
    log_message(LOG_DEBUG, "io_write_block: wrote block %d", block_num);
    return 0;
}