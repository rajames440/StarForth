#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([veriT], [m4_esyscmd_s([scripts/version.sh])], [verit-bugs@lists.gforge.inria.fr])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_AUX_DIR([scripts])
AM_INIT_AUTOMAKE([subdir-objects foreign 1.13])
AM_SILENT_RULES([yes])

AC_REQUIRE_AUX_FILE([tap-driver.sh])

AC_CONFIG_SRCDIR([src/response.h])
# Checks C compiler (with empty default flags).
: ${CFLAGS=""}
AC_PROG_CC
# TODO: Make standard compatible
#AX_CHECK_COMPILE_FLAG([-std=c11],
#    [ CFLAGS+=" -std=c11" ],
#    [ AC_MSG_ERROR([C compiler doesn't support C11 mode])] )

# Check programs
AC_PROG_LEX
AC_PROG_AWK
AX_PROG_BISON([],[])
AC_PATH_PROG(PROG_SED, sed, no)

# Checks for libraries.
AC_CHECK_LIB(gmp, __gmpz_init,, [AC_MSG_ERROR([libgmp not found.])])

AC_CHECK_FUNCS(setitimer, [posix_timer=yes], [])
if test x$posix_timer = xyes; then
  AC_DEFINE([HAVE_POSIX_TIMER], [1], [POSIX timers exist])
fi

AC_CHECK_FUNCS(sigaction, [have_sigaction=yes], [])

# Check if we need to link with -lm
AC_SEARCH_LIBS([sqrt], [m])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stddef.h stdint.h stdlib.h string.h sys/resource.h sys/time.h unistd.h windows.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
# We do not check for the presence of malloc, realloc, because autoconf
# replaces them with rpl_malloc in some cases and we don't want that.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset mkdir pow strchr strdup strstr strtol])

# User defined options: static, debug, lto
AC_ARG_ENABLE([static],
    [AS_HELP_STRING([--enable-static],[static linking])],
    [static=${enableval}], [static=no])
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],[compile with debug options])],
    [debug=${enableval}], [debug=no])
AC_ARG_ENABLE([lto],
    [AS_HELP_STRING([--disable-lto], [disable link time optimization])],
    [lto=${enableval}], [lto=yes])
AC_ARG_ENABLE([signal-capture],
    [AS_HELP_STRING([--disable-signal-capture],[disable the capturing of system signals])],
    [signal_capture=${enableval}], [signal_capture=yes])

if test x$signal_capture = xyes; then
    if test x$have_sigaction = xyes; then
        AC_DEFINE([CAPTURE_SIGNALS], [1], [Capture system signals])
    else
        AC_MSG_NOTICE([Signal capturing disabled because sigaction is missing])
    fi
else
    AC_MSG_NOTICE([Signal capturing disabled by user])
fi

AC_MSG_NOTICE([Setting up compiler flags])
CFLAGS+=" -Wall -Wno-unused-function"

if test x$static = xyes; then
    LDFLAGS+=" -static"
fi

if test x$debug = xyes; then
    CFLAGS+=" -O0"
    CFLAGS+=" -g -g3 -gdwarf-2"
    CFLAGS+=" -ftrapv"
    CPPFLAGS+=" -DDEBUG"
    YFLAGS+=" --debug -v"
    lto=no
else
    CFLAGS+=" -O3"
    CPPFLAGS+=" -DNDEBUG"
    CFLAGS+=" -fomit-frame-pointer"
    AX_CHECK_COMPILE_FLAG([-finline-limit=1000000],
        [ CFLAGS+=" -finline-limit=1000000" ], ,[-Werror])
fi

if test x$lto = xyes; then
    AX_CHECK_COMPILE_FLAG([-flto],
        [ CFLAGS+=" -flto" ],
        [ AC_MSG_WARN([C compiler doesn't support link time optimization])], [-Werror])
fi

AC_CONFIG_FILES([Makefile src/Makefile test/Makefile])
AC_CONFIG_HEADERS([src/config.h])

AC_OUTPUT

AS_ECHO("")
AS_ECHO("")
AS_ECHO("Configuration summary")
AS_ECHO("CC = $CC")
AS_ECHO("CFLAGS = $CFLAGS")
AS_ECHO("CPPLAGS = $CPPFLAGS")
AS_ECHO("YFLAGS = $YFLAGS")
AS_ECHO("LDFLAGS = $LDFLAGS")
AS_ECHO("LIBS = $LIBS")
