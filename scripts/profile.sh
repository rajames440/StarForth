#!/bin/bash
#
#
#                                 ***   StarForth   ***
#  profile.sh - FORTH-79 Standard and ANSI C99 ONLY
# Last modified - 8/14/25, 10:29 AM
#  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
# This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#  To the extent possible under law, the author(s) have dedicated all copyright and related
#  and neighboring rights to this software to the public domain worldwide.
#  This software is distributed without any warranty.
#
#  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#
#

make clean
make CFLAGS='-std=c99 -O3 -march=native -flto -DNDEBUG -g -Wall -Wextra -Iinclude -Isrc/word_source -Isrc/test_runner/include' LDFLAGS='-flto'

prlimit --nofile=4096:4096 -- valgrind --tool=callgrind --callgrind-out-file=callgrind.out.sf ./build/starforth --run-tests

callgrind_annotate --auto=yes --threshold=1 callgrind.out.sf | head -100 | tee profile.txt


