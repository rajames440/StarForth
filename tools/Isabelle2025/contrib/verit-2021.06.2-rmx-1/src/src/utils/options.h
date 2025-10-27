/**
   \file options.h

   \brief Generic module for handling options

   The module must be initialised before any other module that declares program
   options and rely on program arguments.

   A set-up function is available to store information about the
   running program (this was available before through the initialization
   function, but that causes problems if code using the options module
   needs to be distributed as a library).

   New options can be declared, with association with variables of the
   corresponding type.  As this is generally done in the init part of
   other modules, this enforces that options_init should be called BEFORE
   any init of module using options.
   Just AFTER declaring an option, a DEFAULT value can be assigned to any
   option variable.
   When every option has been declared (i.e. after all inits of modules
   using options) options_parse is called, with program arguments (as passed
   to main).  Now the value of every option is set.

   Also environment variables corresponding to the long name of
   options, prefixed by env_prefix (string given as argument for
   options_setup), upcased, and with each '-' replaced by '_' are
   automatically bound to those options.

   Such a non-zero environment  variable sets the boolean option.  For
   integer and string options, the interpretation is natural.

   Command line options override environment variables.
   Both override default values.

   Names for options should use "-" rather than "_"

   When the command line is parsed, the table passed as argument to
   options_setup, is filled with all the arguments that do not
   correspond to options (i.e. do not follow a short option or long
   follow with argument). */

#ifndef __OPTIONS_H
#define __OPTIONS_H

#include "utils/stack.h"

#include <stdbool.h>
#include <stdio.h>

/* PF Do we use argp for parsing options?
   ARGP is not available everywhere. */
/* #define WITH_ARGP */

/**
   \brief declares boolean option to store in *aint with short name, long name,
   description (doc)
   \param short_name char with option short name
   \param long_name pointer to string of option long name
   \param doc pointer to string documenting option
   \param abool pointer to a bool where the value of the argument is stored */
void options_new(char short_name, char* long_name, char* doc, bool* abool);

/**
   \brief declares integer option to store in *aint with short name, long name,
   description (doc)
   \param short_name char with option short name
   \param long_name pointer to string of option long name
   \param doc pointer to string documenting option
   \param arg pointer to string explaining authorised values
   \param aint pointer to an int where the value of the argument is stored */
void options_new_int(
	char short_name, char* long_name, char* doc, char* arg, int* aint);

/**
   \brief declares integer option to store in *aint with short name, long name,
   description (doc), and defaut value
   \param short_name char with option short name
   \param long_name pointer to string of option long name
   \param doc pointer to string documenting option
   \param arg pointer to string explaining authorised values
   \param aint pointer to an int where the value of the argument is stored 
   \param deafult_val value */

void options_new_int_set_default(
	char short_name, char* long_name, char* doc, char* arg, int* aint,
	int default_val);

/**
   \brief declares string-valued option to store in *str with short name, long
   name, description (doc)
   \param short_name char with option short name
   \param long_name pointer to string of option long name
   \param doc pointer to string documenting option
   \param arg pointer to string explaining authorised values
   \param str pointer to string where the value of the argument is stored */
void options_new_string(
	char short_name, char* long_name, char* doc, char* arg, char** str);

/**
   \brief parse options (program arguments)
   \param argc number of arguments
   \param argv array of pointers to argument strings */
void options_parse(int argc, char** argv);

/**
   \brief print options and their values to file
   \param file the stream where options are printed */
void options_fprint(FILE* file);

/**
   \brief initialize module */
void options_init(void);

/**
   \brief sets up miscellaneous informations about the program usage and options
   \remark client code shall not free these objects until after options_done()
   \param arg_table table where non-optional argument strings are stored (destructive)
   \param name program name
   \param version program version
   \param bug_address email address to report bugs
   \param doc_args string describing non-optional arguments (e.g. FILENAME)
   \param doc_head short header string, coming before option descr. in usage
   \param doc_foot additional doc string, coming after option descr. in usage
   \param env_prefix prefix string for environment variables */
void options_setup(
	Tstack_Pchar arg_table, char* name, char* version, char* bug_address,
	char* doc_args, char* doc_head, char* doc_foot, char* env_prefix);
/**
   \brief release module */
void options_done(void);

/**
   \brief prints usage message to file
   \param file the stream where options are printed */
void options_usage(FILE* file);

#endif /* __OPTIONS_H */
