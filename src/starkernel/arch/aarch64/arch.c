/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

 See the License for the specific language governing permissions and
 limitations under the License.

 StarForth — Steady-State Virtual Machine Runtime
  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.

 */

/**
 * arch.c (aarch64) - Architecture abstractions for StarKernel on ARM64
 */

#include "arch.h"

void arch_early_init(void)
{
    /* Placeholder for exception level transitions and vector setup */
}

void arch_enable_interrupts(void)
{
    __asm__ volatile ("msr daifclr, #2" ::: "memory");
}

void arch_disable_interrupts(void)
{
    __asm__ volatile ("msr daifset, #2" ::: "memory");
}

void arch_halt(void)
{
    __asm__ volatile ("wfi");
}

uint64_t arch_read_timestamp(void)
{
    uint64_t cnt;
    __asm__ volatile ("mrs %0, cntpct_el0" : "=r"(cnt));
    return cnt;
}

void arch_mmu_init(void)
{
    /* Stub: page table setup will land in later milestones */
}
