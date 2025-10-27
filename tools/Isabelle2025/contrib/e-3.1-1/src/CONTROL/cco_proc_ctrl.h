/*
                                  ***   StarForth   ***

  cco_proc_ctrl.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.161-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/CONTROL/cco_proc_ctrl.h
 */

#ifndef CCO_PROC_CTRL

#define CCO_PROC_CTRL

#include <sys/select.h>
#include <signal.h>
#include <clb_numtrees.h>
#include <clb_simple_stuff.h>
#include <cio_tempfile.h>


/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/


typedef struct e_pctrl_cell
{
   pid_t        pid;
   int          fileno;
   FILE*        pipe;
   char*        input_file;
   char*        name;
   long long    start_time;
   long         prob_time;
   ProverResult result;
   DStr_p       output;
}EPCtrlCell, *EPCtrl_p;

#define EPCTRL_BUFSIZE 200
#define MAX_CORES 8


typedef struct e_pctrl_set_cell
{
   NumTree_p procs;  /* Indexed by fileno() */
   char      buffer[EPCTRL_BUFSIZE];
}EPCtrlSetCell, *EPCtrlSet_p;

/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

#define SZS_THEOREM_STR    "# SZS status Theorem"
#define SZS_CONTRAAX_STR   "# SZS status ContradictoryAxioms"
#define SZS_UNSAT_STR      "# SZS status Unsatisfiable"
#define SZS_SATSTR_STR     "# SZS status Satisfiable"
#define SZS_COUNTERSAT_STR "# SZS status CounterSatisfiable"
#define SZS_GAVEUP_STR     "# SZS status GaveUp"
#define SZS_FAILURE_STR    "# Failure:"


#define E_OPTIONS_BASE " --print-pid -s -R  --memory-limit=2048 --proof-object "
#define E_OPTIONS "--satauto-schedule --assume-incompleteness"


extern char* PRResultTable[];

#define EPCtrlCellAlloc()    (EPCtrlCell*)SizeMalloc(sizeof(EPCtrlCell))
#define EPCtrlCellFree(junk) SizeFree(junk, sizeof(EPCtrlCell))

EPCtrl_p EPCtrlAlloc(char* name);
void     EPCtrlFree(EPCtrl_p junk);

EPCtrl_p ECtrlCreate(char* prover, char* name,
                     char* extra_options,
                     long cpu_limit, char* file);

EPCtrl_p ECtrlCreateGeneric(char* prover, char* name,
                            char* options, char* extra_options,
                            long cpu_limit, char* file);
void     EPCtrlCleanup(EPCtrl_p ctrl, bool delete_file1);

bool EPCtrlGetResult(EPCtrl_p ctrl,
                     char* buffer,
                     long buf_size);

#define EPCtrlSetCellAlloc()    (EPCtrlSetCell*)SizeMalloc(sizeof(EPCtrlSetCell))
#define EPCtrlSetCellFree(junk) SizeFree(junk, sizeof(EPCtrlSetCell))

EPCtrlSet_p EPCtrlSetAlloc(void);
void        EPCtrlSetFree(EPCtrlSet_p junk, bool delete_files);
void        EPCtrlSetAddProc(EPCtrlSet_p set, EPCtrl_p proc);
EPCtrl_p    EPCtrlSetFindProc(EPCtrlSet_p set, int fd);
void        EPCtrlSetDeleteProc(EPCtrlSet_p set,
                                EPCtrl_p proc, bool delete_file);
#define     EPCtrlSetEmpty(set) ((set)->procs==NULL)
#define     EPCtrlSetCardinality(set) NumTreeNodes((set)->procs)

int         EPCtrlSetFDSet(EPCtrlSet_p set, fd_set *rd_fds);

EPCtrl_p    EPCtrlSetGetResult(EPCtrlSet_p set, bool delete_files);

#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
