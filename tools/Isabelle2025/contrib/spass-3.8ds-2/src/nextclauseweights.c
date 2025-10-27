#include "nextclause.h"
#include "hashmap.h"
#include "list.h"
#include "clause.h"
#include "cnf.h"

HASHMAP LabelToNumber; //label's string to its number
HASHMAP NumberToLabel; //label's number to its string
HASHMAP LabelNumberToRank; //label's number to its rank (used to compute clause ranks)
intptr_t lastusedlabelnumber=0; //last number used when encountered a new label string
HASHMAP ClauseNumberToLabelNumbers; //map clause numbers to the labels it is derived from (for input clauses the labels they have)
HASHMAP ClauseNumberToRank; //map clause numbers to that rank of that clause
HASHMAP DerivedFromThisClause; //map clause numbers to a list of clause numbers of clauses derived from it

#define SPASS_NEXTCLAUSE_CONJ_RANK 501

void nextclauseweights_Init(void)  {
	LabelToNumber              = hm_Create(11,(HM_GET_HASH_FUNCTION)hm_StringHash ,(HM_KEY_EQUAL_FUNCTION)string_Equal   ,FALSE);
	NumberToLabel              = hm_Create(11,(HM_GET_HASH_FUNCTION)hm_PointerHash,(HM_KEY_EQUAL_FUNCTION)hm_PointerEqual,FALSE);
	LabelNumberToRank          = hm_Create(11,(HM_GET_HASH_FUNCTION)hm_PointerHash,(HM_KEY_EQUAL_FUNCTION)hm_PointerEqual,FALSE);
	ClauseNumberToLabelNumbers = hm_Create(17,(HM_GET_HASH_FUNCTION)hm_PointerHash,(HM_KEY_EQUAL_FUNCTION)hm_PointerEqual,FALSE);
	ClauseNumberToRank         = hm_Create(17,(HM_GET_HASH_FUNCTION)hm_PointerHash,(HM_KEY_EQUAL_FUNCTION)hm_PointerEqual,FALSE);
	DerivedFromThisClause      = hm_Create(17,(HM_GET_HASH_FUNCTION)hm_PointerHash,(HM_KEY_EQUAL_FUNCTION)hm_PointerEqual,FALSE);
}

void nextclauseweights_Free(void) {
	hm_Delete(LabelToNumber);
	hm_Delete(LabelNumberToRank);
}

void nextclauseweights_addLabel(const char* label,intptr_t weight) {
	++lastusedlabelnumber;
//	printf("label %d: %s has weight %d\n",lastusedlabelnumber,label,weight);
	hm_Insert(LabelToNumber,(POINTER)label,(POINTER)lastusedlabelnumber);
	hm_Insert(NumberToLabel,(POINTER)lastusedlabelnumber,(POINTER)label);
	hm_Insert(LabelNumberToRank,(POINTER)lastusedlabelnumber,(POINTER)weight);
}

void nextclauseweights_addClauseWithLabel(intptr_t clausenumber,const char* label) {
	intptr_t labelnumber= (intptr_t)hm_Retrieve(LabelToNumber,(POINTER)label);
//	printf("\tlabel number: %d -> %d\n",clausenumber, labelnumber);
	hm_InsertListInsertUnique(ClauseNumberToLabelNumbers,(POINTER)clausenumber,(POINTER)labelnumber);
}

void nextclauseweights_addLabelToClauseFromClause(intptr_t toclausenumber, intptr_t fromclausenumber) {
	LIST scan= hm_Retrieve(ClauseNumberToLabelNumbers,(POINTER)fromclausenumber);
	if (scan == NULL) {
		//		printf("unlabeled %d <- %d!\n",toclausenumber,fromclausenumber);
		//		misc_StartErrorReport();
		//		misc_FinishErrorReport();
	}
	for (;!list_Empty(scan);scan=list_Cdr(scan)) {
		POINTER clausenumber= list_Car(scan); //is int
		hm_InsertListInsertUnique(ClauseNumberToLabelNumbers,(POINTER)toclausenumber,(POINTER)clausenumber);
	}
}

void nextclauseweights_addInputClauses(LIST clauseList, HASHMAP ClauseToLabelMap) {

//	printf("\n\naddInutClauses\n\n");
	for (;!(list_Empty(clauseList));clauseList = list_Cdr(clauseList)) {
		CLAUSE clause= list_Car(clauseList);
		intptr_t clausenumber= clause_Number(clause);
//		printf("processing clause [%d", clause_Number(clause)); printf("]\n");
		if (ClauseToLabelMap != NULL) {

			LIST L=hm_Retrieve(ClauseToLabelMap,clause);

			L = cnf_DeleteDuplicateLabelsFromList(L);

			if (L == NULL) {
				printf("no entry in map\n");
				misc_StartErrorReport();
				misc_FinishErrorReport();
			}

			intptr_t minrank= INT_MAX;
			for (LIST Scan = L; !list_Empty(Scan); Scan = list_Cdr(Scan)) {
				if (!(strncmp((char*) list_Car(Scan), "_SORT_", 6) == 0)) {
					const char* label= (const char*)list_Car(Scan);
					intptr_t labelnumber= (intptr_t)hm_Retrieve(LabelToNumber,(POINTER)label);
					nextclauseweights_addClauseWithLabel(clausenumber,(POINTER)label);
					intptr_t rank= (intptr_t) hm_Retrieve(LabelNumberToRank,(POINTER)labelnumber);
					if (rank < minrank && rank > 0) {
						minrank= rank;
					}
				}
			}
			if (minrank == INT_MAX) {
				minrank=1111;
			} else if (hm_Retrieve(ClauseNumberToRank,(POINTER)clausenumber)) {
				hm_Remove(ClauseNumberToRank,(POINTER)clausenumber);
			}
			hm_Insert(ClauseNumberToRank,(POINTER)clausenumber,(POINTER)minrank);
		} else {
			printf("label map empty\n");
			misc_StartErrorReport();
			misc_FinishErrorReport();
		}
	}
}

void nextclauseweights_addClause(CLAUSE clause) {
	intptr_t clausenumber= (intptr_t)clause_Number(clause);
	int rank= nextclauseweights_getRank(clause);

	intptr_t minrank= INT_MAX;
	for (LIST scan= clause_ParentClauses(clause);!list_Empty(scan); scan= list_Cdr(scan)) {
		intptr_t parentnumber= (intptr_t)list_Car(scan);
		hm_InsertListInsertUnique(DerivedFromThisClause,parentnumber,clausenumber);
		nextclauseweights_addLabelToClauseFromClause(clausenumber,parentnumber);
		intptr_t rank= (intptr_t) hm_Retrieve(ClauseNumberToRank,(POINTER)parentnumber);
		if (rank < minrank && rank > 0)
			minrank= rank;
	}
	if (minrank <= 0) {
		//TODO
		printf("Warning setting to rank which is <= 0 to 1000\n");
		clause_Print(clause);
		minrank= rank;
	} else if (minrank == INT_MAX) {
		minrank=1112;
	} else {
		if (minrank < rank || (rank == 0 && minrank > 0) ) {
			if (hm_Retrieve(ClauseNumberToRank,(POINTER)clausenumber)) {
				hm_Remove(ClauseNumberToRank,(POINTER)clausenumber);
			}
			hm_Insert(ClauseNumberToRank,(POINTER)clausenumber,(POINTER)minrank);
		}
	}

}

intptr_t nextclauseweights_getRank(CLAUSE clause) {
	intptr_t clausenumber= (intptr_t)clause_Number(clause);
	intptr_t rank= (intptr_t)hm_Retrieve(ClauseNumberToRank,(POINTER)clausenumber);
	return rank;
}


void nextclauseweights_fixClauseList(LIST ClauseList)
/**************************************************************
  INPUT:   A list of clauses.
  RETURNS: Nothing.
  SUMMARY: Prints the clauses to stdout.
  CAUTION: Uses the clause_Print function.
 ***************************************************************/
{
	for (;!(list_Empty(ClauseList));ClauseList = list_Cdr(ClauseList)) {
		CLAUSE clause= list_Car(ClauseList);
		nextclauseweights_addClause(clause);
	}
}
