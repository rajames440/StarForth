NULL =

bin_PROGRAMS = veriT veriT-SAT

# Rules to generate the LA headers
%-hw.c: %-xx.c.tpl
	$(AM_V_GEN) $(PROG_SED) "s/xx/hw/g;s/XX/HW/g" $< > $@

%-mp.c: %-xx.c.tpl
	$(AM_V_GEN) $(PROG_SED) "s/xx/mp/g;s/XX/MP/g" $< > $@

%-hw.h: %-xx.h.tpl
	$(AM_V_GEN) $(PROG_SED) "s/xx/hw/g;s/XX/HW/g" $< > $@

%-mp.h: %-xx.h.tpl
	$(AM_V_GEN) $(PROG_SED) "s/xx/mp/g;s/XX/MP/g" $< > $@

# Rules to generate the flex, lex
%.c:%.y
	$(AM_V_GEN) $(YACC) $(YFLAGS) -d -o $@ $<

%.c:%.l
	$(AM_V_GEN) $(LEX) $(LFLAGS) -o $@ $<

src/parsers/smtlib2/smtsyn.c: YFLAGS += -p yy2
src/parsers/smtlib2/smtlex.c: LFLAGS += -P yy2

# Source files that are generated
BUILT_SOURCES = \
	src/arith/LA-hw.c \
	src/arith/LA-hw.h \
	src/arith/LA-mp.c \
	src/arith/LA-mp.h \
	src/arith/numbers-hw.h \
	src/arith/numbers-mp.h \
	src/arith/simplex-hw.c \
	src/arith/simplex-hw.h \
	src/arith/simplex-mp.c \
	src/arith/simplex-mp.h \
	src/parsers/smtlib2/smtsyn.c \
	src/parsers/smtlib2/smtlex.c \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	src/parsers/smtlib2/smtsyn.h \
	$(NULL)

# veriT configuration
veriT_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(NULL)

veriT_LDADD = \
	$(AM_LDADD) \
	$(NULL)

dist_veriT_SOURCES = \
	src/parsers/smtlib2/smtsyn.h \
	$(NULL)

REGULAR_SOURCES = \
	src/veriT-config.h \
	src/arith/gcd-cache.def \
	src/bool/cnf.c.tpl \
	src/arith/eq-store.c \
	src/arith/eq-store.h \
	src/arith/LA.c \
	src/arith/LA.h \
	src/arith/LA-pre.c \
	src/arith/LA-pre.h \
	src/arith/numbers-hw.c \
	src/arith/numbers-mp.c \
	src/arith/simplex.c \
	src/arith/simplex.h \
	src/arith/totality.c \
	src/arith/totality.h \
	src/bool/bool.c \
	src/bool/bool.h \
	src/bool/clause.c \
	src/bool/cnf.c \
	src/bool/cnf.h \
	src/bool/literal.c \
	src/bool/literal.h \
	src/complete.c \
	src/complete.h \
	src/congruence/congruence.c \
	src/congruence/congruence.h \
	src/hint.c \
	src/hint.h \
	src/instantiation/ccfv-bckt.c \
	src/instantiation/ccfv-bckt.h \
	src/instantiation/ccfv.c \
	src/instantiation/ccfv.h \
	src/instantiation/ccfv-constr.c \
	src/instantiation/ccfv-constr.h \
	src/instantiation/ccfv-mod.c \
	src/instantiation/ccfv-mod.h \
	src/instantiation/discrimination-tree.c \
	src/instantiation/discrimination-tree.h \
	src/instantiation/free-vars.c \
	src/instantiation/free-vars.h \
	src/instantiation/inst-create.c \
	src/instantiation/inst-create.h \
	src/instantiation/inst-del.c \
	src/instantiation/inst-del.h \
	src/instantiation/inst-enum.c \
	src/instantiation/inst-enum.h \
	src/instantiation/inst-index.c \
	src/instantiation/inst-index.h \
	src/instantiation/inst-man.c \
	src/instantiation/inst-man.h \
	src/instantiation/inst-pre.c \
	src/instantiation/inst-pre.h \
	src/instantiation/inst-strategy.h \
	src/instantiation/inst-symbs.c \
	src/instantiation/inst-symbs.h \
	src/instantiation/inst-trigger.c \
	src/instantiation/inst-trigger.h \
	src/instantiation/inst-trigger-selection.c \
	src/instantiation/inst-trigger-selection.h \
	src/instantiation/ujobs.c \
	src/instantiation/ujobs.h \
	src/instantiation/unify.c \
	src/instantiation/unify.h \
	src/instantiation/syntactic-unify.c \
	src/instantiation/syntactic-unify.h \
	src/parsers/dimacs/dimacs.c \
	src/parsers/dimacs/dimacs.h \
	src/parsers/smtlib2/parser_macro.h \
	src/parsers/smtlib2/smtfun.c \
	src/parsers/smtlib2/smtfun.h \
	src/parsers/smtlib2/smtlib2.h \
	src/parsers/smtlib2/smttypes.h \
	src/pre/bclauses.c \
	src/pre/bclauses.h \
	src/pre/bfun-elim.c \
	src/pre/bfun-elim.h \
	src/pre/binder-rename.c \
	src/pre/binder-rename.h \
	src/pre/connectives.c \
	src/pre/connectives.h \
	src/pre/distinct-elim.c \
	src/pre/distinct-elim.h \
	src/pre/fix-trigger.c \
	src/pre/fix-trigger.h \
	src/pre/HOL.c \
	src/pre/HOL.h \
	src/pre/ite-elim.c \
	src/pre/ite-elim.h \
	src/pre/let-elim.c \
	src/pre/let-elim.h \
	src/pre/nary-elim.c \
	src/pre/nary-elim.h \
	src/pre/pm.c \
	src/pre/pm.h \
	src/pre/pre.c \
	src/pre/pre.h \
	src/pre/qnt-tidy.h \
	src/pre/qnt-tidy.c \
	src/pre/qnt-trigger.h \
	src/pre/qnt-trigger.c \
	src/pre/rare-symb.h \
	src/pre/rare-symb.c \
	src/pre/simp-formula-sat.c \
	src/pre/simp-formula-sat.h \
	src/pre/simplify-AC.c \
	src/pre/simplify-AC.h \
	src/pre/simplify.c \
	src/pre/simplify.h \
	src/pre/simp-node-proof.h \
	src/pre/simp-node-proof.c \
	src/pre/simp-sym.h \
	src/pre/simp-sym.c \
	src/pre/simp-unit.h \
	src/pre/simp-unit.c \
	src/pre/skolem.h \
	src/pre/skolem.c \
	src/pre/qsimp-by-unification.h \
	src/pre/qsimp-by-unification.c \
	src/proof/proof.c \
	src/proof/proof-checking.c \
	src/proof/proof-checking.h \
	src/proof/proof.h \
	src/proof/proof-id.c \
	src/proof/proof-id.h \
	src/proof/proof-lemma-hash.c \
	src/proof/proof-lemma-hash.h \
	src/proof/proof-output.c \
	src/proof/proof-output.h \
	src/proof/proof-rules.c \
	src/proof/proof-rules.h \
	src/proof/proof-rules-tautologies.c \
	src/proof/proof-rules-tautologies.h \
	src/proof/proof-sat-solver.c \
	src/proof/proof-sat-solver.h \
	src/proof/proof-step.c \
	src/proof/proof-step.h \
	src/proof/proof-step-hash.c \
	src/proof/proof-step-hash.h \
	src/proof/proof-step-table.c \
	src/proof/proof-step-table.h \
	src/proof/proof-subproof.c \
	src/proof/proof-subproof.h \
	src/proof/proof-type.c \
	src/proof/proof-type.h \
	src/proof/proof-unsat-core.c \
	src/proof/proof-unsat-core.h \
	src/proof/proof-print.c \
	src/proof/proof-print.h \
	src/response.c \
	src/response.h \
	src/symbolic/context-handling.c \
	src/symbolic/context-handling.h \
	src/symbolic/context-recursion.c \
	src/symbolic/context-recursion.h \
	src/symbolic/context-recursion-proof.c \
	src/symbolic/context-recursion-proof.h \
	src/symbolic/DAG-arith.c \
	src/symbolic/DAG-arith.h \
	src/symbolic/DAG.c \
	src/symbolic/DAG-flag.c \
	src/symbolic/DAG-flag.h \
	src/symbolic/DAG.h \
	src/symbolic/DAG-print.c \
	src/symbolic/DAG-print.h \
	src/symbolic/DAG-prop.c \
	src/symbolic/DAG-prop.h \
	src/symbolic/DAG-ptr.h \
	src/symbolic/DAG-smtlib.c \
	src/symbolic/DAG-sort.c \
	src/symbolic/DAG-sort.h \
	src/symbolic/DAG-sort-pm.c \
	src/symbolic/DAG-sort-pm.h \
	src/symbolic/DAG-stat.c \
	src/symbolic/DAG-stat.h \
	src/symbolic/DAG-subst.c \
	src/symbolic/DAG-subst.h \
	src/symbolic/DAG-symb.c \
	src/symbolic/DAG-symb-DAG.c \
	src/symbolic/DAG-symb-DAG.h \
	src/symbolic/DAG-symb.h \
	src/symbolic/DAG-tmp.c \
	src/symbolic/DAG-tmp.h \
	src/symbolic/dbg-trigger.c \
	src/symbolic/dbg-trigger.h \
	src/symbolic/polarities.c \
	src/symbolic/polarities.h \
	src/symbolic/qnt-utils.c \
	src/symbolic/qnt-utils.h \
	src/symbolic/recursion.c \
	src/symbolic/recursion.h \
	src/symbolic/veriT-DAG.h \
	src/symbolic/veriT-status.h \
	src/undo.c \
	src/undo.h \
	src/utils/assoc.h \
	src/utils/bitset.h \
	src/utils/bitset.c \
	src/utils/dll.h \
	src/utils/dll.c \
	src/utils/dll-DAG.h \
	src/utils/general.h \
	src/utils/general.c \
	src/utils/hash.c \
	src/utils/hash.h \
	src/utils/hashmap.c \
	src/utils/hashmap.h \
	src/utils/h.h \
	src/utils/hk.h \
	src/utils/h-util.c \
	src/utils/h-util.h \
	src/utils/list.c \
	src/utils/list.h \
	src/utils/nonce.c \
	src/utils/nonce.h \
	src/utils/options.c \
	src/utils/options.h \
	src/utils/stack.h \
	src/utils/statistics.c \
	src/utils/statistics.h \
	src/utils/sys.c \
	src/utils/sys.h \
	src/utils/table.c \
	src/utils/table.h \
	src/utils/types.h \
	src/utils/veriT-qsort.c \
	src/utils/veriT-qsort.h \
	src/utils/veriT-signal.c \
	src/utils/veriT-signal.h \
	src/utils/ha.c.tpl \
	src/veriT.c \
	src/veriT.h \
	src/veriT-state.c \
	src/veriT-state.h \
	src/SAT/veriT-SAT.c \
	src/SAT/veriT-SAT.h \
	$(NULL)

veriT_SOURCES = \
	$(BUILT_SOURCES) \
	$(REGULAR_SOURCES) \
	src/main.c \
	$(NULL)

# veriT-SAT configuration
veriT_SAT_SOURCES = \
	src/veriT-SAT/main.c \
	src/SAT/veriT-SAT.c \
	src/SAT/veriT-SAT.h \
	$(NULL)

veriT_SAT_CFLAGS = \
	$(AM_CFLAGS) \
	$(NULL)

veriT_SAT_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-DSAT_STANDALONE \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src \
	$(NULL)


