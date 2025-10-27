#include "instantiation/discrimination-tree.h"

#include "symbolic/DAG-print.h"
#include "symbolic/veriT-DAG.h"
#include "utils/stack.h"

/**
   \typedef Tsymbol
   \brief Represents things that can be symbols. Either proper symbols,
          n-ary symbols with arity, or DAGs starting with binders.
  */
typedef struct TSelement
{
	/** If the lowest bit is 1 this is a DAG, otherwise a reglar symbol */
	unsigned arity;
	unsigned element; /*< Either a symbol or a DAG */
} Telement;

TSstack(_element, Telement); /* typedefs Tstack_element */

/**
   \brief Creates an appropiate element from the head of the DAG */
static inline Telement
element_from_DAG(TDAG s, Tstack_DAG vars)
{
	assert(s != DAG_NULL);
	Tsymb symb = DAG_symb(s);

	/* We are at a variable */
	if (!DAG_arity(s) && DAG_symb_type(symb) & SYMB_VARIABLE) {
		if (!vars || !stack_DAG_contains(vars, s))
			return (Telement){ .arity = 0, .element = DAG_SYMB_NULL };
	}

	if (binder(symb))
		return (Telement){ .arity = 1, .element = s };

	unsigned local_arity = DAG_arity(s);
	return (Telement){ .arity = local_arity << 1, .element = symb };
}

static inline unsigned
element_arity(Telement element)
{
	return (element.arity >> 1);
}

#ifdef DEBUG_DISC_TREE
static inline void
element_print(Telement element)
{
	if (element.element == DAG_SYMB_NULL) {
		assert(!element.arity);
		printf("* ");
		return;
	}

	if (element.arity != 1) {
		printf(
			"%s/%u ", DAG_symb_name_rectify(element.element), element_arity(element));
		return;
	}

	if (element.arity != 1) {
		DAG_print(element.element);
		printf(" ");
	}
}
#endif /* DEBUG_DISC_TREE */

/**
   \typedef Tsymb_assoc
   \brief Associates a symbol with the corresponding child. Elements in the
          symbol to node hash map.
  */
typedef struct TSsymb_assoc
{
	Telement element;
	unsigned node_idx;
} Tsymb_assoc;

static inline unsigned
symb_assoc_key(Tsymb_assoc symb_assoc)
{
	/* TODO: This hashes multiple occurences of the same n-ary symbol
	   with different arities to the same key. */
	if (symb_assoc.element.arity == 1)
		return DAG_key(symb_assoc.element.element);
	else
		return DAG_symb_key(symb_assoc.element.element);
}

static inline bool
symb_assoc_eq(Tsymb_assoc symb_assoc1, Tsymb_assoc symb_assoc2)
{
	return (
		symb_assoc1.element.arity == symb_assoc2.element.arity &&
		symb_assoc1.element.element == symb_assoc2.element.element);
}

/* set up definitions to create hash table for symb_assoc's */
#define TYPE Tsymb_assoc
#define TYPE_EXT symb_assoc
#define TYPE_KEY symb_assoc_key
#define TYPE_EQ symb_assoc_eq
#define TYPE_NULL \
	(Tsymb_assoc) { 0 }

#include "utils/h.h"

/* unset definitions */
#undef TYPE
#undef TYPE_EXT
#undef TYPE_KEY
#undef TYPE_EQ
#undef TYPE_NULL

/* Note: this is a pointer */
typedef Th_symb_assoc Tsymb_map;

typedef struct TSnode
{
	bool is_leaf;
	/* We either have a map from symbols to children or leaf terms */
	union
	{
		Tsymb_map children;
		struct
		{
			Tstack_DAG terms; /*< Leaf terms. Equal up to variables */
			void* meta;
		};
	};
	Tstack_unsigned
		jumps; /*< Pointers to the end of the term started by this node */
} Tnode;

TSstack(_node, Tnode); /* typedefs Tstack_DAGstack */

struct disc_tree
{
	Tstack_node nodes;
	unsigned num_terms; /*< The number of indexed terms */
};

#ifdef DEBUG_DISC_TREE
static void
print_assoc(Tsymb_assoc assoc)
{
	if (assoc.element.arity | 1)
		printf(
			"  [%u]%s/%u -> %d\n", symb_assoc_key(assoc),
			DAG_symb_name_rectify(assoc.element.element), assoc.element.arity >> 1,
			assoc.node_idx);
	else
		printf(
			"  [%u]%s -> %d\n", symb_assoc_key(assoc),
			DAG_symb_name_rectify(assoc.element.element), assoc.node_idx);
}

static inline void
print_node_d(Tnode n)
{
	if (n.is_leaf) {
		printf(" Leaf\n");
		for (unsigned i = 0; i < stack_size(n.terms); ++i) {
			DAG_print(stack_get(n.terms, i));
			printf("\n");
		}
	} else {
		h_symb_assoc_apply(n.children, print_assoc);
		printf(" Jumps:\n  ");
		for (unsigned i = 0; i < stack_size(n.jumps); ++i) {
			printf("%u ", stack_get(n.jumps, i));
		}
		printf("\n");
	}
}

static inline void
print_node(Tdisc_tree* dt, unsigned node_idx)
{
	printf("Node %u\n", node_idx);
	print_node_d(stack_get(dt->nodes, node_idx));
}
#endif /* DEBUG_DISC_TREE */

static void
node_free(Tnode node)
{
	if (node.is_leaf) {
		stack_apply(node.terms, DAG_free);
		stack_free(node.terms);
	} else
		h_symb_assoc_free(&node.children);
	if (node.jumps)
		stack_free(node.jumps);
}

Tdisc_tree*
disc_tree_INIT(void)
{
	Tdisc_tree* t;
	MY_MALLOC(t, sizeof(Tdisc_tree));
	stack_INIT_s(t->nodes, 16); /* We allocate a few extra starting nodes. */
	Tsymb_map map = h_symb_assoc_new(2);
	Tnode node = (Tnode){ .is_leaf = false, .children = map };
	stack_INIT(node.jumps);
	stack_push(t->nodes, node);
	t->num_terms = 0;
	return t;
}

void
disc_tree_free(Tdisc_tree* dt)
{
	assert(dt);
	stack_apply(dt->nodes, node_free);
	stack_free(dt->nodes);
	free(dt);
}

unsigned
disc_tree_num_terms(Tdisc_tree* dt)
{
	assert(dt);
	return dt->num_terms;
}

static unsigned
disc_tree_insert_rec(
	Tdisc_tree* dt, TDAG term, unsigned attach_node_idx, bool in_end, TDAG target,
	Tstack_DAG vars)
{
	assert(term != DAG_NULL);

	Tnode* attach_node = &stack_get(dt->nodes, attach_node_idx);
	Tsymb_map attach_map = attach_node->children;

	Telement element = element_from_DAG(term, vars);
#ifdef DEBUG_DISC_TREE
	printf("Attach ");
	element_print(element);
	printf("at:\n");
	print_node(dt, attach_node_idx);
#endif /* DEBUG_DISC_TREE */

	Tsymb_assoc query = (Tsymb_assoc){ .element = element, .node_idx = 0 };
	Tsymb_assoc result;
	result = h_symb_assoc_get(attach_map, query);

	bool is_leaf = in_end && (DAG_arity(term) == 0);
	unsigned new_node_idx;
	if (result.node_idx == 0) {
		Tnode new_node = (Tnode){ .is_leaf = is_leaf };
		if (is_leaf) {
			stack_INIT(new_node.terms);
			stack_push(new_node.terms, DAG_dup(target));
			new_node.meta = NULL;
		} else {
			stack_INIT(new_node.jumps);
			new_node.children = h_symb_assoc_new(2);
		}
		new_node_idx = stack_size(dt->nodes);
		stack_push(dt->nodes, new_node);
		Tsymb_assoc assoc =
			(Tsymb_assoc){ .element = element, .node_idx = new_node_idx };
		h_symb_assoc_push(attach_map, assoc);
	} else {
		if (is_leaf) {
			assert(stack_get(dt->nodes, result.node_idx).is_leaf);
			stack_push(stack_get(dt->nodes, result.node_idx).terms, DAG_dup(target));
		}
		new_node_idx = result.node_idx;
	}

	/* Recurse */
	unsigned idx = new_node_idx;
	if (!is_leaf) {
		for (unsigned i = 0; i < DAG_arity(term); ++i) {
			/* If this is the last argument of a last argument */
			bool end = in_end && i == (DAG_arity(term) - 1);
			idx = disc_tree_insert_rec(dt, DAG_arg(term, i), idx, end, target, vars);
		}
	}

	/* Push to the jump list. If the arity is 0 this is a self loop. Since nodes
	   are allocated linearely we know that if idx is bigger than the last index
	   in the jump list, it must be new. */
	/* The node might have moved if something got resized */
	attach_node = &stack_get(dt->nodes, attach_node_idx);
	if (stack_size(attach_node->jumps) > 0) {
		unsigned last = stack_top(attach_node->jumps);
		if (idx > last)
			stack_push(attach_node->jumps, idx);
	} else {
		stack_push(attach_node->jumps, idx);
	}

	return idx;
}

Tdisc_tree_node
disc_tree_insert_vars(Tdisc_tree* dt, TDAG term, Tstack_DAG vars)
{
	assert(term != DAG_NULL);
#ifdef DEBUG_DISC_TREE
	printf("Insert term ");
	DAG_print(term);
	printf("\n");
#endif /* DEBUG_DISC_TREE */
	unsigned u = disc_tree_insert_rec(dt, term, 0, true, term, vars);
	assert(stack_get(dt->nodes, u).is_leaf);
	dt->num_terms++;
#ifdef DEBUG_DISC_TREE
	printf("------ Done Inserting -------\n");
#endif /* DEBUG_DISC_TREE */
	return u;
}

Tdisc_tree_node
disc_tree_insert(Tdisc_tree* dt, TDAG term)
{
	return disc_tree_insert_vars(dt, term, NULL);
}

/**
   \typedef Tbacktrack
   \brief Data structure to save backtrack points. */
typedef struct TSbacktrack
{
	unsigned idx;
	unsigned node_idx;
} Tbacktrack;

TSstack(_backtrack, Tbacktrack); /* typedefs Tstack_backtrack */

/* Get all subtrees matching a variable in the query term */
static inline Tstack_backtrack
next_tree(Tdisc_tree* dt, Tstack_element query, unsigned idx, Tnode* node)
{
	assert(!(stack_get(query, idx).arity & 1));
	assert(stack_get(query, idx).element == DAG_SYMB_NULL);

	Tstack_backtrack result;

	stack_INIT_s(result, stack_size(node->jumps));
	for (unsigned i = 0; i < stack_size(node->jumps); ++i) {
		unsigned jump = stack_get(node->jumps, i);
		Tbacktrack bt = (Tbacktrack){ .idx = idx + 1, .node_idx = jump };
		stack_push(result, bt);
	}

	return result;
}

/* result needs to be the symbol not captured by a variable on the current
   symbol */
static inline unsigned
next_term(Tstack_element query, unsigned idx)
{
	unsigned arity = element_arity(stack_get(query, idx));

	idx++;
	for (unsigned a = 0; a < arity; ++a) idx = next_term(query, idx);

	return idx;
}

/**
   \brief Performs a preorder traversal of the term and writes the result into
          a stack of symbols. Varaibles are replaced with DAG_SYMB_NULL.
  */
static inline Tstack_element
traverse(TDAG term, Tstack_DAG vars)
{
	Tstack_element result;
	stack_INIT(result);
	Tstack_DAG queue;
	stack_INIT(queue);
	stack_push(queue, term);

	while (!stack_is_empty(queue)) {
		TDAG term = stack_pop(queue);
		Telement element = element_from_DAG(term, vars);
		stack_push(result, element);

		assert(binder(DAG_symb(term)) || DAG_arity(term) == element_arity(element));

		if (element_arity(element) > 0) {
			for (unsigned i = element_arity(element); i > 0; --i) {
				stack_push(queue, DAG_arg(term, i - 1));
			}
		}
	}
	stack_free(queue);
	return result;
}

Tstack_disc_tree_node
disc_tree_lookup_vars_nodes(Tdisc_tree* dt, TDAG term, Tstack_DAG vars)
{
	assert(term != DAG_NULL);
	Tstack_element query = traverse(term, vars);
#ifdef DEBUG_DISC_TREE
	printf("Lookup of ");
	DAG_print(term);
	printf("\nTraverses to: ");
	for (unsigned i = 0; i < stack_size(query); ++i)
		element_print(stack_get(query, i));
	printf("\n");
	printf("Base:\n ");
	print_node(dt, 0);
#endif /* DEBUG_DISC_TREE */

	Tstack_disc_tree_node result;
	stack_INIT(result);

	Tstack_backtrack backtrack;
	stack_INIT(backtrack);

	Tbacktrack bt = (Tbacktrack){ .idx = 0, .node_idx = 0 };
	stack_push(backtrack, bt);

	while (!stack_is_empty(backtrack)) {
		Tbacktrack bt = stack_pop(backtrack);
		unsigned i = bt.idx;
		Tnode node = stack_get(dt->nodes, bt.node_idx);
		Tdisc_tree_node current_node_idx = bt.node_idx;
#ifdef DEBUG_DISC_TREE
		if (i < stack_size(query)) {
			printf("Backtrack node %u query [%u]", bt.node_idx, i);
			element_print(stack_get(query, i));
			printf("\n");
		} else
			printf("Backtrack node %u end.\n", bt.node_idx);
#endif

		while (i < stack_size(query)) {
			assert(!node.is_leaf);
			Telement e = stack_get(query, i);
#ifdef DEBUG_DISC_TREE
			printf("Element under consisteration [%u]", i);
			element_print(e);
			printf("\n");
			print_node_d(node);
#endif

			/* Two variable cases: we are one, or there is one in the tree */
			/* We are a variable */
			if (e.element == DAG_SYMB_NULL) {
				assert(!e.arity);
#ifdef DEBUG_DISC_TREE
				printf("Symbol is variable.\n");
#endif
				Tstack_backtrack next_bt = next_tree(dt, query, i, &node);
#ifdef DEBUG_DISC_TREE
				printf("  Add %u backtrack points.\n", stack_size(next_bt));
#endif
				stack_merge(backtrack, next_bt);
				stack_free(next_bt);
				break;
			}

			/* TODO: Optimize by not storing the variable case in the hash map */
			Tsymb_assoc q =
				(Tsymb_assoc){ .element = (Telement){ 0, 0 }, .node_idx = 0 };
			Tsymb_assoc r = h_symb_assoc_get(node.children, q);
			if (r.node_idx != 0) {
#ifdef DEBUG_DISC_TREE
				printf("There is a variable at this node.\n");
#endif
				/* We have an outgoing edge which is a variable */
				unsigned next_idx = next_term(query, i);
				Tbacktrack bt = (Tbacktrack){ .idx = next_idx, .node_idx = r.node_idx };
				stack_push(backtrack, bt);
			}

			q = (Tsymb_assoc){ e, 0 };
			r = h_symb_assoc_get(node.children, q);
			/* We found a subsequent node */
			if (r.node_idx != 0) {
#ifdef DEBUG_DISC_TREE
				printf("Found connection!\n");
#endif
				/* This shouldn't be a variable */
				assert(r.element.element != DAG_SYMB_NULL);
				assert(r.element.arity == e.arity);
				assert(r.element.element == e.element);
				node = stack_get(dt->nodes, r.node_idx);
				current_node_idx = r.node_idx;
				i++;
			} else {
#ifdef DEBUG_DISC_TREE
				printf("No connection found!\n");
#endif
				break;
			}
		}
		/* Either we failed, or we are at the end */
		assert(
			(!node.is_leaf && i < stack_size(query)) ||
			(node.is_leaf && i == stack_size(query)));
		if (node.is_leaf) {
#ifdef DEBUG_DISC_TREE
			printf("merging in %u terms\n", stack_size(node.terms));
#endif
			stack_push(result, current_node_idx);
		}
	}
	stack_free(backtrack);
	return result;
}

Tstack_disc_tree_node
disc_tree_lookup_nodes(Tdisc_tree* dt, TDAG term)
{
	return disc_tree_lookup_vars_nodes(dt, term, NULL);
}

Tstack_DAG
disc_tree_node_DAGs(Tdisc_tree* dt, Tdisc_tree_node node)
{
	Tnode n = stack_get(dt->nodes, node);
	assert(n.is_leaf);
	Tstack_DAG result;
	stack_INIT_s(result, stack_size(n.terms));
	stack_COPY(result, n.terms);
	return result;
}

Tstack_DAG
disc_tree_lookup_vars(Tdisc_tree* dt, TDAG term, Tstack_DAG vars)
{
	Tstack_disc_tree_node nodes = disc_tree_lookup_vars_nodes(dt, term, vars);
	Tstack_DAG result;
	stack_INIT(result);
	for (unsigned i = 0; i < stack_size(nodes); ++i) {
		Tnode n = stack_get(dt->nodes, stack_get(nodes, i));
		assert(n.is_leaf);
		stack_merge(result, n.terms);
	}
	return result;
}

Tstack_DAG
disc_tree_lookup(Tdisc_tree* dt, TDAG term)
{
	return disc_tree_lookup_vars(dt, term, NULL);
}

void*
disc_tree_node_meta(Tdisc_tree* dt, Tdisc_tree_node node)
{
	Tnode n = stack_get(dt->nodes, node);
	assert(n.is_leaf);
	return n.meta;
}

void
disc_tree_node_set_meta(Tdisc_tree* dt, Tdisc_tree_node node, void* meta)
{
	assert(stack_get(dt->nodes, node).is_leaf);
	stack_get(dt->nodes, node).meta = meta;
}

void
disc_tree_meta_apply(Tdisc_tree* dt, void (*f)(void*))
{
	for (unsigned i = 0; i < stack_size(dt->nodes); ++i) {
		Tnode n = stack_get(dt->nodes, i);
		if (n.is_leaf && n.meta != NULL)
			f(n.meta);
	}
}

