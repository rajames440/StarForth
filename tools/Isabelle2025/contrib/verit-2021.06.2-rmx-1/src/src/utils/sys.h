/**
   \file sys.h

   \brief Provides functions to access some system resources.
*/

#ifndef __SYS_H
#define __SYS_H

#include <stdio.h>

/**
   \brief exit status codes */
typedef enum Tsys_exit_code
{
	SYS_EXIT_VALID = 0,
	SYS_EXIT_COUNTERSATISFIABLE,
	SYS_EXIT_DONT_KNOW,
	SYS_EXIT_CONFLICTING_AXIOMS,
	SYS_EXIT_TIMEOUT,
	SYS_EXIT_RESOURCES_EXHAUSTED,
	SYS_EXIT_ERROR_USAGE,
	SYS_EXIT_ERROR_IO,
	SYS_EXIT_ERROR_UNKNOWN
} Tsys_exit_code;

/**
   \brief opens a file
   \param name the file name
   \param mode access mode (as defined in C)
   \return a pointer to this file */
extern FILE* sys_open_file(const char* name, const char* mode);

/**
   \brief Closes a file
   \param file the file to close */
extern void sys_close_file(FILE* file);

#endif /* __SYS_H */
