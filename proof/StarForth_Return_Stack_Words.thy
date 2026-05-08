theory StarForth_Return_Stack_Words
  imports StarForth_Base
begin

(* =========================================================================
   POST-04: Return Stack Words
   Mirrors: src/word_source/return_stack_words.c
            src/test_runner/modules/return_stack_words_test.c

   These words transfer cells between the data stack and the return stack.
   Both stacks are subject to capacity bounds.
   ======================================================================== *)

(* ── >R ( n -- ) [R: -- n] ─────────────────────────────────────────────── *)
(* Moves TOS from data stack to top of return stack.
   C guards: dsp \<ge> 0 (data stack not empty) AND rsp+1 < STACK_SIZE (return
   stack not full).                                                          *)

definition forth_to_r :: "vm_state \<Rightarrow> vm_state" where
  "forth_to_r vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow>
         if rs_full vm
         then set_error vm
         else vm\<lparr>data_stack := xs, return_stack := x # return_stack vm\<rparr>)"

lemma to_r_normal:
  assumes "data_stack vm = x # xs"
  assumes "\<not> rs_full vm"
  shows "data_stack (forth_to_r vm) = xs"
    and "return_stack (forth_to_r vm) = x # return_stack vm"
  by (simp add: forth_to_r_def assms)+

lemma to_r_ds_decreases:
  assumes "data_stack vm = x # xs"
  assumes "\<not> rs_full vm"
  shows "length (data_stack (forth_to_r vm)) = length (data_stack vm) - 1"
  by (simp add: forth_to_r_def assms)

lemma to_r_rs_increases:
  assumes "data_stack vm = x # xs"
  assumes "\<not> rs_full vm"
  shows "length (return_stack (forth_to_r vm)) = length (return_stack vm) + 1"
  by (simp add: forth_to_r_def assms)

lemma to_r_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_to_r vm)"
  by (simp add: forth_to_r_def set_error_def assms)

lemma to_r_rs_overflow:
  assumes "data_stack vm = x # xs"
  assumes "rs_full vm"
  shows "vm_error (forth_to_r vm)"
  by (simp add: forth_to_r_def set_error_def assms)

(* ── R> ( -- n ) [R: n -- ] ────────────────────────────────────────────── *)
(* Moves top of return stack to data stack TOS.
   C guards: rsp \<ge> 0 (return stack not empty) AND dsp+1 < STACK_SIZE (data
   stack not full).                                                          *)

definition forth_from_r :: "vm_state \<Rightarrow> vm_state" where
  "forth_from_r vm =
    (case return_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow>
         if ds_full vm
         then set_error vm
         else vm\<lparr>data_stack := x # data_stack vm, return_stack := xs\<rparr>)"

lemma from_r_normal:
  assumes "return_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_from_r vm) = x # data_stack vm"
    and "return_stack (forth_from_r vm) = xs"
  by (simp add: forth_from_r_def assms)+

lemma from_r_ds_increases:
  assumes "return_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "length (data_stack (forth_from_r vm)) = length (data_stack vm) + 1"
  by (simp add: forth_from_r_def assms)

lemma from_r_rs_decreases:
  assumes "return_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "length (return_stack (forth_from_r vm)) = length (return_stack vm) - 1"
  by (simp add: forth_from_r_def assms)

lemma from_r_rs_underflow:
  assumes "return_stack vm = []"
  shows "vm_error (forth_from_r vm)"
  by (simp add: forth_from_r_def set_error_def assms)

lemma from_r_ds_overflow:
  assumes "return_stack vm = x # xs"
  assumes "ds_full vm"
  shows "vm_error (forth_from_r vm)"
  by (simp add: forth_from_r_def set_error_def assms)

(* ── R@ ( -- n ) [R: n -- n] ───────────────────────────────────────────── *)
(* Copies (not moves) top of return stack to data stack TOS.
   C guards: rsp \<ge> 0 AND dsp+1 < STACK_SIZE.                               *)

definition forth_r_fetch :: "vm_state \<Rightarrow> vm_state" where
  "forth_r_fetch vm =
    (case return_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow>
         if ds_full vm
         then set_error vm
         else vm\<lparr>data_stack := x # data_stack vm\<rparr>)"

lemma r_fetch_normal:
  assumes "return_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_r_fetch vm) = x # data_stack vm"
  by (simp add: forth_r_fetch_def assms)

lemma r_fetch_rs_unchanged:
  assumes "return_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "return_stack (forth_r_fetch vm) = return_stack vm"
  by (simp add: forth_r_fetch_def assms)

lemma r_fetch_rs_underflow:
  assumes "return_stack vm = []"
  shows "vm_error (forth_r_fetch vm)"
  by (simp add: forth_r_fetch_def set_error_def assms)

lemma r_fetch_ds_overflow:
  assumes "return_stack vm = x # xs"
  assumes "ds_full vm"
  shows "vm_error (forth_r_fetch vm)"
  by (simp add: forth_r_fetch_def set_error_def assms)

(* ── Round-trip correctness: >R then R> restores original state ─────────── *)

lemma to_r_then_from_r:
  assumes "data_stack vm = x # xs"
  assumes "\<not> rs_full vm"
  assumes "\<not> ds_full (forth_to_r vm)"
  assumes "\<not> vm_error vm"
  shows "data_stack (forth_from_r (forth_to_r vm)) = data_stack vm"
    and "return_stack (forth_from_r (forth_to_r vm)) = return_stack vm"
proof -
  have ds: "data_stack (forth_to_r vm) = xs"
    by (simp add: forth_to_r_def assms)
  have rs: "return_stack (forth_to_r vm) = x # return_stack vm"
    by (simp add: forth_to_r_def assms)
  have no_ds_full: "\<not> ds_full (forth_to_r vm)"
    using assms(3) by simp
  show "data_stack (forth_from_r (forth_to_r vm)) = data_stack vm"
    by (simp add: forth_from_r_def forth_to_r_def rs ds no_ds_full assms)
  show "return_stack (forth_from_r (forth_to_r vm)) = return_stack vm"
    by (simp add: forth_from_r_def forth_to_r_def rs ds no_ds_full assms)
qed

(* >R then R@ preserves the return stack and pushes a copy to data stack. *)
lemma to_r_then_r_fetch:
  assumes "data_stack vm = x # xs"
  assumes "\<not> rs_full vm"
  assumes "\<not> ds_full (forth_to_r vm)"
  shows "hd (data_stack (forth_r_fetch (forth_to_r vm))) = x"
    and "return_stack (forth_r_fetch (forth_to_r vm)) = x # return_stack vm"
proof -
  have rs: "return_stack (forth_to_r vm) = x # return_stack vm"
    by (simp add: forth_to_r_def assms)
  show "hd (data_stack (forth_r_fetch (forth_to_r vm))) = x"
    by (simp add: forth_r_fetch_def forth_to_r_def rs assms)
  show "return_stack (forth_r_fetch (forth_to_r vm)) = x # return_stack vm"
    by (simp add: forth_r_fetch_def forth_to_r_def rs assms)
qed

end
