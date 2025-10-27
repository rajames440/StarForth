/*
                                  ***   StarForth   ***

  pcl_ministeps.c- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:04.096-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/PCL2/pcl_ministeps.c
 */

#include "pcl_ministeps.h"


/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/



/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
//
// Function: PCLMiniStepFree()
//
//   Free a PCLMini step.
//
// Global Variables: -
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

void PCLMiniStepFree(PCLMiniStep_p junk)
{
   assert(junk && junk->id && junk->just);

   if(PCLStepIsFOF(junk))
   {
      /* Formua is garbage collected */
   }
   else
   {
      if(junk->logic.clause)
      {
         MiniClauseFree(junk->logic.clause);
      }
   }
   PCLMiniExprFree(junk->just);
   if(junk->extra)
   {
      FREE(junk->extra);
   }
   PCLMiniStepCellFree(junk);
}


/*-----------------------------------------------------------------------
//
// Function: PCLMiniStepParse()
//
//   Parse a PCLMini step.
//
// Global Variables: -
//
// Side Effects    : Input, memory operations
//
/----------------------------------------------------------------------*/

PCLMiniStep_p PCLMiniStepParse(Scanner_p in, TB_p bank)
{
   PCLMiniStep_p handle = PCLMiniStepCellAlloc();

   assert(in);
   assert(bank);

   handle->bank = bank;
   handle->id = ParseInt(in);
   if(TestInpTok(in, Fullstop))
   {
      AktTokenError(in,
          "No compound PCL identifiers allowed in this mode",
          false);
   }
   AcceptInpTok(in, Colon);
   handle->properties = PCLParseExternalType(in);
   AcceptInpTok(in, Colon);

   if(SupportShellPCL && TestInpTok(in, Colon))
   {
      handle->logic.clause = NULL;
      PCLStepSetProp(handle, PCLIsShellStep);
   }
   else if(TestInpTok(in, OpenSquare))
   {
      handle->logic.clause = MinifyClause(ClausePCLParse(in, bank));
      PCLStepDelProp(handle, PCLIsFOFStep);
   }
   else
   {
      handle->logic.formula = TFormulaTPTPParse(in, bank);
      PCLStepSetProp(handle, PCLIsFOFStep);
   }
   AcceptInpTok(in, Colon);
   handle->just = PCLMiniExprParse(in);
   if(TestInpTok(in, Colon))
   {
      NextToken(in);
      CheckInpTok(in, SQString);
      handle->extra = DStrCopy(AktToken(in)->literal);
      NextToken(in);
   }
   else
   {
      handle->extra = NULL;
   }
   PCLStepDelProp(handle, PCLIsProofStep);
   if(handle->just->op == PCLOpInitial)
   {
      PCLStepSetProp(handle, PCLIsInitial);
   }
   return handle;
}


/*-----------------------------------------------------------------------
//
// Function: PCLMiniStepPrint()
//
//   Print a PCLMini step.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void PCLMiniStepPrint(FILE* out, PCLMiniStep_p step, TB_p bank)
{
   assert(step);

   fprintf(out, "%6ld : ", step->id);
   PCLPrintExternalType(out, step->properties);
   fputs(" : ", out);
   if(!PCLStepIsShell(step))
   {
      if(PCLStepIsFOF(step))
      {
         TFormulaTPTPPrint(out, step->bank, step->logic.formula, true, true);
      }
      else
      {
         MiniClausePCLPrint(out, step->logic.clause, bank);
      }
   }
   fputs(" : ", out);
   PCLMiniExprPrint(out, step->just);
   if(step->extra)
   {
      fprintf(out, " : %s", step->extra);
   }
}


/*-----------------------------------------------------------------------
//
// Function: PCLMiniStepPrintTSTP()
//
//   Print a PCLMini step in TSTP format.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void PCLMiniStepPrintTSTP(FILE* out, PCLMiniStep_p step, TB_p bank)
{
   assert(step);

   if(PCLStepIsClausal(step))
   {
      fprintf(out, "cnf(%ld,%s,",step->id,
              PCLPropToTSTPType(step->properties));
      if(PCLStepIsShell(step))
      {
      }
      else
      {
         MiniClauseTSTPCorePrint(out, step->logic.clause, bank);
      }
   }
   else
   {
      fprintf(out, "fof(%ld, %s,", step->id,
              PCLPropToTSTPType(step->properties));
      if(PCLStepIsShell(step))
      {
      }
      else
      {
         TFormulaTPTPPrint(out, step->bank, step->logic.formula, true, true);
      }
   }
   fputc(',', out);
   PCLExprPrintTSTP(out, step->just, true);
   if(step->extra)
   {
      fprintf(out, ",[%s]", step->extra);
   }
   fputs(").", out);
}


/*-----------------------------------------------------------------------
//
// Function: PCLMiniStepPrintFormat()
//
//   Print a PCL step in the requested format.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void PCLMiniStepPrintFormat(FILE* out, PCLMiniStep_p step, TB_p bank,
                            OutputFormatType format)
{
   switch(format)
   {
   case pcl_format:
    PCLMiniStepPrint(out, step, bank);
    break;
   case tstp_format:
    PCLMiniStepPrintTSTP(out, step, bank);
    break;
   default:
    assert(false);
    break;
   }
}


/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/



