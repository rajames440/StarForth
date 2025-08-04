/*

  * Copyright (c) 2025 Robert A. James - StarshipOS Forth Project.
  *
  * This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  * To the extent possible under law, the author(s) have dedicated all copyright and related
  * and neighboring rights to this software to the public domain worldwide.
  * This software is distributed without any warranty.
  *
  * See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

 */

#ifndef BLOCK_WORDS_H
#define BLOCK_WORDS_H

#include "../../../include/vm.h"

/* FORTH-79 Block & Mass Storage Words:
 * BLOCK          ( n -- addr )          Get block buffer address
 * BUFFER         ( n -- addr )          Get empty block buffer  
 * UPDATE         ( -- )                 Mark current block as modified
 * FLUSH          ( -- )                 Write all modified buffers
 * EMPTY-BUFFERS  ( -- )                 Mark all buffers as empty
 * SAVE-BUFFERS   ( -- )                 Write all modified buffers
 * BLK            ( -- addr )            Variable: current block number
 * SCR            ( -- addr )            Variable: screen being edited
 * LOAD           ( n -- )               Load and interpret block
 * THRU           ( n1 n2 -- )           Load range of blocks n1 through n2
 * -->            ( -- )                 Continue loading from next block
 * LIST           ( n -- )               Display block contents
 * FIRST          ( -- addr )            Address of first block buffer
 * USE            ( -- addr )            Address of next buffer to use
 * #BLK           ( -- n )               Constant: number of blocks available
 */

void register_block_words(VM *vm);

#endif /* BLOCK_WORDS_H */
