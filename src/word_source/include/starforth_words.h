/*

                                 ***   StarForth   ***
  starforth_words.h - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/15/25, 9:11 AM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#ifndef STARFORTH_WORDS_H
#define STARFORTH_WORDS_H

#include "../../../include/vm.h"

/* StarForth Implementation Vocabulary Words */
void starforth_word_entropy_fetch(VM * vm); /* ENTROPY@ ( addr -- n ) */
void starforth_word_entropy_store(VM * vm); /* ENTROPY! ( n addr -- ) */
void starforth_word_word_entropy(VM * vm); /* WORD-ENTROPY ( -- ) */
void starforth_word_reset_entropy(VM * vm); /* RESET-ENTROPY ( -- ) */
void starforth_word_top_words(VM * vm); /* TOP-WORDS ( n -- ) */

/* Registration function */
void register_starforth_words(VM * vm);

#endif /* STARFORTH_WORDS_H */