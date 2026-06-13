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

#ifndef CAPSULE_WORDS_H
#define CAPSULE_WORDS_H

#include "../../../include/vm.h"

/**
 * @brief Register all capsule and SDK bridge words with the VM.
 *
 * Registers:
 *   EXEC         ( c-addr u -- )  Load and execute a capsule .4th file
 *   L8-UPDATE    ( ent cv tmp stb -- )  Feed Q48.16 metrics into L8 selector
 *   L8-APPLY     ( -- )           Apply current L8 mode to ssm_config
 *   L8-MODE      ( -- mode )      Push current L8 mode index (0-15)
 *   INFER-RUN    ( -- )           Run one inference engine pass
 *   WINDOW-DIVERSITY ( -- q )     Rolling-window diversity as Q48.16
 *   INFER-WINDOW@    ( -- n )     Last inferred window width
 *   INFER-DECAY@     ( -- q )     Last inferred decay slope (Q48.16)
 *   INFER-VARIANCE@  ( -- q )     Last inferred window variance (Q48.16)
 *   INFER-EARLY-EXIT@ ( -- flag ) Last early-exit flag (0 or 1)
 *   BAYES-CACHE-MEAN ( -- q )     Hot-words cache prefetch accuracy (Q48.16)
 *   BAYES-BUCKET-MEAN ( -- q )    Rolling-window bucket frequency mean (Q48.16)
 *   INFER-FIT@       ( -- q )     Last inference fit quality (Q48.16)
 */
void register_capsule_words(VM *vm);

#endif /* CAPSULE_WORDS_H */
