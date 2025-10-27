#ifndef UNDO_H
#define UNDO_H

#include "utils/general.h"
#include "utils/stack.h"

/* Public */

/**
   \brief type for undo information */
typedef unsigned Tundo_type;
/**
   \brief current level */
extern unsigned undo_level;
/**
   \brief current level */
extern unsigned undo_top_level;

/**
   \brief function to call when undoing some step */
typedef void (*Tundo_hook)(void*);

enum
{
	UNDO_LEVEL = 0,
	UNDO_CC = 1, /* + 14 */
	UNDO_OLD_ARITH = 20, /* + 6 */
	UNDO_EMATCH = 30, /* + 5 */
	UNDO_VERIT = 40, /* + 2 */
	UNDO_ARITH = 50, /* + 3 */
	UNDO_LA = 60, /* + 3 */
	UNDO_NB = 100
};

/* Private */

extern Tstack_unsigned undo;
extern Tstack_unsigned undo_top;
extern Tundo_hook undo_hook[];
extern unsigned undo_hook_arg_size[];

/* Public */

/**
   \brief put some information on the undo stack
   \param type of information
   \return pointer to a memory chunk sufficiently large to hold information */
static __inline__ void*
undo_push(Tundo_type type)
{
	stack_inc_n(undo, undo_hook_arg_size[type]);
	stack_push(undo, type);
	return (void*)(&stack_top(undo) - undo_hook_arg_size[type]);
}

/**
   \brief put some information on the undo_top stack
   \param type of information
   \return pointer to a memory chunk sufficiently large to hold information */
static __inline__ void*
undo_top_push(Tundo_type type)
{
	stack_inc_n(undo_top, undo_hook_arg_size[type]);
	stack_push(undo_top, type);
	return (void*)(&stack_top(undo_top) - undo_hook_arg_size[type]);
}

/**
   \brief notifies a new level
   \remark Warning: every level del should correspond to a level new */
static __inline__ unsigned
undo_level_new(void)
{
	stack_push(undo, UNDO_LEVEL);
	undo_level++;
	return undo_level;
}

/**
   \brief notifies a new level
   \remark Warning: every level del should correspond to a level new */
static __inline__ unsigned
undo_top_level_new(void)
{
	stack_push(undo_top, UNDO_LEVEL);
	undo_top_level++;
	return undo_top_level;
}

/**
   \brief backtrack to previous level
   \remark Warning: every level del should correspond to a level new */
static __inline__ void
undo_level_del(void)
{
	unsigned type = stack_pop(undo);
	while (type != UNDO_LEVEL) {
		undo_hook[type](&stack_top_n(undo, undo_hook_arg_size[type]));
		stack_dec_n(undo, undo_hook_arg_size[type]);
		type = stack_pop(undo);
	}
	undo_level--;
}

/**
   \brief backtrack to previous level
   \remark Warning: every level del should correspond to a level new */
static __inline__ void
undo_level_del_to_level(unsigned level)
{
	unsigned type;
	while (undo_level > level)
		if ((type = stack_pop(undo)) == UNDO_LEVEL)
			undo_level--;
		else {
			undo_hook[type](&stack_top_n(undo, undo_hook_arg_size[type]));
			stack_dec_n(undo, undo_hook_arg_size[type]);
		}
}

/**
   \brief backtrack to previous level
   \remark Warning: every level del should correspond to a level new */
static __inline__ void
undo_top_level_del(void)
{
	unsigned type = stack_pop(undo_top);
	while (type != UNDO_LEVEL) {
		undo_hook[type](&stack_top_n(undo_top, undo_hook_arg_size[type]));
		stack_dec_n(undo_top, undo_hook_arg_size[type]);
		type = stack_pop(undo_top);
	}
	undo_top_level--;
}

/**
   \brief associate a hook function and a size with a type
   \param type of information
   \param f hook function
   \param size number of bytes necessary to store information */
void undo_set_hook(Tundo_type type, Tundo_hook f, unsigned size);

/**
   \brief remove association of hook function with type
   \param type of information */
void undo_unset_hook(Tundo_type type);

/**
   \brief module init */
void undo_init(void);

/**
   \brief module done */
void undo_done(void);

#ifdef DEBUG
void undo_debug(void);
#endif

#endif
