/*
  --------------------------------------------------------------
  ite (functions) removing in formulas
  --------------------------------------------------------------
*/

#include "pre/bfun-elim.h"

#include "symbolic/DAG-tmp.h"
#include "symbolic/veriT-DAG.h"
#include "utils/general.h"
#include "utils/types.h"

#include <assert.h>
#include <stdlib.h>

/*
  --------------------------------------------------------------
  Eliminating quantifiers over Booleans
  --------------------------------------------------------------
*/

/**
   \brief substitutes every node by its DAG_tmp_DAG[node] in a term
   \param src the term
   \return the modified term
   \remark tree-linear */
static TDAG
bfun_elim_quant_subst(TDAG src) {
    unsigned i;
    TDAG *PDAG, dest;
    if (DAG_tmp_DAG[src]) return DAG_dup(DAG_tmp_DAG[src]);
    MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
    for (i = 0; i < DAG_arity(src); i++)
        PDAG[i] = bfun_elim_quant_subst(DAG_arg(src, i));
    dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
    for (i = 0; i < DAG_arity(src); i++) DAG_free(DAG_arg(dest, i));
    return dest;
}

/**
   \brief eliminates Boolean quantifiers at the top level
   \param src the DAG of the formula
   \return the orignal formula if no top-level quantifier or
   if no quantified Boolean variables
   \remark may use DAG_tmp_DAG[node] on every sub-node
   \remark useing DAG_tmp_DAG in substitution may interfere in a
   non-fundamental way with the use at the caller.  This is not
   important since if a DAG has DAG_tmp_DAG[DAG], it does not contain
   variables thanks to variable renaming: variables are unique symbols,
   and should therefore not appear free */
static TDAG
bfun_elim_quant_aux(TDAG src) {
    unsigned i, j = 0;
    if (!quantifier(DAG_symb(src))) return DAG_dup(src);
    for (i = 0; i < DAG_arity(src) - 1u; i++)
        if (DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN) j++;
    if (!j)
        return DAG_dup(src);
    else {
        TDAG *PDAG_bool = NULL, *PDAG = NULL, *PDAG2;
        TDAG dest;
        unsigned k;
        MY_MALLOC(PDAG_bool, j * sizeof(TDAG));
        MY_MALLOC(PDAG2, (1u << j) * sizeof(TDAG));
        if (DAG_arity(src) - j - 1) {
            MY_MALLOC(PDAG, (DAG_arity(src) - j) * sizeof(TDAG));
        }
        for (i = 0, j = 0; i < DAG_arity(src) - 1u; i++)
            if (DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN) {
                if (DAG_tmp_DAG[DAG_arg(src, i)])
                    my_error("bfun_elim_quant: internal error\n");
                PDAG_bool[j++] = DAG_arg(src, i);
            } else
                PDAG[i - j] = DAG_arg(src, i);
        for (i = 0; i < (1u << j); i++) {
            for (k = 0; k < j; k++)
                DAG_tmp_DAG[PDAG_bool[k]] = ((1u << k) & i) ? DAG_TRUE : DAG_FALSE;
            PDAG2[i] = bfun_elim_quant_subst(DAG_arg_last(src));
        }
        for (k = 0; k < j; k++) DAG_tmp_DAG[PDAG_bool[k]] = DAG_NULL;
        dest = DAG_dup(DAG_new(
            (DAG_symb(src) == QUANTIFIER_EXISTS) ? CONNECTOR_OR : CONNECTOR_AND,
            1u << j, PDAG2));
        for (i = 0; i < (1u << j); i++) DAG_free(DAG_arg(dest, i));
        if (DAG_arity(src) - j - 1u) {
            PDAG[DAG_arity(src) - j - 1u] = dest;
            dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src) - j, PDAG));
            DAG_free(DAG_arg(dest, DAG_arity(src) - j - 1u));
        }
        free(PDAG_bool);
        return dest;
    }
}

static TDAG
bfun_elim_quant_tree(TDAG src) {
    if (!DAG_quant(src)) return DAG_dup(src);
    if (DAG_tmp_DAG[src] != DAG_NULL) return DAG_dup(DAG_tmp_DAG[src]);
    if (quantifier(DAG_symb(src))) {
        unsigned i;
        TDAG *PDAG, dest;
        MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
        for (i = 0; i < DAG_arity(src) - 1u; i++) PDAG[i] = DAG_arg(src, i);
        PDAG[i] = bfun_elim_quant_tree(DAG_arg_last(src));
        dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
        DAG_free(DAG_arg(dest, i));
        src = dest;
        dest = bfun_elim_quant_aux(src);
        DAG_free(src);
        return dest;
    } else {
        unsigned i;
        TDAG *PDAG, dest;
        MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
        for (i = 0; i < DAG_arity(src); i++)
            PDAG[i] = bfun_elim_quant_tree(DAG_arg(src, i));
        dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
        for (i = 0; i < DAG_arity(src); i++) DAG_free(DAG_arg(dest, i));
        return dest;
    }
}

static TDAG
bfun_elim_quant_DAG(TDAG src) {
    if (!DAG_quant(src)) return DAG_dup(src);
    if (DAG_tmp_DAG[src]) return DAG_dup(DAG_tmp_DAG[src]);
    if (quantifier(DAG_symb(src))) {
        unsigned i;
        TDAG *PDAG, dest, tmp;
        MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
        for (i = 0; i < DAG_arity(src) - 1u; i++) PDAG[i] = DAG_arg(src, i);
        PDAG[i] = bfun_elim_quant_tree(DAG_arg_last(src));
        dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
        DAG_free(DAG_arg(dest, i));
        tmp = bfun_elim_quant_aux(dest);
        DAG_tmp_DAG[src] = tmp;
        DAG_free(dest);
        return tmp;
    } else {
        unsigned i;
        TDAG *PDAG, dest;
        MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
        for (i = 0; i < DAG_arity(src); i++)
            PDAG[i] = bfun_elim_quant_DAG(DAG_arg(src, i));
        dest = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
        for (i = 0; i < DAG_arity(src); i++) DAG_free(DAG_arg(dest, i));
        DAG_tmp_DAG[src] = dest;
        return dest;
    }
}

static TDAG
bfun_elim_quant(TDAG src) {
    TDAG dest = bfun_elim_quant_DAG(src);
    DAG_tmp_reset_DAG(src);
    return dest;
}

/*
  --------------------------------------------------------------
  Eliminating Boolean arguments of functions
  --------------------------------------------------------------
*/

/**
   if top most construction is a function with Boolean argument
   which is not true or false, adequately introduces ite functions
   so that arguments are true or false
   \param src the term to rewrite
   \return the rewritten term
   \remark Destructive */
static TDAG
bfun_clean(TDAG src) {
    unsigned i, pos;
    TDAG *PDAG;
    TDAG dest, when_true, when_false;
    if (
        !DAG_arity(src) || boolean_connector(DAG_symb(src)) ||
        DAG_symb(src) == FUNCTION_ITE)
        return src;
    /* PF check if function with Boolean argument */
    for (i = 0; i < DAG_arity(src); i++)
        if (
            DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN &&
            DAG_arg(src, i) != DAG_TRUE && DAG_arg(src, i) != DAG_FALSE)
            break;
    if (i == DAG_arity(src)) return src;
    pos = i;
    MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
    for (i = 0; i < DAG_arity(src); i++)
        PDAG[i] = (i == pos) ? DAG_TRUE : DAG_arg(src, i);
    when_true = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
    MY_MALLOC(PDAG, DAG_arity(src) * sizeof(TDAG));
    for (i = 0; i < DAG_arity(src); i++)
        PDAG[i] = (i == pos) ? DAG_FALSE : DAG_arg(src, i);
    when_false = DAG_dup(DAG_new(DAG_symb(src), DAG_arity(src), PDAG));
    MY_MALLOC(PDAG, 3 * sizeof(TDAG));
    PDAG[0] = DAG_arg(src, pos);
    PDAG[1] = when_true = bfun_clean(when_true);
    PDAG[2] = when_false = bfun_clean(when_false);
    dest = DAG_new(
        (DAG_sort(src) == SORT_BOOLEAN) ? CONNECTOR_ITE : FUNCTION_ITE, 3, PDAG);
    DAG_dup(dest);
    DAG_free(when_true);
    DAG_free(when_false);
    DAG_free(src);
    return dest;
}

/**
   if top most construction is a quantifier with a Boolean variable,
   raise an error.
   Should in the future be changed to rewrite the formula to eliminate
   this quantifier
   \param src the term to rewrite
   \return the term */
static TDAG
bquant_clean(TDAG src) {
    unsigned i;
    assert(DAG_arity(src));
    for (i = 0; i < DAG_arity(src) - 1u; i++)
        if (DAG_sort(DAG_arg(src, i)) == SORT_BOOLEAN)
            my_error("bquant_clean: quantified Boolean variable, unsupported\n");
    return src;
}

static int
bfun_elim_aux(TDAG DAG) {
    unsigned i;
    int changed = 0;
    if (DAG_tmp_DAG[DAG]) return DAG != DAG_tmp_DAG[DAG];
    if (binder(DAG_symb(DAG)))
        changed |= bfun_elim_aux(DAG_arg_last(DAG));
    else
        for (i = 0; i < DAG_arity(DAG); i++)
            changed |= bfun_elim_aux(DAG_arg(DAG, i));
    if (changed) {
        TDAG *PDAG, tmp;
        MY_MALLOC(PDAG, DAG_arity(DAG) * sizeof(TDAG));
        for (i = 0; i < DAG_arity(DAG); i++) PDAG[i] = DAG_tmp_DAG[DAG_arg(DAG, i)];
        tmp = DAG_dup(DAG_new(DAG_symb(DAG), DAG_arity(DAG), PDAG));
        DAG_tmp_DAG[DAG] = tmp;
    } else
        DAG_tmp_DAG[DAG] = DAG_dup(DAG);
    if (!binder(DAG_symb(DAG))) {
        TDAG tmp = bfun_clean(DAG_tmp_DAG[DAG]);
        DAG_tmp_DAG[DAG] = tmp;
    } else {
        TDAG tmp = bquant_clean(DAG_tmp_DAG[DAG]);
        DAG_tmp_DAG[DAG] = tmp;
    }
    return DAG != DAG_tmp_DAG[DAG];
}

static void
bfun_elim_clean(TDAG DAG) {
    unsigned i;
    if (!DAG_tmp_DAG[DAG]) return;
    for (i = 0; i < DAG_arity(DAG); i++) bfun_elim_clean(DAG_arg(DAG, i));
    DAG_free(DAG_tmp_DAG[DAG]);
    DAG_tmp_DAG[DAG] = DAG_NULL;
}

/*
  --------------------------------------------------------------
  Public functions
  --------------------------------------------------------------
*/

TDAG
bfun_elim(TDAG src) {
    TDAG dest;
    DAG_tmp_reserve();
    src = bfun_elim_quant(src);
    bfun_elim_aux(src);
    dest = DAG_dup(DAG_tmp_DAG[src]);
    bfun_elim_clean(src);
    DAG_free(src);
    DAG_tmp_release();
    return dest;
}

void
bfun_elim_array(unsigned n, TDAG *Psrc) {
    unsigned i;
    TDAG *PDAG;
    DAG_tmp_reserve();
    MY_MALLOC(PDAG, n * sizeof(TDAG));
    for (i = 0; i < n; i++) {
        TDAG dest = bfun_elim_quant(Psrc[i]);
        DAG_free(Psrc[i]);
        Psrc[i] = dest;
    }
    for (i = 0; i < n; i++) {
        bfun_elim_aux(Psrc[i]);
        PDAG[i] = DAG_dup(DAG_tmp_DAG[Psrc[i]]);
    }
    for (i = 0; i < n; i++) {
        bfun_elim_clean(Psrc[i]);
        DAG_free(Psrc[i]);
        Psrc[i] = PDAG[i];
    }
    free(PDAG);
    DAG_tmp_release();
}