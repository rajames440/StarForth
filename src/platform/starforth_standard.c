/*

                                 ***   StarForth   ***
  starforth_standard.c - FORTH-79 Standard and ANSI C99 ONLY
 Last modified - 8/14/25, 8:49 PM
  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

 This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.


 */

#include "../../include/platform/starforth_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

/* Standard platform implementation */

/* Function implementations for platform abstraction that need special handling */
void sf_exit(int status) {
    exit(status);
}

void sf_sleep(unsigned seconds) {
#if defined(_WIN32)
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

void sf_sigemptyset(void *set) {
#if defined(__unix__) || defined(__APPLE__)
    sigemptyset((sigset_t *) set);
#else
    memset(set, 0, sizeof(sigset_t));
#endif
}

void sf_sigfillset(void *set) {
#if defined(__unix__) || defined(__APPLE__)
    sigfillset((sigset_t *) set);
#else
    memset(set, 0xFF, sizeof(sigset_t));
#endif
}

void sf_sigaddset(void *set, int signum) {
#if defined(__unix__) || defined(__APPLE__)
    sigaddset((sigset_t *) set, signum);
#else
    /* Basic implementation for non-POSIX platforms */
    if (signum >= 0 && signum < 32) {
        *((unsigned int *) set) |= (1U << signum);
    }
#endif
}

void sf_sigdelset(void *set, int signum) {
#if defined(__unix__) || defined(__APPLE__)
    sigdelset((sigset_t *) set, signum);
#else
    /* Basic implementation for non-POSIX platforms */
    if (signum >= 0 && signum < 32) {
        *((unsigned int *) set) &= ~(1U << signum);
    }
#endif
}

int sf_sigismember(const void *set, int signum) {
#if defined(__unix__) || defined(__APPLE__)
    return sigismember((const sigset_t *) set, signum);
#else
    /* Basic implementation for non-POSIX platforms */
    if (signum < 0 || signum >= 32) return 0;
    return (*((unsigned int *) set) & (1U << signum)) ? 1 : 0;
#endif
}

int sf_sigaction(int signum, const sf_sigaction_t *act, sf_sigaction_t *oldact) {
#if defined(__unix__) || defined(__APPLE__)
    return sigaction(signum, (const struct sigaction *) act, (struct sigaction *) oldact);
#else
    /* Basic implementation for non-POSIX platforms */
    if (oldact) {
        oldact->sa_handler = signal(signum, SIG_IGN);
        signal(signum, oldact->sa_handler);
    }
    if (act) {
        signal(signum, act->sa_handler);
    }
    return 0;
#endif
}

char *sf_itoa(int value, char *str, int base) {
    /* itoa is not standard, so we implement it */
    if (base < 2 || base > 36 || str == NULL) {
        return NULL;
    }

    int i = 0;
    int neg = 0;

    /* Handle negative numbers */
    if (value < 0 && base == 10) {
        neg = 1;
        value = -value;
    }

    /* Handle special case of zero */
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    /* Convert to specified base */
    while (value != 0) {
        int remainder = value % base;
        str[i++] = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
        value /= base;
    }

    /* Add negative sign if needed */
    if (neg) {
        str[i++] = '-';
    }

    /* Null-terminate the string */
    str[i] = '\0';

    /* Reverse the string */
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}