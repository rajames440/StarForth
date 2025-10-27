/*
                                  ***   StarForth   ***

  inums.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:03.567-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/rsync-3.2.7-1/src/inums.h
 */

static inline char *
big_num(int64 num)
{
	return do_big_num(num, 0, NULL);
}

static inline char *
comma_num(int64 num)
{
	extern int human_readable;
	return do_big_num(num, human_readable != 0, NULL);
}

static inline char *
human_num(int64 num)
{
	extern int human_readable;
	return do_big_num(num, human_readable, NULL);
}

static inline char *
big_dnum(double dnum, int decimal_digits)
{
	return do_big_dnum(dnum, 0, decimal_digits);
}

static inline char *
comma_dnum(double dnum, int decimal_digits)
{
	extern int human_readable;
	return do_big_dnum(dnum, human_readable != 0, decimal_digits);
}

static inline char *
human_dnum(double dnum, int decimal_digits)
{
	extern int human_readable;
	return do_big_dnum(dnum, human_readable, decimal_digits);
}
