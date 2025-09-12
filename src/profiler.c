/*

                                 ***   StarForth   ***
  profiler.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 9:03 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../include/profiler.h"
#include <stdio.h>

/* Global profiler stub */
void *g_profiler = (void *) 0;

void profiler_shutdown(void) {
}

void profiler_generate_report(void) {
    printf("Profiler: Report generation not yet implemented\n");
}

void profiler_inc_stack_op(void) {
    /* Stub - do nothing */
}