theory StarForth_Stack_Words
  imports StarForth_Base
begin

(* =========================================================================
   POST-01: Stack Manipulation Words
   Mirrors: src/word_source/stack_words.c
            src/test_runner/modules/stack_words_test.c
   ======================================================================== *)

(* ── DROP ( n -- ) ──────────────────────────────────────────────────────── *)

definition forth_drop :: "vm_state \<Rightarrow> vm_state" where
  "forth_drop vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | _ # xs \<Rightarrow> vm\<lparr>data_stack := xs\<rparr>)"

lemma drop_normal:
  assumes "data_stack vm = x # xs"
  shows "data_stack (forth_drop vm) = xs \<and> vm_error (forth_drop vm) = vm_error vm"
  by (simp add: forth_drop_def assms)

lemma drop_depth:
  assumes "data_stack vm = x # xs"
  shows "length (data_stack (forth_drop vm)) = length (data_stack vm) - 1"
  by (simp add: forth_drop_def assms)

lemma drop_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_drop vm)"
  by (simp add: forth_drop_def set_error_def assms)

(* ── DUP ( n -- n n ) ───────────────────────────────────────────────────── *)

definition forth_dup :: "vm_state \<Rightarrow> vm_state" where
  "forth_dup vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow>
         if ds_full vm
         then set_error vm
         else vm\<lparr>data_stack := x # x # xs\<rparr>)"

lemma dup_normal:
  assumes "data_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_dup vm) = x # x # xs"
  by (simp add: forth_dup_def assms)

lemma dup_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_dup vm)"
  by (simp add: forth_dup_def set_error_def assms)

lemma dup_overflow:
  assumes "data_stack vm = x # xs"
  assumes "ds_full vm"
  shows "vm_error (forth_dup vm)"
  by (simp add: forth_dup_def set_error_def assms)

lemma dup_increases_depth:
  assumes "data_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  shows "length (data_stack (forth_dup vm)) = length (data_stack vm) + 1"
  by (simp add: forth_dup_def assms)

(* ── ?DUP ( n -- n n | 0 ) ─────────────────────────────────────────────── *)
(* Duplicates top if non-zero; leaves zero unchanged.                        *)

definition forth_qdup :: "vm_state \<Rightarrow> vm_state" where
  "forth_qdup vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow>
         if x = 0
         then vm
         else if ds_full vm
              then set_error vm
              else vm\<lparr>data_stack := x # x # xs\<rparr>)"

lemma qdup_zero:
  assumes "data_stack vm = 0 # xs"
  shows "data_stack (forth_qdup vm) = 0 # xs"
  by (simp add: forth_qdup_def assms)

lemma qdup_zero_noop:
  assumes "data_stack vm = 0 # xs"
  shows "forth_qdup vm = vm"
  by (simp add: forth_qdup_def assms)

lemma qdup_nonzero:
  assumes "data_stack vm = x # xs"
  assumes "x \<noteq> 0"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_qdup vm) = x # x # xs"
  by (simp add: forth_qdup_def assms)

lemma qdup_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_qdup vm)"
  by (simp add: forth_qdup_def set_error_def assms)

lemma qdup_overflow:
  assumes "data_stack vm = x # xs"
  assumes "x \<noteq> 0"
  assumes "ds_full vm"
  shows "vm_error (forth_qdup vm)"
  by (simp add: forth_qdup_def set_error_def assms)

(* ── SWAP ( n1 n2 -- n2 n1 ) ───────────────────────────────────────────── *)
(* Stack effect (right = TOS): n1=second, n2=TOS.
   In list notation (head = TOS): [n2, n1, rest] \<rightarrow> [n1, n2, rest]         *)

definition forth_swap :: "vm_state \<Rightarrow> vm_state" where
  "forth_swap vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := n1 # n2 # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma swap_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_swap vm) = n1 # n2 # rest"
  by (simp add: forth_swap_def assms)

lemma swap_depth_preserved:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "length (data_stack (forth_swap vm)) = length (data_stack vm)"
  by (simp add: forth_swap_def assms)

lemma swap_involutive:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_swap (forth_swap vm)) = data_stack vm"
  by (simp add: forth_swap_def assms)

lemma swap_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_swap vm)"
  by (simp add: forth_swap_def set_error_def assms)

lemma swap_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_swap vm)"
  by (simp add: forth_swap_def set_error_def assms)

(* ── OVER ( n1 n2 -- n1 n2 n1 ) ───────────────────────────────────────── *)
(* n2=TOS, n1=second. Copies n1 (second) to new TOS.
   In list: [n2, n1, rest] \<rightarrow> [n1, n2, n1, rest]                           *)

definition forth_over :: "vm_state \<Rightarrow> vm_state" where
  "forth_over vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow>
         if ds_full vm
         then set_error vm
         else vm\<lparr>data_stack := n1 # n2 # n1 # rest\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma over_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_over vm) = n1 # n2 # n1 # rest"
  by (simp add: forth_over_def assms)

lemma over_second_preserved:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "\<not> ds_full vm"
  shows "hd (data_stack (forth_over vm)) = n1"
  by (simp add: forth_over_def assms)

lemma over_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_over vm)"
  by (simp add: forth_over_def set_error_def assms)

lemma over_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_over vm)"
  by (simp add: forth_over_def set_error_def assms)

lemma over_overflow:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "ds_full vm"
  shows "vm_error (forth_over vm)"
  by (simp add: forth_over_def set_error_def assms)

(* ── ROT ( n1 n2 n3 -- n2 n3 n1 ) ─────────────────────────────────────── *)
(* n3=TOS, n2=second, n1=third.  Brings n1 (third) to TOS.
   In list: [n3, n2, n1, rest] \<rightarrow> [n1, n3, n2, rest]

   Verified against C:
     n3 = data_stack[dsp]       (TOS)
     n2 = data_stack[dsp-1]     (second)
     n1 = data_stack[dsp-2]     (third)
     data_stack[dsp]   = n1     (new TOS)
     data_stack[dsp-1] = n3
     data_stack[dsp-2] = n2                                                *)

definition forth_rot :: "vm_state \<Rightarrow> vm_state" where
  "forth_rot vm =
    (case data_stack vm of
       n3 # n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := n1 # n3 # n2 # rest\<rparr>
     | _                   \<Rightarrow> set_error vm)"

lemma rot_normal:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "data_stack (forth_rot vm) = n1 # n3 # n2 # rest"
  by (simp add: forth_rot_def assms)

lemma rot_depth_preserved:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "length (data_stack (forth_rot vm)) = length (data_stack vm)"
  by (simp add: forth_rot_def assms)

lemma rot_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_rot vm)"
  by (simp add: forth_rot_def set_error_def assms)

lemma rot_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_rot vm)"
  by (simp add: forth_rot_def set_error_def assms)

lemma rot_underflow_two:
  assumes "data_stack vm = [x, y]"
  shows "vm_error (forth_rot vm)"
  by (simp add: forth_rot_def set_error_def assms)

(* ── -ROT ( n1 n2 n3 -- n3 n1 n2 ) ─────────────────────────────────────── *)
(* Reverse rotation: brings TOS (n3) to third position.
   In list: [n3, n2, n1, rest] \<rightarrow> [n2, n1, n3, rest]

   Verified against C:
     data_stack[dsp]   = n2     (new TOS)
     data_stack[dsp-1] = n1
     data_stack[dsp-2] = n3                                                *)

definition forth_nrot :: "vm_state \<Rightarrow> vm_state" where
  "forth_nrot vm =
    (case data_stack vm of
       n3 # n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := n2 # n1 # n3 # rest\<rparr>
     | _                   \<Rightarrow> set_error vm)"

lemma nrot_normal:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "data_stack (forth_nrot vm) = n2 # n1 # n3 # rest"
  by (simp add: forth_nrot_def assms)

lemma nrot_depth_preserved:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "length (data_stack (forth_nrot vm)) = length (data_stack vm)"
  by (simp add: forth_nrot_def assms)

(* ROT and -ROT are mutual inverses on the data stack. *)
lemma rot_nrot_inverse:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "data_stack (forth_nrot (forth_rot vm)) = data_stack vm"
  by (simp add: forth_nrot_def forth_rot_def assms)

lemma nrot_rot_inverse:
  assumes "data_stack vm = n3 # n2 # n1 # rest"
  shows "data_stack (forth_rot (forth_nrot vm)) = data_stack vm"
  by (simp add: forth_nrot_def forth_rot_def assms)

lemma nrot_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_nrot vm)"
  by (simp add: forth_nrot_def set_error_def assms)

lemma nrot_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_nrot vm)"
  by (simp add: forth_nrot_def set_error_def assms)

lemma nrot_underflow_two:
  assumes "data_stack vm = [x, y]"
  shows "vm_error (forth_nrot vm)"
  by (simp add: forth_nrot_def set_error_def assms)

(* ── DROP \<circ> DUP = identity ─────────────────────────────────────────────── *)

lemma dup_then_drop:
  assumes "data_stack vm = x # xs"
  assumes "\<not> ds_full vm"
  assumes "\<not> vm_error vm"
  shows "data_stack (forth_drop (forth_dup vm)) = data_stack vm"
  by (simp add: forth_dup_def forth_drop_def assms)

(* ── DEPTH ( -- n ) ─────────────────────────────────────────────────────── *)
(* Pushes the current stack depth (number of items before DEPTH executes).
   C impl: depth = dsp + 1; data_stack[++dsp] = depth.                     *)

definition forth_depth :: "vm_state \<Rightarrow> vm_state" where
  "forth_depth vm =
    (if ds_full vm
     then set_error vm
     else vm\<lparr>data_stack := int (length (data_stack vm)) # data_stack vm\<rparr>)"

lemma depth_pushes_count:
  assumes "\<not> ds_full vm"
  shows "hd (data_stack (forth_depth vm)) = int (length (data_stack vm))"
  by (simp add: forth_depth_def assms)

lemma depth_rest_preserved:
  assumes "\<not> ds_full vm"
  shows "tl (data_stack (forth_depth vm)) = data_stack vm"
  by (simp add: forth_depth_def assms)

lemma depth_increases_by_one:
  assumes "\<not> ds_full vm"
  shows "length (data_stack (forth_depth vm)) = length (data_stack vm) + 1"
  by (simp add: forth_depth_def assms)

lemma depth_overflow:
  assumes "ds_full vm"
  shows "vm_error (forth_depth vm)"
  by (simp add: forth_depth_def set_error_def assms)

(* ── PICK ( n -- x ) ───────────────────────────────────────────────────── *)
(* Replaces TOS (n) with the element at index n in the current stack.
   Index 0 = TOS itself; index 1 = second item; etc.
   In-place replacement — depth unchanged.

   Verified against C:
     n   = data_stack[dsp]       (TOS, not popped)
     val = data_stack[dsp - n]
     data_stack[dsp] = val       (replace TOS with val)

   Bounds check: n \<ge> 0 and n < length (data_stack vm)                     *)

definition forth_pick :: "vm_state \<Rightarrow> vm_state" where
  "forth_pick vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow>
         if n < 0 \<or> nat n \<ge> length (data_stack vm)
         then set_error vm
         else vm\<lparr>data_stack := data_stack vm ! nat n # xs\<rparr>)"

lemma pick_normal:
  assumes "data_stack vm = n # xs"
  assumes "n \<ge> 0"
  assumes "nat n < length (data_stack vm)"
  shows "data_stack (forth_pick vm) = data_stack vm ! nat n # xs"
  by (simp add: forth_pick_def assms)

lemma pick_depth_unchanged:
  assumes "data_stack vm = n # xs"
  assumes "n \<ge> 0"
  assumes "nat n < length (data_stack vm)"
  shows "length (data_stack (forth_pick vm)) = length (data_stack vm)"
  by (simp add: forth_pick_def assms)

(* 0 PICK: replaces TOS (which is 0) with data_stack[0] = 0 — identity. *)
lemma pick_zero_self:
  assumes "data_stack vm = 0 # xs"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_pick vm) = 0 # xs"
  by (simp add: forth_pick_def assms)

(* 1 PICK: replaces TOS (1) with the element at index 1 = hd xs. *)
lemma pick_one:
  assumes "data_stack vm = 1 # x # xs"
  shows "data_stack (forth_pick vm) = x # x # xs"
  by (simp add: forth_pick_def assms)

lemma pick_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_pick vm)"
  by (simp add: forth_pick_def set_error_def assms)

lemma pick_bounds_neg:
  assumes "data_stack vm = n # xs"
  assumes "n < 0"
  shows "vm_error (forth_pick vm)"
  by (simp add: forth_pick_def set_error_def assms)

lemma pick_bounds_high:
  assumes "data_stack vm = n # xs"
  assumes "nat n \<ge> length (data_stack vm)"
  shows "vm_error (forth_pick vm)"
  by (simp add: forth_pick_def set_error_def assms)

(* ── ROLL ( +n -- ) ─────────────────────────────────────────────────────── *)
(* Pops n, then rotates items.
   Special cases in C implementation:
     n = 0: pop n, return (stack depth decreases by 1, TOS unchanged)
     n = 1: pop n, return ("top item already at top")
     n \<ge> 2: save item at index (depth - n) from bottom (= dsp+1-n after pop),
             shift items down to fill gap, place saved item on top.

   Net effect on depth: decreases by 1 (n is consumed, one item moved).

   Index convention after popping n (dsp' = dsp - 1):
     value = data_stack[dsp'+1-n] = data_stack[dsp-n]
   Items at indices (dsp-n)..(dsp-1) shift down by 1.
   Saved value placed at data_stack[dsp].
   Final dsp unchanged (= dsp-1 after pop, but top slot reused).           *)

definition forth_roll :: "vm_state \<Rightarrow> vm_state" where
  "forth_roll vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | n # xs \<Rightarrow>
         if n < 0 \<or> nat n > length xs
         then set_error vm
         else if n = 0 \<or> n = 1
              then vm\<lparr>data_stack := xs\<rparr>
              else let i    = nat n
                       item = xs ! (i - 1)
                       rest = take (i - 1) xs @ drop i xs
                   in vm\<lparr>data_stack := item # rest\<rparr>)"

lemma roll_zero_nop:
  assumes "data_stack vm = 0 # xs"
  shows "data_stack (forth_roll vm) = xs"
  by (simp add: forth_roll_def assms)

lemma roll_one_nop:
  assumes "data_stack vm = 1 # xs"
  shows "data_stack (forth_roll vm) = xs"
  by (simp add: forth_roll_def assms)

(* 2 ROLL is equivalent to ROT (bring third item to top). *)
lemma roll_two_is_rot:
  assumes "data_stack vm = 2 # n3 # n2 # n1 # rest"
  shows "data_stack (forth_roll vm) = n1 # n3 # n2 # rest"
  by (simp add: forth_roll_def assms)

lemma roll_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_roll vm)"
  by (simp add: forth_roll_def set_error_def assms)

lemma roll_bounds_neg:
  assumes "data_stack vm = n # xs"
  assumes "n < 0"
  shows "vm_error (forth_roll vm)"
  by (simp add: forth_roll_def set_error_def assms)

lemma roll_bounds_high:
  assumes "data_stack vm = n # xs"
  assumes "nat n > length xs"
  shows "vm_error (forth_roll vm)"
  by (simp add: forth_roll_def set_error_def assms)

end
