/*
                                  ***   StarForth   ***

  cte_typebanks.h- FORTH-79 Standard and ANSI C99 ONLY
  Modified by - rajames
  Last modified - 2025-10-27T12:40:02.536-04

  Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.

  This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
  To the extent possible under law, the author(s) have dedicated all copyright and related
  and neighboring rights to this software to the public domain worldwide.
  This software is distributed without any warranty.

  See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.

  /home/rajames/CLionProjects/StarForth/tools/Isabelle2025/contrib/e-3.1-1/src/TERMS/cte_typebanks.h
 */


#ifndef CTE_TYPEBANKS
#define CTE_TYPEBANKS

#include <cte_simpletypes.h>
#include <cio_scanner.h>
#include <clb_objtrees.h>
#include <clb_pdarrays.h>

#define TYPEBANK_SIZE      4096
#define TYPEBANK_HASH_MASK TYPEBANK_SIZE-1

#define NAME_NOT_FOUND          -1

/*---------------------------------------------------------------------*/
/*                    Data type declarations                           */
/*---------------------------------------------------------------------*/
typedef struct typebank_cell {
   PStack_p       back_idx;                   // Type constructor or simple type back index
   StrTree_p      name_idx;                   // Name to arity, type_identifier pair
                                              // for sorts arity is always 0
   long           names_count;                // Counter for different names inserted
   TypeUniqueID   types_count;                // Counter for different
                                              // types inserted --
                                              // Each type will have
                                              // unique ID.
   TypeUniqueID   max_predefined_count;       // Maximum built-in type id
   PObjTree_p     hash_table[TYPEBANK_SIZE];  // Hash table for sharing

   // some types that are accessed frequently.
   Type_p         bool_type;
   Type_p         i_type;
   Type_p         kind_type;
   Type_p         integer_type;
   Type_p         rational_type;
   Type_p         real_type;
   Type_p         default_type;
} TypeBank, *TypeBank_p;

/*---------------------------------------------------------------------*/
/*                Exported Functions and Variables                     */
/*---------------------------------------------------------------------*/

#define TypeBankCellAlloc()   SizeMalloc(sizeof(TypeBank));
void    TypeBankFree(TypeBank_p junk);

TypeBank_p   TypeBankAlloc(void);

Type_p       TypeBankInsertTypeShared(TypeBank_p bank, Type_p t);
TypeConsCode TypeBankDefineTypeConstructor(TypeBank_p bank, const char* name, int arity);
TypeConsCode TypeBankDefineSimpleSort(TypeBank_p bank, const char* name);

TypeConsCode TypeBankFindTCCode(TypeBank_p bank, const char* name);
int          TypeBankFindTCArity(TypeBank_p bank, TypeConsCode tc_code);
const char*  TypeBankFindTCName(TypeBank_p bank, TypeConsCode tc_code);
Type_p       TypeBankParseType(Scanner_p in, TypeBank_p bank);
void         TypePrintTSTP(FILE* out, TypeBank_p bank, Type_p type);
void         TypeBankPrintSelectedSortDefs(FILE* out, TypeBank_p bank,
                                           PTree_p selector);

Type_p       TypeChangeReturnType(TypeBank_p bank, Type_p type, Type_p new_ret);

void         TypeBankAppEncodeTypes(FILE* out, TypeBank_p tb, bool print_type_comment);

#define      TypeBankTypeIsUserDefined(bank, type) \
             ((type)->type_uid > (bank)->max_predefined_count)


#endif

/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/
