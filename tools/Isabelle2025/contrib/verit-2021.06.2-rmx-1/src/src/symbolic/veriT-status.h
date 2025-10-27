/**
   \file veriT-status.h

   \brief Proof status in libveriT.

   This file only contains the definition of the type of the proof
   status in the solver. This is in a separate file as the same status
   is also used in different internal proof engines.
*/

#ifndef __VERIT_STATUS_H
#define __VERIT_STATUS_H

/** \brief Enumeration of the different possible proof status in veriT. */
enum Estatus
{
	SAT, /*< \brief satisfiable */
	UNSAT, /*< \brief unsatisfiable */
	/* TODO: shall we rename UNDEF as UNKNOWN ?? Some confusion with UNDEF/OPEN */
	UNDEF, /*< \brief undefined.  The proof obligation is not within the
           theories for which the solver is complete, and the solver was
           not able to show unsatisfiability */
	OPEN /*< not verified yet. Run veriT_solve to (semi)decide */
};

/** \brief Type of proof status in libveriT. */
typedef enum Estatus Tstatus;

#endif /* __VERIT_STATUS_H */
