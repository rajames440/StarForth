#ifndef IO_H
#define IO_H

#include <stdint.h>
#include "vm.h"

#define BLOCK_SIZE 1024
#define BLOCK_COUNT 1024
#define MEMORY_SIZE (BLOCK_SIZE * BLOCK_COUNT)

void io_init(VM *vm);

int io_read_block(VM *vm, int block_num, unsigned char *buffer);
int io_write_block(VM *vm, int block_num, const unsigned char *buffer);

#endif /* IO_H */
