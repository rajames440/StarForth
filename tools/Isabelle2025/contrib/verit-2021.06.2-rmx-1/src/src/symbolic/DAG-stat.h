#ifndef DAG_STAT_H
#define DAG_STAT_H

#include "symbolic/DAG.h"

/**
   \brief DAG size
   \param DAG to be measured
   \return Number of nodes in DAG as a DAG representation */
extern unsigned DAG_count_nodes(TDAG DAG);

/**
   \brief Expanded DAG size
   \param DAG to be measured
   \return Number of nodes in DAG as a tree representation
   \remark returns UINT_MAX for overflow */
extern unsigned DAG_count_nodes_tree(TDAG DAG);

/**
   \brief Expanded DAG boolean size
   \param DAG to be measured
   \return Number of atoms in DAG as a tree representation */
extern unsigned DAG_count_atoms(TDAG DAG);

/**
   \brief Depth of a DAG
   \param DAG to be measured
   \return Depth of a DAG as a tree representation (depth is 1 for leafs) */
extern unsigned DAG_depth(TDAG DAG);

#endif
