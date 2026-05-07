theory StarForth_Base
  imports Main
begin

(* =========================================================================
   StarForth_Base — Shared types and VM state model
   =========================================================================

   This theory defines the foundational types used by all proof modules.

   Overflow note: StarForth uses cell_t = signed long (64-bit on x86-64).
   We model it as HOL int (arbitrary precision).  Proofs therefore hold for
   mathematical integers and hold in C provided no intermediate value
   overflows.  Wrapping-word semantics (word 64) are deferred to a future
   milestone.
   ======================================================================== *)

(* ── Cell type ──────────────────────────────────────────────────────────── *)

type_synonym cell = int

(* ── FORTH-79 boolean convention ────────────────────────────────────────── *)
(* Matches the C macros in src/word_source/logical_words.c:
     #define FORTH_TRUE  ((cell_t)-1)
     #define FORTH_FALSE ((cell_t) 0)                                        *)

definition forth_true :: cell where
  "forth_true = -1"

definition forth_false :: cell where
  "forth_false = 0"

definition to_forth_bool :: "bool \<Rightarrow> cell" where
  "to_forth_bool b = (if b then forth_true else forth_false)"

lemma to_forth_bool_True [simp]:
  "to_forth_bool True = -1"
  by (simp add: to_forth_bool_def forth_true_def)

lemma to_forth_bool_False [simp]:
  "to_forth_bool False = 0"
  by (simp add: to_forth_bool_def forth_false_def)

lemma to_forth_bool_eq:
  "to_forth_bool b = (if b then -1 else 0)"
  by (cases b; simp)

(* ── Stack type ─────────────────────────────────────────────────────────── *)
(* Top-of-stack is the head of the list, matching C: data_stack[dsp] = TOS. *)

type_synonym forth_stack = "cell list"

(* Matches #define STACK_SIZE 1024 in include/vm.h *)
definition STACK_SIZE :: nat where
  "STACK_SIZE = 1024"

(* ── VM state record ─────────────────────────────────────────────────────── *)
(* Minimal model: stacks, abstract memory, error flag.
   Compilation state, physics metadata, and the instruction pointer are
   omitted here; they are not needed to prove primitive stack effects.
   They will be added when interpreter-loop proofs are developed.           *)

record vm_state =
  data_stack   :: forth_stack
  return_stack :: forth_stack
  memory       :: "nat \<Rightarrow> cell"   \<comment> \<open>abstract byte-addressed flat memory\<close>
  vm_error     :: bool

(* ── Well-formedness ─────────────────────────────────────────────────────── *)

definition wf_vm :: "vm_state \<Rightarrow> bool" where
  "wf_vm vm \<longleftrightarrow>
     length (data_stack vm)   \<le> STACK_SIZE \<and>
     length (return_stack vm) \<le> STACK_SIZE \<and>
     \<not> vm_error vm"

(* ── Error signalling ────────────────────────────────────────────────────── *)

definition set_error :: "vm_state \<Rightarrow> vm_state" where
  "set_error vm = vm\<lparr>vm_error := True\<rparr>"

lemma set_error_error [simp]:
  "vm_error (set_error vm)"
  by (simp add: set_error_def)

lemma set_error_data_stack [simp]:
  "data_stack (set_error vm) = data_stack vm"
  by (simp add: set_error_def)

lemma set_error_return_stack [simp]:
  "return_stack (set_error vm) = return_stack vm"
  by (simp add: set_error_def)

(* ── Capacity predicate ──────────────────────────────────────────────────── *)

definition ds_full :: "vm_state \<Rightarrow> bool" where
  "ds_full vm \<longleftrightarrow> length (data_stack vm) \<ge> STACK_SIZE"

definition rs_full :: "vm_state \<Rightarrow> bool" where
  "rs_full vm \<longleftrightarrow> length (return_stack vm) \<ge> STACK_SIZE"

end
