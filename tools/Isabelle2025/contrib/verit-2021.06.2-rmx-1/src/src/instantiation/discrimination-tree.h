/**
   \file discrimination-tree.h
   \brief This implements non-perfect discrimination tree. Hence, variables
          are assumed to not repeat. */

#ifndef __DISCRIMINATION_TREE_H
#define __DISCRIMINATION_TREE_H

#include "symbolic/DAG.h"

/**
   \typedef Tdisc_tree
   \brief Represents a discrimination tree.
  */
typedef struct disc_tree Tdisc_tree;

/**
   \typedef Tdisc_tree_node
   \brief Representation of a node in the tree. Currently only used for leaf
          nodes.
  */
typedef unsigned Tdisc_tree_node;

TSstack(_disc_tree_node, Tdisc_tree_node); /* typedefs Tstack_disc_tree_node */

/**
   \brif Create an empty discrimination tree. */
Tdisc_tree* disc_tree_INIT(void);

/**
   \brief Free discrimination tree.
   \param dt Pointer to a discrimination tree.
   \remark Metadata added by the `disc_tree_node_set_meta` function is not
           freed.*/
void disc_tree_free(Tdisc_tree* dt);

/**
   \brief Returns the number of indexed terms.
   \param dt Pointer to a discrimination tree.
   \return The number of indexed DAGs. Corresponds to insert calls. Hence,
           reperated DAGs are counted every time the are added. */
unsigned disc_tree_num_terms(Tdisc_tree* dt);

/**
   \brief Insert a DAG into the tree.
   \param dt Pointer to a discrimination tree.
   \param term A term. Should not have a top-level binder. Variables which
               should be considered as variables must be free.
   \return The leaf corresponding to the inserted term. */
Tdisc_tree_node disc_tree_insert(Tdisc_tree* dt, TDAG term);

/**
   \brief Insert a DAG into the tree while treating some variables as constants.
   \param dt Pointer to a discrimination tree.
   \param term A DAG. Should not have a top-level binder. Variables which
               should be considered as variables must be free.
   \param vars Variables to ignore. The stack must be sorted.
   \return The leaf node the DAG got added to. */
Tdisc_tree_node disc_tree_insert_vars(
	Tdisc_tree* dt, TDAG term, Tstack_DAG vars);

/**
   \brief Lookup potential unifiable DAGs.
   \param dt Pointer to a discrimination DAG.
   \param term The query DAG. Should not have a top-level binder. Variables
               which should be considered as variables must be free.
   \return A stack of DAGs that are unifiable with the query if all variables
           are made distinct.
   \remark DAGs might appear multiple times in the result. */
Tstack_DAG disc_tree_lookup(Tdisc_tree* dt, TDAG term);

/**
   \brief Lookup potential unifiable DAGs while treating some variables in the
          query as constants.
   \param dt Pointer to a discrimination DAG.
   \param term The query DAG. Should not have a top-level binder. Variables
               which should be considered as variables must be free.
   \param vars Variables to ignore. The stack must be sorted.
   \return A stack of DAGs that are unifiable with the query if all variables
           not in `vars` are made distinct.
   \remark DAGs might appear multiple times in the result. */
Tstack_DAG disc_tree_lookup_vars(Tdisc_tree* dt, TDAG term, Tstack_DAG vars);

/**
   \brief Lookup the leaf nodes corresponding to potential unifiable DAGs.
   \param dt Pointer to a discrimination DAG.
   \param term The query DAG. Should not have a top-level binder. Variables
               which should be considered as variables must be free.
   \return A stack of node references. */
Tstack_disc_tree_node disc_tree_lookup_nodes(Tdisc_tree* dt, TDAG term);

/**
   \brief Lookup the leaf nodes corresponding to potential unifiable DAGs while
          treating some variables in the query as constants.
   \param dt Pointer to a discrimination DAG.
   \param term The query DAG. Should not have a top-level binder. Variables
               which should be considered as variables must be free.
   \param vars Variables to ignore. The stack must be sorted.
   \return A stack of node references. */
Tstack_disc_tree_node disc_tree_lookup_vars_nodes(
	Tdisc_tree* dt, TDAG term, Tstack_DAG vars);

/**
   \brief Return the DAGs stored at a leaf node.
   \param dt Pointer to a discrimination DAG.
   \param node A leaf node.
   \return A copy of the stack of DAGs stored at this node. */
Tstack_DAG disc_tree_node_DAGs(Tdisc_tree* dt, Tdisc_tree_node node);

/**
   \brief Return the meta pointer stored at a leaf node.
   \param dt Pointer to a discrimination DAG.
   \param node A leaf node.
   \return A value saved by `disc_tree_node_set_meta` at this node. */
void* disc_tree_node_meta(Tdisc_tree* dt, Tdisc_tree_node node);

/**
   \brief Save a meta data pointer to a leaf node.
   \param dt Pointer to a discrimination DAG.
   \param node A leaf node.
   \param meta A user chosen value. */
void disc_tree_node_set_meta(Tdisc_tree* dt, Tdisc_tree_node node, void* meta);

/**
   \brief Applies a function to all stored meta data.
   \param dt Pointer to a discrimination tree.
   \param f The function to apply.
   \remark Intended to free the data stored in metadata */
void disc_tree_meta_apply(Tdisc_tree* dt, void (*f)(void*));

#endif /* __DISCRIMINATION_TREE_H */
