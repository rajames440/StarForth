theory StarForth_Logical_Words
  imports StarForth_Base
begin

(* =========================================================================
   POST-03: Logical and Comparison Words
   Mirrors: src/word_source/logical_words.c
            src/test_runner/modules/logical_words_test.c

   FORTH-79 boolean convention: TRUE = -1 (all bits set), FALSE = 0.
   Bitwise NOT (INVERT) is ~n in C; on HOL int this is -(n+1).
   AND/OR/XOR are bitwise on the machine word.
   ======================================================================== *)

(* ── AND ( n1 n2 -- n3 ) ───────────────────────────────────────────────── *)
(* Bitwise AND.  Uses HOL's AND for integers.                               *)

definition forth_and :: "vm_state \<Rightarrow> vm_state" where
  "forth_and vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := (n1 AND n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma and_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_and vm) = (n1 AND n2) # rest"
  by (simp add: forth_and_def assms)

lemma and_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_and vm) = data_stack (forth_and vm')"
  by (simp add: forth_and_def assms and.commute)

lemma and_with_zero_tos:
  assumes "data_stack vm = 0 # n1 # rest"
  shows "data_stack (forth_and vm) = 0 # rest"
  by (simp add: forth_and_def assms)

lemma and_with_zero_second:
  assumes "data_stack vm = n2 # 0 # rest"
  shows "data_stack (forth_and vm) = 0 # rest"
  by (simp add: forth_and_def assms)

lemma and_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_and vm)"
  by (simp add: forth_and_def set_error_def assms)

lemma and_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_and vm)"
  by (simp add: forth_and_def set_error_def assms)

(* ── OR ( n1 n2 -- n3 ) ────────────────────────────────────────────────── *)

definition forth_or :: "vm_state \<Rightarrow> vm_state" where
  "forth_or vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := (n1 OR n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma or_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_or vm) = (n1 OR n2) # rest"
  by (simp add: forth_or_def assms)

lemma or_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_or vm) = data_stack (forth_or vm')"
  by (simp add: forth_or_def assms or.commute)

lemma or_with_zero_tos:
  assumes "data_stack vm = 0 # n1 # rest"
  shows "data_stack (forth_or vm) = n1 # rest"
  by (simp add: forth_or_def assms)

lemma or_with_zero_second:
  assumes "data_stack vm = n2 # 0 # rest"
  shows "data_stack (forth_or vm) = n2 # rest"
  by (simp add: forth_or_def assms)

lemma or_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_or vm)"
  by (simp add: forth_or_def set_error_def assms)

(* ── XOR ( n1 n2 -- n3 ) ───────────────────────────────────────────────── *)

definition forth_xor :: "vm_state \<Rightarrow> vm_state" where
  "forth_xor vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := (n1 XOR n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma xor_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_xor vm) = (n1 XOR n2) # rest"
  by (simp add: forth_xor_def assms)

lemma xor_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_xor vm) = data_stack (forth_xor vm')"
  by (simp add: forth_xor_def assms xor.commute)

lemma xor_self_zero:
  assumes "data_stack vm = n # n # rest"
  shows "data_stack (forth_xor vm) = 0 # rest"
  by (simp add: forth_xor_def assms)

lemma xor_with_zero:
  assumes "data_stack vm = 0 # n # rest"
  shows "data_stack (forth_xor vm) = n # rest"
  by (simp add: forth_xor_def assms)

lemma xor_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_xor vm)"
  by (simp add: forth_xor_def set_error_def assms)

(* ── INVERT / NOT ( n -- ~n ) ──────────────────────────────────────────── *)
(* C implementation: result = ~n1  (bitwise NOT).
   On HOL int: NOT n = -(n + 1), i.e., the bitwise complement
   for two's-complement representation.                                      *)

definition forth_invert :: "vm_state \<Rightarrow> vm_state" where
  "forth_invert vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow> vm\<lparr>data_stack := (NOT n) # xs\<rparr>)"

lemma invert_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_invert vm) = (NOT n) # xs"
  by (simp add: forth_invert_def assms)

lemma invert_involutive:
  assumes "data_stack vm = n # xs"
  shows "hd (data_stack (forth_invert (forth_invert vm))) = n"
  by (simp add: forth_invert_def assms)

lemma invert_zero_is_neg_one:
  assumes "data_stack vm = 0 # xs"
  shows "hd (data_stack (forth_invert vm)) = -1"
  by (simp add: forth_invert_def assms)

lemma invert_neg_one_is_zero:
  assumes "data_stack vm = (-1) # xs"
  shows "hd (data_stack (forth_invert vm)) = 0"
  by (simp add: forth_invert_def assms)

lemma invert_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_invert vm)"
  by (simp add: forth_invert_def set_error_def assms)

(* ── 0= ( n -- flag ) ──────────────────────────────────────────────────── *)
(* TRUE (-1) if n = 0, FALSE (0) otherwise.                                 *)

definition forth_zero_eq :: "vm_state \<Rightarrow> vm_state" where
  "forth_zero_eq vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n = 0) # xs\<rparr>)"

lemma zero_eq_zero:
  assumes "data_stack vm = 0 # xs"
  shows "data_stack (forth_zero_eq vm) = (-1) # xs"
  by (simp add: forth_zero_eq_def assms)

lemma zero_eq_nonzero:
  assumes "data_stack vm = n # xs"
  assumes "n \<noteq> 0"
  shows "data_stack (forth_zero_eq vm) = 0 # xs"
  by (simp add: forth_zero_eq_def to_forth_bool_def forth_true_def forth_false_def assms)

lemma zero_eq_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_zero_eq vm)"
  by (simp add: forth_zero_eq_def set_error_def assms)

(* ── 0< ( n -- flag ) ──────────────────────────────────────────────────── *)
(* TRUE if n < 0.                                                            *)

definition forth_zero_lt :: "vm_state \<Rightarrow> vm_state" where
  "forth_zero_lt vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n < 0) # xs\<rparr>)"

lemma zero_lt_negative:
  assumes "data_stack vm = n # xs"
  assumes "n < 0"
  shows "data_stack (forth_zero_lt vm) = (-1) # xs"
  by (simp add: forth_zero_lt_def to_forth_bool_def forth_true_def assms)

lemma zero_lt_nonneg:
  assumes "data_stack vm = n # xs"
  assumes "n \<ge> 0"
  shows "data_stack (forth_zero_lt vm) = 0 # xs"
  by (simp add: forth_zero_lt_def to_forth_bool_def forth_false_def assms)

lemma zero_lt_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_zero_lt vm)"
  by (simp add: forth_zero_lt_def set_error_def assms)

(* ── 0> ( n -- flag ) ──────────────────────────────────────────────────── *)
(* TRUE if n > 0.                                                            *)

definition forth_zero_gt :: "vm_state \<Rightarrow> vm_state" where
  "forth_zero_gt vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n > 0) # xs\<rparr>)"

lemma zero_gt_positive:
  assumes "data_stack vm = n # xs"
  assumes "n > 0"
  shows "data_stack (forth_zero_gt vm) = (-1) # xs"
  by (simp add: forth_zero_gt_def to_forth_bool_def forth_true_def assms)

lemma zero_gt_nonpos:
  assumes "data_stack vm = n # xs"
  assumes "n \<le> 0"
  shows "data_stack (forth_zero_gt vm) = 0 # xs"
  by (simp add: forth_zero_gt_def to_forth_bool_def forth_false_def assms)

lemma zero_gt_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_zero_gt vm)"
  by (simp add: forth_zero_gt_def set_error_def assms)

(* ── Mutual exclusion of 0=, 0<, 0> ────────────────────────────────────── *)
(* At most one of {0=, 0<, 0>} can return TRUE for any n.                  *)

lemma zero_tests_exclusive:
  "\<not> (n = (0::int) \<and> n < 0)"
  "\<not> (n = (0::int) \<and> n > 0)"
  "\<not> (n < (0::int) \<and> n > 0)"
  by auto

(* Exactly one of {n=0, n<0, n>0} holds for any integer. *)
lemma zero_tests_exhaustive:
  "n = (0::int) \<or> n < 0 \<or> n > 0"
  by auto

(* ── = ( n1 n2 -- flag ) ───────────────────────────────────────────────── *)

definition forth_eq :: "vm_state \<Rightarrow> vm_state" where
  "forth_eq vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n1 = n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma eq_equal:
  assumes "data_stack vm = n # n # rest"
  shows "data_stack (forth_eq vm) = (-1) # rest"
  by (simp add: forth_eq_def to_forth_bool_def forth_true_def assms)

lemma eq_unequal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n1 \<noteq> n2"
  shows "data_stack (forth_eq vm) = 0 # rest"
  by (simp add: forth_eq_def to_forth_bool_def forth_false_def assms)

lemma eq_symmetric:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_eq vm) = data_stack (forth_eq vm')"
  by (simp add: forth_eq_def assms)

lemma eq_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_eq vm)"
  by (simp add: forth_eq_def set_error_def assms)

(* ── <> ( n1 n2 -- flag ) ──────────────────────────────────────────────── *)

definition forth_neq :: "vm_state \<Rightarrow> vm_state" where
  "forth_neq vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n1 \<noteq> n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma neq_equal:
  assumes "data_stack vm = n # n # rest"
  shows "data_stack (forth_neq vm) = 0 # rest"
  by (simp add: forth_neq_def to_forth_bool_def forth_false_def assms)

lemma neq_unequal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n1 \<noteq> n2"
  shows "data_stack (forth_neq vm) = (-1) # rest"
  by (simp add: forth_neq_def to_forth_bool_def forth_true_def assms)

(* = and <> are complementary. *)
lemma eq_neq_complement:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "hd (data_stack (forth_eq vm)) + hd (data_stack (forth_neq vm)) = -1"
proof (cases "n1 = n2")
  case True
  then show ?thesis
    by (simp add: forth_eq_def forth_neq_def to_forth_bool_def forth_true_def forth_false_def assms)
next
  case False
  then show ?thesis
    by (simp add: forth_eq_def forth_neq_def to_forth_bool_def forth_true_def forth_false_def assms)
qed

(* ── < ( n1 n2 -- flag ) ───────────────────────────────────────────────── *)
(* n2 = TOS.  TRUE if n1 < n2.                                              *)

definition forth_lt :: "vm_state \<Rightarrow> vm_state" where
  "forth_lt vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n1 < n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma lt_less:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n1 < n2"
  shows "data_stack (forth_lt vm) = (-1) # rest"
  by (simp add: forth_lt_def to_forth_bool_def forth_true_def assms)

lemma lt_not_less:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "\<not> (n1 < n2)"
  shows "data_stack (forth_lt vm) = 0 # rest"
  by (simp add: forth_lt_def to_forth_bool_def forth_false_def assms)

lemma lt_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_lt vm)"
  by (simp add: forth_lt_def set_error_def assms)

(* ── > ( n1 n2 -- flag ) ───────────────────────────────────────────────── *)
(* TRUE if n1 > n2.                                                         *)

definition forth_gt :: "vm_state \<Rightarrow> vm_state" where
  "forth_gt vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := to_forth_bool (n1 > n2) # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma gt_greater:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n1 > n2"
  shows "data_stack (forth_gt vm) = (-1) # rest"
  by (simp add: forth_gt_def to_forth_bool_def forth_true_def assms)

lemma gt_not_greater:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "\<not> (n1 > n2)"
  shows "data_stack (forth_gt vm) = 0 # rest"
  by (simp add: forth_gt_def to_forth_bool_def forth_false_def assms)

(* < and > are related by argument reversal. *)
lemma lt_gt_swap:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_lt vm) = data_stack (forth_gt vm')"
  by (simp add: forth_lt_def forth_gt_def assms)

lemma gt_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_gt vm)"
  by (simp add: forth_gt_def set_error_def assms)

end
