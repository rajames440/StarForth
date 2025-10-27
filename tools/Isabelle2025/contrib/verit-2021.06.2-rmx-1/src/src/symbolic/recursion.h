/*
  --------------------------------------------------------------
  Module for doing structural recursion on DAGs
  --------------------------------------------------------------
*/

#ifndef __RECURSION_H
#define __RECURSION_H

#include "proof/proof.h"
#include "symbolic/DAG.h"

/**
   \brief builds a new DAG by applying a function
   \param src the input DAG
   \param f a destructive function from DAG to DAG
   \return src in which f has been applied recursively from leaves to root
   \remarks Linear with the DAG size
   \remarks Uses flag
   \remarks Non destructive */
TDAG structural_recursion(TDAG src, TDAG (*f)(TDAG));

/**
   \brief builds new DAGs by applying a function.  The new DAGs are stored in
   the input array
   \param n the number of input DAGs
   \param Psrc the input DAGs
   \param f a destructive function from DAG to DAG
   \remarks Linear with the DAGs size
   \remarks Uses flag
   \remarks Destructive */
void structural_recursion_array(unsigned n, TDAG* Psrc, TDAG (*f)(TDAG));

/**
   \brief builds a new DAG by applying a function while cont(DAG) is true
   \param src the input DAG
   \param f a destructive function from DAG to DAG
   \param cont a predicate on DAGs
   \return src in which f has been applied recursively while cont(DAG) is true
   \remarks Linear with the DAG size
   \remarks Uses flag
   \remarks Non destructive */
TDAG cond_structural_recursion(TDAG src, TDAG (*f)(TDAG), bool (*cont)(TDAG));

/**
   \brief builds a new DAG by applying a function while cont(DAG) is true
   \param n the number of input DAGs
   \param Psrc the input DAGs
   \param f a destructive function from DAG to DAG
   \param cont a predicate on DAGs
   \return src in which f has been applied recursively while cont(DAG) is true
   \remarks Linear with the DAG size
   \remarks Uses flag
   \remarks Non destructive */
void cond_structural_recursion_array(
	unsigned n, TDAG* Psrc, TDAG (*f)(TDAG), bool (*cont)(TDAG));

/**
   \brief applies a predicate in a DAG
   \param src the input DAG
   \param f is a predicate on DAG node
   \return true if and only if f(N) is true for all nodes in src
   \remarks Linear with the DAG size
   \remarks Uses flag
   \remarks Non destructive */
bool structural_recursion_pred(TDAG src, bool (*f)(TDAG));

/**
   \brief applies f on every node of DAG
   \param src the input DAG
   \param f a void function on DAG node
   \remarks Linear with the DAG size
   \remarks Uses flag
   \remarks Non destructive */
void structural_recursion_void(TDAG src, void (*f)(TDAG));

/*
  \brief applies f on every node of DAG while cont(DAG) is true
  \param src the input DAG
  \param f a void function on DAG node
  \param cont a predicate on DAGs
  \remarks Linear with the DAG size
  \remarks Uses flag
  \remarks Non destructive */
void cond_structural_recursion_void(
	TDAG src, void (*f)(TDAG), bool (*cont)(TDAG));

#endif /* __RECURSION_H */
