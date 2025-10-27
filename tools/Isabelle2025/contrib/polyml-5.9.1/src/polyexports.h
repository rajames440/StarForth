/*
                                  ***   StarForth   ***

  polyexports.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.156-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/polyml-5.9.1/src/polyexports.h
 */

/*
This header contains the structures used in saved state created by "export".
*/

#ifndef _STANDALONE_H
#define _STANDALONE_H 1

// Get time_t
#ifdef HAVE_TIME_H
#include <time.h>
#endif

// Get uintptr_t
#if HAVE_STDINT_H
#  include <stdint.h>
#endif

#if HAVE_INTTYPES_H
#  ifndef __STDC_FORMAT_MACROS
#    define __STDC_FORMAT_MACROS
#  endif
#  include <inttypes.h>
#endif

#ifdef HAVE_STDDEF_H
#  include <stddef.h>
#endif

#if defined(HAVE_WINDOWS_H)
#  include <windows.h>
#endif

// There are several entries 
typedef struct _memTableEntry {
    void *mtCurrentAddr;             // The address of the area of memory
    void *mtOriginalAddr;            // The original address, for saved states and 32-in-64.
    uintptr_t mtLength;              // The length in bytes of the area
    unsigned mtFlags;               // Flags describing the area.
    unsigned mtIndex;               // An index to identify permanent spaces.
} memoryTableEntry;

#define MTF_WRITEABLE         0x00000001  // The area is writeable by ML code
#define MTF_EXECUTABLE        0x00000002  // The area contains executable code
#define MTF_NO_OVERWRITE      0x00000004  // With MTF_WRITEABLE: Don't load over the top
#define MTF_BYTES             0x00000008  // Contains only byte data and no addresses

typedef struct _exportDescription {
    unsigned structLength;         // The length of this structure
    unsigned memTableSize;         // The size of each entry in the memory table
    unsigned memTableEntries;      // The number of entries in the memory table
    memoryTableEntry *memTable;    // Pointer to the memory table.
    void *rootFunction;            // Points to the start-up function
    time_t timeStamp;              // Creation time stamp
    unsigned architecture;         // Machine architecture
    unsigned rtsVersion;           // Run-time system version
    void *originalBaseAddr;        // Original base address (32-in-64 only)
} exportDescription;

extern exportDescription poly_exports;

#ifdef __cplusplus
extern "C" {
#endif

#if (defined(_WIN32))
#include <windows.h>

# ifdef LIBPOLYML_BUILD
#  ifdef DLL_EXPORT
#   define POLYLIB_API            __declspec (dllexport)
#  endif
# elif defined _MSC_VER
    // Visual C - POLYLIB_EXPORTS is defined in the library project settings
#  ifdef POLYLIB_EXPORTS
#   define POLYLIB_API             __declspec (dllexport)
#  else
#   define POLYLIB_API             __declspec (dllimport)
#  endif
# elif defined DLL_EXPORT
#  define POLYLIB_API             __declspec (dllimport)
# else
#  define POLYLIB_API
# endif

extern POLYLIB_API int PolyWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPSTR lpCmdLine, int nCmdShow, exportDescription *exports);
#else
int polymain(int argc, char *argv[], exportDescription *exports);
#endif

#ifdef __cplusplus
};
#endif

#endif
