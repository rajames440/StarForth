/**
   \file veriT-signal.h


   \brief API for signal handling functionality.

   \note This module provides an API to centralize the signal handling
   necessities from different components. Once initialized, through
   call to veriT_signal_init, client modules may register a signal
   handler procedure and an address. The signal handler procedure
   sig_handler shall have one argument P, a pointer.

   When the program is signaled, all registered signal handling
   procedures are called with the signal and the corresponding address
   as argument.

   Routine veriT_signal_done shall be called to free the resources
   allocated by the module. After it has been called, the program will
   no longer react to signals with the registered signal handlers.
*/

#ifndef __VERIT_SIGNAL_H
#define __VERIT_SIGNAL_H

#include <signal.h>

/**
   \brief Type for signal handling functions. It is a routine that
   takes as argument the signal caught and an address.
*/

typedef void (*Tsigfun)(void);

/**
   \brief Module initialization.
   \note Must be called before veriT_signal_register.
*/
extern void veriT_signal_init(void);
extern void veriT_signal_done(void);

extern void veriT_signal_register(Tsigfun sigfun);

/** \brief Hard coded SIGLRM value to ensure protability. Used for exit(...). */
#define VERIT_SIGALRM 14
/** \brief Hard coded SIGXCPU value to ensure protability. Used for exit(...). */
#define VERIT_SIGXCPU 24

#define TIMEOUT_TEXT "Time limit exceeded\n"

#endif /* __VERIT_SIGNAL_H */
