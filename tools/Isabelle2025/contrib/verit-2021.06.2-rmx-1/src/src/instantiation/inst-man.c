#include "instantiation/inst-man.h"

#include "SAT/veriT-SAT.h"
#include "bool/bool.h"
#include "float.h"
#include "instantiation/free-vars.h"
#include "instantiation/inst-del.h"
#include "instantiation/inst-enum.h"
#include "instantiation/inst-index.h"
#include "instantiation/inst-pre.h"
#include "instantiation/inst-strategy.h"
#include "instantiation/inst-symbs.h"
#include "instantiation/inst-trigger-selection.h"
#include "instantiation/inst-trigger.h"
#include "math.h"
#include "pre/pre.h"
#include "proof/proof.h"
#include "symbolic/DAG-print.h"
#include "symbolic/DAG-ptr.h"
#include "symbolic/DAG-subst.h"
#include "symbolic/DAG.h"
#include "utils/bitset.h"
#include "utils/options.h"
#include "utils/stack.h"
#include "utils/statistics.h"

#if defined(DEBUG_HEURISTICS)
#define DEBUG_INST 1
#endif

/* TODO: Is this necessary? */
extern Tprop_id DAG_PROP_CNF;
extern Tprop_id DAG_PROP_SYMBS;

/*
    TODO: workaround to know how many instances were produced at each
    succesful round
*/
unsigned insts_ccfv_round;
unsigned insts_triggers_round;
unsigned insts_sorts_round;

extern bool triggers_new;

/*
  --------------------------------------------------------------
  Options
  --------------------------------------------------------------
*/

/**
   \addtogroup arguments_developer

   - --triggers-off

   Disables trigger-based instantiation. */
static bool triggers_off;

/**
   \addtogroup arguments_developer

   - --CIs-off

   Disables conflict-based instantiation. */
static bool CIs_off;

/**
   \addtogroup arguments_developer

   - --CIs-bound=X

   Bound the number of found CIs per round */
int CIs_bound;

/**
   \addtogroup arguments_developer

   - --enum-one

   Enumerate instances one by one. */
static bool enum_one;

/**
   \addtogroup arguments_developer

   - --inst-sorts-threshold

   Maximum number of sort instantiations to be derived from a
   quantifier. Default is 10^4 [optimize] */
static int insts_sorts_threshold;

/**
   \addtogroup arguments_user

   - --index-SIG

   Index CC signature rather than minimized ground model. */
static bool index_SIG;

/**
   \addtogroup arguments_user

   - --index-SAT-triggers

   Index ground model rather than CC signature. */
static bool index_SAT_triggers;

/**
   \addtogroup arguments_user
   - --index-sorts-exhaust-depth

   Try all depths first before increasing inst level */
/* static bool index_sorts_exhaust_depth_first; */

/**
   \addtogroup arguments_user

   - --inst-deletion

   Delete unused instances between instantiation rounds [unstable] */
static bool inst_deletion;

/**
   \addtogroup arguments_user

   - --inst-deletion-priority-ccfv

   Give highest priority for CCFV instances [unstable] */
static bool inst_deletion_priority_ccfv;

/**
   \addtogroup arguments_user
   - --index-sorts

   Index SAT stack for use in enum instantiation */
bool index_sorts;

#ifdef POLARITY_FILTER
/**
   \addtogroup arguments_developer

   - --bool-required-off

   Do not prune by this superset of the prime-implicant. */
bool opt_bool_required_off;
#endif

/**
   \addtogroup arguments_developer

   - --prime-implicant-off

   Do not prune boolean model by computing its prime implicant. */
bool prime_implicant_off;

/**
   \addtogroup arguments_developer

   - --prune-cnf-off

   Do not prune boolean model from removing unnecessary literals added by CNF
   transformation */
bool prune_cnf_off;

/*
  --------------------------------------------------------------
  Stats
  --------------------------------------------------------------
*/

#ifdef STATS_INST

extern unsigned ccfv_stats_rounds;
extern float insts_time;

/* TODO: Workarounds for counting proper stats */
bool doing_ccfv = true;
bool doing_triggers = true;

/** \brief Number of instantiations rounds */
unsigned insts_stats_rounds;

static unsigned insts_stats_ccfv_before_last;

/** \brief How many instances CCFV generated. */
static unsigned insts_stats_ccfv;

/** \brief How many redundant instances CCFV generated. */
static unsigned insts_stats_redundant_ccfv;

/** \brief How many instances triggers generated. */
static unsigned insts_stats_triggers;

/** \brief How many redundant instances triggers generated. */
static unsigned insts_stats_redundant_triggers_tree;
static unsigned insts_stats_redundant_triggers;

static unsigned insts_stats_sorts;
static unsigned insts_stats_redundant_sorts;

static unsigned sorts_stats_time;
static unsigned sorts_stats_rounds;

/** \brief how many times needed to go to sig table */
static unsigned sorts_stats_more_SIG;

static float ccfv_calls, ccfv_success;

#endif

/* #define STATS_ONLY */

#ifdef STATS_ONLY

extern Tstack_DAG CC_quantified;

/* \brief How many quantifier alternations in initially asserted
   quantified formulas */
static unsigned stats_alt;

/* \brief Deepst alternation in given benchmark */
static unsigned stats_alt_max;

/**

   \brief computes number of quantifier alternations in a DAG, as well as the
   deepest alternation

   \param DAG a subformula
   \param pol polarity under which DAG occurs
   \param underForall Whether DAG is under a universar or existential quantifier
   \param depth keeps the value of deepest alternation in DAG
   \return the total number of alternations in DAG
   \remark traverses DAG as a tree in depth-first manner
   \remark A(EA and AE) is three alternations and the deepest alternation is two
   (for the first conjunct)
   \remark First call should always be in last argument of quantified DAG */
static void
compute_alternations(void) {
    unsigned alt_depth, alternations = 0;
    for (i = 0; i < stack_size(CC_quantified); ++i) {
        TDAG qformula = stack_get(CC_quantified, i);
        alt_depth = 0;
        alternations += count_alternations(
            DAG_arg_last(qformula), 1, DAG_symb(qformula) == QUANTIFIER_FORALL,
            &alt_depth);
        if (alt_depth > stats_counter_get(stats_alt_max))
            stats_counter_set(stats_alt_max, alt_depth);
    }
    stats_counter_set(stats_alt, alternations);
}
#endif

/* TODO: Kill this. Full of ugly hacks. */

/*
  --------------------------------------------------------------
  Groundness
  --------------------------------------------------------------
*/

typedef struct VarClass {
    unsigned rep_ind;
    Tstack_unsigned vars_inds;
} VarClass;

TSstack(_VarClass, VarClass); /* typedefs Tstack_VarClass */

/* Associates a sort to a set of variables indices */
typedef struct SortVars {
    Tsort sort;
    Tstack_VarClass var_classes;
} SortVars;

TSstack(_SortVars, SortVars); /* typedefs Tstack_SortVars */

/* Destructive */
static Tstack_unifier
ground_unifier(Tunifier unifier, unsigned cap, bool all_CIs) {
    unsigned i, j, k, l, var_ind, rep_ind;
    Tstack_DAG terms;
    Tstack_unsigned free_vars_inds;
    Tstack_VarClass var_classes;
    Tstack_SortVars sort_vars;
    Tstack_unifier result, old_result;
    stack_INIT(free_vars_inds);
    stack_INIT(result);
    for (i = 0; i < unifier->size; ++i) {
        rep_ind = unifier->val[i].rep ? i : unify_find(unifier, i);
        if (unifier->val[rep_ind].term) {
            /* Guarantee that the term the var is equal to is disequal to all
             terms in diff */
            if (
                all_CIs &&
                diff_breaks(unifier, rep_ind, unifier->val[rep_ind].term, true)) {
                stack_free(result);
                return NULL;
            }
            if (unifier->val[rep_ind].diff) stack_free(unifier->val[rep_ind].diff);
            if (i != rep_ind) {
                unifier->val[i].rep = true;
                unifier->val[i].term = unifier->val[rep_ind].term;
                unifier->val[i].diff = NULL;
                set_ground_var(unifier, i);
            }
            continue;
        }
        stack_push(free_vars_inds, i);
    }
    stack_push(result, unifier);
#ifdef DEBUG
    if (stack_is_empty(free_vars_inds))
        for (i = 0; i < unifier->size; ++i)
            assert(unifier->val[i].rep && ground(unifier->val[i].term));
#endif
    /* Unifier was grounded already */
    if (stack_is_empty(free_vars_inds)) {
        assert(unify_grounded(unifier));
        stack_free(free_vars_inds);
        return result;
    }
    /* Partition free variables per sort/class */
    stack_INIT(sort_vars);
    while (!stack_is_empty(free_vars_inds)) {
        var_ind = stack_pop(free_vars_inds);
        rep_ind = unify_find(unifier, var_ind);
        for (i = 0; i < stack_size(sort_vars); ++i)
            if (DAG_sort(unifier->val[var_ind].var) == stack_get(sort_vars, i).sort)
                break;
        if (i != stack_size(sort_vars)) {
            for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
                if (
                    stack_get(stack_get(sort_vars, i).var_classes, j).rep_ind == rep_ind)
                    break;
            if (j != stack_size(stack_get(sort_vars, i).var_classes))
                stack_push(
                    stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds, var_ind);
            else {
                VarClass var_class;
                var_class.rep_ind = rep_ind;
                stack_INIT(var_class.vars_inds);
                stack_push(var_class.vars_inds, var_ind);
                stack_push(stack_get(sort_vars, i).var_classes, var_class);
            }
            continue;
        }
        VarClass var_class;
        var_class.rep_ind = rep_ind;
        stack_INIT(var_class.vars_inds);
        stack_push(var_class.vars_inds, var_ind);
        stack_inc(sort_vars);
        stack_top(sort_vars).sort = DAG_sort(unifier->val[var_ind].var);
        stack_INIT(stack_top(sort_vars).var_classes);
        stack_push(stack_top(sort_vars).var_classes, var_class);
    }
    stack_free(free_vars_inds);
    assert(!stack_is_empty(sort_vars));
    /* Collect all ground terms from sort classes of free variables, then ground
     all variables with all terms (per sort), doing the proper combinations */
    stack_INIT(old_result);
    for (i = 0; i < stack_size(sort_vars); ++i) {
        terms = get_sort_terms(stack_get(sort_vars, i).sort);
        if (!terms) break;
        /* For each free variable class, generate a set of unifiers by combining
         previous ones with each term in its sort class */
        var_classes = stack_get(sort_vars, i).var_classes;
        for (j = 0; j < stack_size(var_classes); ++j) {
            stack_merge(old_result, result);
            stack_reset(result);
            rep_ind = stack_get(var_classes, j).rep_ind;
            /* To avoid explosion; TODO: needs to be refined */
            for (k = 0; k < stack_size(terms) && k < cap; ++k)
                for (l = 0; l < stack_size(old_result); ++l) {
                    unifier = unify_copy(stack_get(old_result, l));
                    if (unify_union_ground(
                        unifier, rep_ind, stack_get(var_classes, j).vars_inds,
                        stack_get(terms, k), all_CIs))
                        stack_push(result, unifier);
                    else
                        unify_free(unifier);
                }
            stack_apply(old_result, unify_free);
            stack_reset(old_result);
        }
        stack_free(terms);
    }
    stack_free(old_result);
    /* TODO: One sort class at least is empty. What should be done in such cases?
     Should create a term of the respective sort. */
    if (i != stack_size(sort_vars)) {
        for (i = 0; i < stack_size(sort_vars); ++i) {
            for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
                stack_free(stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds);
            stack_free(stack_get(sort_vars, i).var_classes);
        }
        stack_free(sort_vars);
        stack_apply(result, unify_free);
        stack_free(result);
        return NULL;
    }
    /* Cleaning */
    for (i = 0; i < stack_size(sort_vars); ++i) {
        for (j = 0; j < stack_size(stack_get(sort_vars, i).var_classes); ++j)
            stack_free(stack_get(stack_get(sort_vars, i).var_classes, j).vars_inds);
        stack_free(stack_get(sort_vars, i).var_classes);
    }
    stack_free(sort_vars);
#ifdef DEBUG
    for (i = 0; i < stack_size(result); ++i)
        for (j = 0; j < stack_get(result, i)->size; ++j)
            assert(
                stack_get(result, i)->val[j].rep &&
                ground(stack_get(result, i)->val[j].term));
    for (i = 0; i < stack_size(result); ++i)
        assert(unify_grounded(stack_get(result, i)));
#endif
    if (stack_is_empty(result)) stack_free(result);
    return result;
}

/*
  --------------------------------------------------------------
  Instantiation data structures and global state
  --------------------------------------------------------------
*/

/** \brief Whether quantified formula has already been prepared for
    instantiation (normal forms, triggers) */
static bool *inst_prep;

static void
prep_hook_resize(unsigned old_alloc, unsigned new_alloc) {
    unsigned i;
    MY_REALLOC(inst_prep, new_alloc * sizeof(bool));
    for (i = old_alloc; i < new_alloc; ++i) inst_prep[i] = false;
}

void
DAG_prop_cnf_free(Tstack_DAGstack *Pstack) {
    unsigned i;
    for (i = 0; i < stack_size(*Pstack); ++i) {
        stack_apply(stack_get(*Pstack, i), DAG_free);
        stack_free(stack_get(*Pstack, i));
    }
    stack_free(*Pstack);
}

/*
  --------------------------------------------------------------
  Searching instantiations
  --------------------------------------------------------------
*/

/**
   \brief releases the context dependent information at the end of each
   instantiation cycle */
void
inst_cycle_done(void) {
    /* TODO: make sure it's OK to have this done here */
    CC_reset_symbols();
    CC_free_diseqs();
}

/**
   \brief initializes the context dependent information at each instantiation
   cycle
   \remark indexes the ground model
   \remark prepares quantified formulas for CCFV */
void
inst_cycle_init(void) {
    unsigned i;
    TDAG DAG;
    /* TODO: make sure this is fine */
    stack_reset(lemmas);
    /* TODO: Should I trust this guy? Is it affected by the prime implicant? */
    for (i = 0; i < stack_size(CC_quantified); ++i) {
        DAG = stack_get(CC_quantified, i);
        if (inst_prep[DAG]) continue;
        assert(
            DAG_symb(DAG) != QUANTIFIER_EXISTS && DAG_arg_last(DAG) != DAG_TRUE &&
            DAG_arg_last(DAG) != DAG_FALSE);
        if (triggers_new)
            set_triggers(DAG, NULL, NULL, POL_POS);
        else
            inst_pre(DAG);
        if (!CIs_off) set_NFs(DAG);
        inst_prep[DAG] = true;
    }
    if (!index_SIG) {
        if (!prime_implicant_off) SAT_prime_implicant();
        if (!prune_cnf_off) prune_cnf_model();
    }
#ifdef STATS_INST
    stats_counter_inc(insts_stats_rounds);
#endif
}

static Tstack_DAGinst
inst_sorts(void) {
    unsigned i, j, cap, found_unifiers = 0;
    TDAG qform;
    Tstack_DAG vars;
    Tstack_DAGinst result;
    Tstack_unifier grounds;
#ifdef STATS_INST
    stats_counter_inc(sorts_stats_rounds);
    stats_timer_start(sorts_stats_time);
#endif
    stack_INIT(result);
    for (i = 0; i < stack_size(CC_quantified); ++i) {
        qform = stack_get(CC_quantified, i);
        stack_INIT(vars);
        for (j = 0; j < DAG_arity(qform) - 1; ++j)
            stack_push(vars, DAG_arg(qform, j));
        stack_sort(vars, DAG_cmp_q);
        /* TODO: this is not necessary */
        stack_uniq(vars);
        cap = pow((double) insts_sorts_threshold, 1 / (double) stack_size(vars));
        /* my_DAG_message("Got cap %d for {%d}%D\n", cap, qform, qform); */
        if (index_sorts)
            grounds = ground_unifier(unify_new(vars), cap, false);
        else
            grounds = unify_ground(unify_new(vars), cap, false);
        stack_free(vars);
        if (!grounds) {
#if DEBUG_INST > 2
            my_DAG_message("inst_sorts: unifier ungroundable\n");
#endif
            continue;
        }
        found_unifiers += stack_size(grounds);

        stack_inc(result);
        stack_top(result).qform = qform;
        stack_top(result).clause = NULL;
        stack_COPY(stack_top(result).insts, grounds);
        stack_free(grounds);
    }
#ifdef STATS_INST
    stats_timer_stop(sorts_stats_time);
#if DEBUG_INST
    my_message(
        "\t\t(%2.2fs) Sorts %d: got %d unifiers\n",
        stats_timer_get(sorts_stats_time), stats_counter_get(sorts_stats_rounds),
        found_unifiers);
#endif
#endif
    if (stack_is_empty(result)) stack_free(result);
    return result;
}

extern bool inst_deletion_loops;
extern bool inst_deletion_track_vars;

static bool
inst_strategy(Tstrategy strategy, unsigned inst_lvl) {
    Tstack_DAGinst result = NULL;
#ifdef STATS_INST
    doing_ccfv = strategy == inst_CIs ? true : false;
    doing_triggers = strategy == inst_TRIGGERS ? true : false;
    if (doing_ccfv) ++ccfv_calls;
#endif
    if (strategy == inst_CIs) {
        set_SAT_index(inst_lvl);
        result = CCFV();
        unset_model_index();
    } else if (strategy == inst_TRIGGERS) {
        if (inst_deletion_loops && inst_lvl && inst_lvl == get_deepest_lvl()) {
            set_SAT_lit_index(inst_lvl);
            inst_check_loop = true;
        } else
            set_SAT_index(inst_lvl);
        result = triggers();
        if (inst_deletion_loops && inst_lvl && inst_lvl == get_deepest_lvl()) {
            unset_model_lit_index();
            inst_check_loop = false;
        } else
            unset_model_index();
    } else if (strategy == inst_ENUM) {
        if (index_sorts) set_sorts_index(inst_lvl);
        result = inst_sorts();
        if (index_sorts) unset_sorts_index();
    }
    if (!result) return false;
    while (!stack_is_empty(result)) inst_unifiers(stack_pop(result));
    stack_free(result);
    if (stack_is_empty(lemmas)) return false;
    if (strategy == inst_CIs && inst_lvl < get_deepest_lvl())
        stats_counter_inc(insts_stats_ccfv_before_last);
    if (SAT_set_markup()) update_lvl_up();
    /* TODO: workaround which should disappear */
    if (inst_deletion_priority_ccfv && strategy == inst_CIs)
        inst_done_round = 0;
    else
        inst_done_round = get_last_lvl();
    inst_successful = strategy;
#ifdef STATS_INST
#if DEBUG_INST
    my_message(
        "[(%2.2fs), Round %d, Lvl %d] Instances: %d\n", insts_time,
        stats_counter_get(insts_stats_rounds),
        inst_deletion_priority_ccfv && doing_ccfv ? 0 : get_last_lvl(),
        doing_ccfv
            ? (stats_counter_get(insts_stats_ccfv) - insts_ccfv_round)
            : (doing_triggers
                   ? (stats_counter_get(insts_stats_triggers) - insts_triggers_round)
                   : (stats_counter_get(insts_stats_sorts) - insts_sorts_round)));
#endif
#if DEBUG_INST > 1
    print_Tstack_DAG(lemmas);
    my_message_return();
#endif
    insts_ccfv_round = stats_counter_get(insts_stats_ccfv);
    insts_triggers_round = stats_counter_get(insts_stats_triggers);
    insts_sorts_round = stats_counter_get(insts_stats_sorts);
    if (doing_ccfv) ++ccfv_success;
#endif
    return true;
}

static bool
inst_strategy_conservative(Tstrategy strategy, unsigned inst_lvl) {
    Tstack_DAGinst result = NULL;
#ifdef STATS_INST
    doing_ccfv = strategy == inst_CIs ? true : false;
    doing_triggers = strategy == inst_TRIGGERS ? true : false;
    if (doing_ccfv) ++ccfv_calls;
#endif
    if (strategy == inst_CIs)
        result = CCFV();
    else if (strategy == inst_TRIGGERS)
        result = triggers();
    else if (strategy == inst_ENUM) {
        if (index_sorts) set_sorts_index(inst_lvl);
        result = inst_sorts();
        if (index_sorts) unset_sorts_index();
    }
    if (!result) return false;
    while (!stack_is_empty(result)) inst_unifiers(stack_pop(result));
    stack_free(result);
    if (stack_is_empty(lemmas)) return false;
    unset_model_index();
#ifdef STATS_INST
#if DEBUG_INST
    my_message(
        "[(%2.2fs), Round %d] Instances: %d\n", insts_time,
        stats_counter_get(insts_stats_rounds),
        doing_ccfv
            ? (stats_counter_get(insts_stats_ccfv) - insts_ccfv_round)
            : (doing_triggers
                   ? (stats_counter_get(insts_stats_triggers) - insts_triggers_round)
                   : (stats_counter_get(insts_stats_sorts) - insts_sorts_round)));
#endif
#if DEBUG_INST > 1
    print_Tstack_DAG(lemmas);
    my_message_return();
#endif
    insts_ccfv_round = stats_counter_get(insts_stats_ccfv);
    insts_triggers_round = stats_counter_get(insts_stats_triggers);
    insts_sorts_round = stats_counter_get(insts_stats_sorts);
    if (doing_ccfv) ++ccfv_success;
#endif
    return true;
}

/**
   \brief instantiate as it was always done, without deleting
   \remark this should disappear, only done to simplify the index building
*/
static void
inst_conservative(void) {
    if (!index_SIG)
        set_SAT_index(UNDEF_LVL);
    else
        set_SIG_index();
    /* Search for CIs and constraints */
    if (!CIs_off && inst_strategy_conservative(inst_CIs, UNDEF_LVL)) return;
    if (!index_SIG && !index_SAT_triggers) {
        unset_model_index();
        set_SIG_index();
    }
    if (!triggers_off && inst_strategy_conservative(inst_TRIGGERS, UNDEF_LVL))
        return;
    /* Try exhaustive sort instantiation */
    if (!enum_one && inst_strategy_conservative(inst_ENUM, UNDEF_LVL)) return;
    if (enum_one && inst_enum()) return;
    unset_model_index();
}

void
inst(Tstack_DAG *Plemmas) {
    unsigned lvl;
    inst_cycle_init();
    /* For now, can only use SAT index for triggers if it also used for CIs */
    assert(!index_SAT_triggers || !index_SIG);
    if (!inst_deletion) {
#ifdef STATS_ONLY
        compute_alternations();
        inst_cycle_done();
        return;
#endif
        inst_conservative();
        /* TODO: Do this for each lemma. And clarify the comment below */
        /* TODO: Awful workaround for not having proper polarity handle */
        for (unsigned i = 0; i < stack_size(lemmas); ++i)
            if (triggers_new)
                set_triggers(DAG_arg(stack_get(lemmas, i), 1), NULL, NULL, POL_POS);
            else
                set_triggers_old(stack_get(lemmas, i));
        stack_merge(*Plemmas, lemmas);
        inst_cycle_done();
        return;
    }
    /* Promote literals from conflicts */
    if (inst_deletion_track_vars)
        inst_promote_vars();
    else
        inst_promote_clauses();
    if (!CIs_off && inst_strategy(inst_CIs, UNDEF_LVL)) {
        /* TODO: define a macro for this */
        stack_merge(*Plemmas, lemmas);
        inst_cycle_done();
        return;
    }
    /* Instantiate triggers */
    do {
        /* Look for trigger instantiations from literals from last round
		   (effectively the first); try successively until no more rounds */
        if (!triggers_off && inst_strategy(inst_TRIGGERS, get_last_lvl())) {
            stack_merge(*Plemmas, lemmas);
            inst_cycle_done();
            return;
        }
    } while (update_lvl_next());
    /* Instantiate sorts, as a last resort */
    if (index_sorts) {
        /* Look for insts in literals from first round on; try successively until
         no more rounds; found insts are marked with deepest lvl + 1 */
        lvl = 0;
        do {
            if (inst_strategy(inst_ENUM, lvl)) {
                stack_merge(*Plemmas, lemmas);
                inst_cycle_done();
                return;
            }
        } while (lvl++ < get_deepest_lvl());
        /* Try again, now with signature table */
        index_sorts = false;
        if (inst_strategy_conservative(inst_ENUM, UNDEF_LVL)) {
#ifdef STATS_INST
            stats_counter_inc(sorts_stats_more_SIG);
#endif
            stack_merge(*Plemmas, lemmas);
            inst_cycle_done();
            index_sorts = true;
            return;
        }
        return;
    }
    /* Try exhaustive sort instantiation */
    if (inst_strategy_conservative(inst_ENUM, UNDEF_LVL)) {
        stack_merge(*Plemmas, lemmas);
        inst_cycle_done();
        return;
    }
    assert(stack_is_empty(lemmas));
    inst_cycle_done();
}

/* TODO: handle this */
/* #ifdef DEBUG */
/*       if (!CIs_off && dump_CIs && stack_size(veriT_lemmas)) */
/*         veriT_dump_literals_and_CIs("unsat", veriT_lemmas); */
/* #endif */

/*
  --------------------------------------------------------------
  Init/done
  --------------------------------------------------------------
*/

void
inst_init() {
    /* TODO: check what this is for and consider getting rid of them */
    /* insts_time = 0; */
    ccfv_calls = 0;
    ccfv_success = 0;
    /* Initialize auxiliary modules */
    inst_create_init();
    inst_del_init();
    inst_index_init();
    CCFV_init();
    CCFV_bckt_init();
    triggers_sel_init();
    triggers_init();

    DAG_PROP_CNF = DAG_prop_new((TFfree) DAG_prop_cnf_free, sizeof(Tstack_DAG));
    DAG_PROP_SYMBS =
            DAG_prop_new((TFfree) DAG_prop_symbols_free, sizeof(Tstack_OSymb));

    DAG_set_hook_resize(fvars_hook_resize);
    DAG_set_hook_free(fvars_hook_free);
    /* TODO: awful workaround, should disappear */
    inst_prep = NULL;
    DAG_set_hook_resize(prep_hook_resize);

    triggers_off = false;
    options_new(
        0, "triggers-off", "Disables trigger-based instantiation", &triggers_off);

    CIs_off = false;
    options_new(0, "CIs-off", "Disables conflict-based instantiation", &CIs_off);

    options_new_int(
        0, "CIs-bound",
        "Limit max number of insts per round in depth-first search.", "UNIT_MAX",
        &CIs_bound);
    CIs_bound = UINT_MAX;

    enum_one = false;
    options_new(0, "enum-one", "Enumerate instances one by one [experimental]", &enum_one);

    options_new_int(
        0, "inst-sorts-threshold",
        "Limit to number of sort insts per quantifier [optimize]", "10^4",
        &insts_sorts_threshold);
    insts_sorts_threshold = 10000;

    index_sorts = false;
    options_new(
        0, "index-sorts", "Use indexed SAT stack for enum. instantiation",
        &index_sorts);

    index_SIG = false;
    options_new(
        0, "index-SIG", "Index CC signature rather than ground model", &index_SIG);

    index_SAT_triggers = false;
    options_new(
        0, "index-SAT-triggers",
        "Uses ground model rather than CC signature for triggers",
        &index_SAT_triggers);

    inst_deletion = false;
    options_new(
        0, "inst-deletion", "Delete instances between instantiation rounds",
        &inst_deletion);

#ifdef POLARITY_FILTER
    opt_bool_required_off = false;
    options_new(
        0, "bool-required-off",
        "Do not prune ground model based on this superset of the prime implicant.",
        &opt_bool_required_off);
#endif
    prime_implicant_off = false;
    options_new(
        0, "prime-implicant-off",
        "Do not prune ground model by computing its prime implicant.",
        &prime_implicant_off);
    prune_cnf_off = false;
    options_new(
        0, "prune-cnf-off", "Do not prune ground model by removing CNF overhead.",
        &prune_cnf_off);
#ifdef STATS_INST
    insts_ccfv_round = 0;
    insts_triggers_round = 0;
    insts_sorts_round = 0;

    insts_stats_rounds =
            stats_counter_new("insts/rounds", "how many instantiation rounds", "%6d");
    insts_stats_ccfv_before_last = stats_counter_new(
        "ccfv/before_last",
        "how many instances times CCFV with deletion was useful", "%6d");
    insts_stats_ccfv =
            stats_counter_new("ccfv/insts", "how many instances CCFV generated", "%6d");
    insts_stats_redundant_ccfv = stats_counter_new(
        "ccfv/redundant", "how many redundant instances CCFV computed", "%6d");
    insts_stats_triggers = stats_counter_new(
        "triggers/insts", "how many instances triggers generated", "%6d");
    insts_stats_redundant_triggers = stats_counter_new(
        "triggers/redundant", "how many redundant instances triggers computed",
        "%6d");
    insts_stats_redundant_triggers_tree = stats_counter_new(
        "triggers/tree_redundant", "how many redundant instances triggers computed",
        "%6d");
    sorts_stats_time = stats_timer_new(
        "sorts_time", "Sorts Instantiation time", "%7.2f", STATS_TIMER_ALL);
    sorts_stats_rounds = stats_counter_new(
        "sorts/rounds", "number of sorts instantiation rounds", "%6d");
    sorts_stats_more_SIG = stats_counter_new(
        "sorts/more_SIG", "how many times went to SIG table", "%6d");
    insts_stats_sorts = stats_counter_new(
        "sorts/insts", "how many instances sorts instantiation generated", "%6d");
    insts_stats_redundant_sorts = stats_counter_new(
        "sorts/redundant", "how many redundant instances sorts inst computed",
        "%6d");
#endif

#ifdef STATS_ONLY
    stats_alt =
            stats_counter_new("alterns", "how many quant alternations", "%6d");
    stats_alt_max =
            stats_counter_new("alterns_max", "deepest alternation in benchmark", "%6d");
#endif
}

void
inst_done() {
#ifdef STATS_INST
    stats_float(
        "ccfv/success", "rate of success (found insts) of CCFV calls", "%7.2f",
        ccfv_calls && ccfv_success ? ccfv_success / ccfv_calls : 0);
#endif
    /* Release auxiliary modules */
    inst_create_done();
    inst_index_done();
    CCFV_done();
    CCFV_bckt_done();
    triggers_sel_done();
    triggers_done();
    inst_del_done();
}

/*
  --------------------------------------------------------------
  Debugging
  --------------------------------------------------------------
*/

void
print_boolean(Tboolean_value bvalue) {
    if (bvalue == 0)
        my_message("FALSE\n");
    else if (bvalue == 1)
        my_message("TRUE\n");
    else
        my_message("UNDEFINED\n");
}

void
print_SAT_stack(void) {
    unsigned i;
    my_DAG_message("SAT stack:\n");
    for (i = 0; i < SAT_literal_stack_n; i++)
        my_DAG_message(
            "lit %d; %D\n", SAT_literal_stack[i], lit_to_DAG(SAT_literal_stack[i]));
    my_message_return();
}

void
print_Pindex(Pindex p_index) {
    unsigned i;
    my_message("Index_pred:\n      #[%s]:\n", DAG_symb_name2(p_index.symbol));
    my_message("Positive signatures:\n");
    if (p_index.signatures[1] && !stack_is_empty(p_index.signatures[1])) {
        for (i = 0; i < stack_size(p_index.signatures[1]); ++i)
            my_DAG_message(
                "\t(%d): [%d]{%d}%D:\n", i,
                CC_abstract(stack_get(p_index.signatures[1], i)),
                stack_get(p_index.signatures[1], i),
                stack_get(p_index.signatures[1], i));
        my_message("--------------\n");
    }
    my_message("Negative signatures:\n");
    if (p_index.signatures[0] && !stack_is_empty(p_index.signatures[0])) {
        for (i = 0; i < stack_size(p_index.signatures[0]); ++i)
            my_DAG_message(
                "\t(%d): [%d]{%d}%D:\n", i,
                CC_abstract(stack_get(p_index.signatures[0], i)),
                stack_get(p_index.signatures[0], i),
                stack_get(p_index.signatures[0], i));
    }
    my_message_return();
}

void
print_Findex(Findex f_index) {
    unsigned i;
    my_message("Index:\n      #[%s]:\n", DAG_symb_name2(f_index.symbol));
    my_message("Signatures:\n");
    if (f_index.signatures && !stack_is_empty(f_index.signatures)) {
        for (i = 0; i < stack_size(f_index.signatures); ++i)
            my_DAG_message(
                "\t(%d): [%d]{%d}%D:\n", i,
                CC_abstract(stack_get(f_index.signatures, i)),
                stack_get(f_index.signatures, i), stack_get(f_index.signatures, i));
        my_message("--------------\n");
    }
    my_message("Diseq terms:\n");
    if (f_index.diseq_terms && !stack_is_empty(f_index.diseq_terms)) {
        for (i = 0; i < stack_size(f_index.diseq_terms); ++i)
            my_DAG_message(
                "\t(%d): [%d]{%d}%D:\n", i,
                CC_abstract(stack_get(f_index.diseq_terms, i)),
                stack_get(f_index.diseq_terms, i), stack_get(f_index.diseq_terms, i));
        my_message("--------------\n");
    }
    my_message_return();
}