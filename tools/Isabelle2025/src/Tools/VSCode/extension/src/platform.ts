/*
 *                                  ***   StarForth   ***
 *
 *  platform.ts- FORTH-79 Standard and ANSI C99 ONLY
 *  Modified by - rajames
 *  Last modified - 2025-10-27T12:40:04.142-04
 *
 *  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
 *
 *  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
 *  To the extent possible under law, the author(s) have dedicated all copyright and related
 *  and neighboring rights to this software to the public domain worldwide.
 *  This software is distributed without any warranty.
 *
 *  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
 *
 *  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/src/Tools/VSCode/extension/src/platform.ts
 */

/*  Author:     Makarius

System platform identification (see Pure/System/platform.scala).
*/

'use strict';

import * as os from 'os'


/* platform family */

export function is_windows(): boolean
{
  return os.type().startsWith("Windows")
}

export function is_linux(): boolean
{
  return os.type().startsWith("Linux")
}

export function is_macos(): boolean
{
  return os.type().startsWith("Darwin")
}

export function is_unix(): boolean
{
  return is_linux() || is_macos()
}
