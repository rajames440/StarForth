theory StarForth_Arithmetic_Words
  imports StarForth_Base
begin

(* =========================================================================
   POST-02: Arithmetic Words
   Mirrors: src/word_source/arithmetic_words.c
            src/test_runner/modules/arithmetic_words_test.c

   All operations use HOL int (arbitrary precision).  Proofs hold in C
   provided no intermediate value overflows 64-bit signed range.
   ======================================================================== *)

(* ── Helper: binary op lifting ─────────────────────────────────────────── *)
(* Most binary arithmetic words: pop two, push result.                      *)

definition binop :: "(cell \<Rightarrow> cell \<Rightarrow> cell) \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "binop f vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow> vm\<lparr>data_stack := f n1 n2 # rest\<rparr>
     | _              \<Rightarrow> set_error vm)"

lemma binop_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (binop f vm) = f n1 n2 # rest"
  by (simp add: binop_def assms)

lemma binop_depth:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "length (data_stack (binop f vm)) = length (data_stack vm) - 1"
  by (simp add: binop_def assms)

lemma binop_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (binop f vm)"
  by (simp add: binop_def set_error_def assms)

lemma binop_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (binop f vm)"
  by (simp add: binop_def set_error_def assms)

(* ── Helper: unary op lifting ───────────────────────────────────────────── *)

definition unop :: "(cell \<Rightarrow> cell) \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "unop f vm =
    (case data_stack vm of
       []     \<Rightarrow> set_error vm
     | x # xs \<Rightarrow> vm\<lparr>data_stack := f x # xs\<rparr>)"

lemma unop_normal:
  assumes "data_stack vm = x # xs"
  shows "data_stack (unop f vm) = f x # xs"
  by (simp add: unop_def assms)

lemma unop_depth_preserved:
  assumes "data_stack vm = x # xs"
  shows "length (data_stack (unop f vm)) = length (data_stack vm)"
  by (simp add: unop_def assms)

lemma unop_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (unop f vm)"
  by (simp add: unop_def set_error_def assms)

(* ── + ( n1 n2 -- n3 ) ─────────────────────────────────────────────────── *)

definition forth_add :: "vm_state \<Rightarrow> vm_state" where
  "forth_add = binop (+)"

lemma add_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_add vm) = (n1 + n2) # rest"
  by (simp add: forth_add_def binop_def assms)

lemma add_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_add vm) = data_stack (forth_add vm')"
  by (simp add: forth_add_def binop_def assms add.commute)

lemma add_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_add vm)"
  by (simp add: forth_add_def binop_def set_error_def assms)

lemma add_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_add vm)"
  by (simp add: forth_add_def binop_def set_error_def assms)

(* ── - ( n1 n2 -- n3 ) ─────────────────────────────────────────────────── *)
(* Stack effect: n3 = n1 - n2  (n2 = TOS subtracted from n1 = second)      *)

definition forth_sub :: "vm_state \<Rightarrow> vm_state" where
  "forth_sub = binop (-)"

lemma sub_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_sub vm) = (n1 - n2) # rest"
  by (simp add: forth_sub_def binop_def assms)

lemma sub_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_sub vm)"
  by (simp add: forth_sub_def binop_def set_error_def assms)

lemma sub_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_sub vm)"
  by (simp add: forth_sub_def binop_def set_error_def assms)

(* ── * ( n1 n2 -- n3 ) ─────────────────────────────────────────────────── *)

definition forth_mul :: "vm_state \<Rightarrow> vm_state" where
  "forth_mul = binop (*)"

lemma mul_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_mul vm) = (n1 * n2) # rest"
  by (simp add: forth_mul_def binop_def assms)

lemma mul_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_mul vm) = data_stack (forth_mul vm')"
  by (simp add: forth_mul_def binop_def assms mult.commute)

lemma mul_by_zero_tos:
  assumes "data_stack vm = 0 # n1 # rest"
  shows "data_stack (forth_mul vm) = 0 # rest"
  by (simp add: forth_mul_def binop_def assms)

lemma mul_by_zero_second:
  assumes "data_stack vm = n2 # 0 # rest"
  shows "data_stack (forth_mul vm) = 0 # rest"
  by (simp add: forth_mul_def binop_def assms)

lemma mul_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_mul vm)"
  by (simp add: forth_mul_def binop_def set_error_def assms)

(* ── / ( n1 n2 -- n3 ) ─────────────────────────────────────────────────── *)
(* Division by zero sets vm_error (matching C behaviour).                   *)

definition forth_div :: "vm_state \<Rightarrow> vm_state" where
  "forth_div vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow>
         if n2 = 0
         then set_error vm
         else vm\<lparr>data_stack := (n1 div n2) # rest\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma div_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n2 \<noteq> 0"
  shows "data_stack (forth_div vm) = (n1 div n2) # rest"
  by (simp add: forth_div_def assms)

lemma div_by_zero:
  assumes "data_stack vm = 0 # n1 # rest"
  shows "vm_error (forth_div vm)"
  by (simp add: forth_div_def set_error_def assms)

lemma div_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_div vm)"
  by (simp add: forth_div_def set_error_def assms)

lemma div_underflow_one:
  assumes "data_stack vm = [x]"
  shows "vm_error (forth_div vm)"
  by (simp add: forth_div_def set_error_def assms)

(* ── MOD ( n1 n2 -- n3 ) ───────────────────────────────────────────────── *)

definition forth_mod :: "vm_state \<Rightarrow> vm_state" where
  "forth_mod vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow>
         if n2 = 0
         then set_error vm
         else vm\<lparr>data_stack := (n1 mod n2) # rest\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma mod_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n2 \<noteq> 0"
  shows "data_stack (forth_mod vm) = (n1 mod n2) # rest"
  by (simp add: forth_mod_def assms)

lemma mod_by_zero:
  assumes "data_stack vm = 0 # n1 # rest"
  shows "vm_error (forth_mod vm)"
  by (simp add: forth_mod_def set_error_def assms)

lemma mod_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_mod vm)"
  by (simp add: forth_mod_def set_error_def assms)

(* ── /MOD ( n1 n2 -- n3 n4 ) ───────────────────────────────────────────── *)
(* Pushes remainder (n3) then quotient (n4); quotient is TOS.               *)

definition forth_divmod :: "vm_state \<Rightarrow> vm_state" where
  "forth_divmod vm =
    (case data_stack vm of
       n2 # n1 # rest \<Rightarrow>
         if n2 = 0
         then set_error vm
         else if ds_full vm
              then set_error vm
              else vm\<lparr>data_stack := (n1 div n2) # (n1 mod n2) # rest\<rparr>
     | _ \<Rightarrow> set_error vm)"

lemma divmod_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n2 \<noteq> 0"
  assumes "\<not> ds_full vm"
  shows "data_stack (forth_divmod vm) = (n1 div n2) # (n1 mod n2) # rest"
  by (simp add: forth_divmod_def assms)

lemma divmod_quotient:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n2 \<noteq> 0"
  assumes "\<not> ds_full vm"
  shows "hd (data_stack (forth_divmod vm)) = n1 div n2"
  by (simp add: forth_divmod_def assms)

lemma divmod_remainder:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "n2 \<noteq> 0"
  assumes "\<not> ds_full vm"
  shows "hd (tl (data_stack (forth_divmod vm))) = n1 mod n2"
  by (simp add: forth_divmod_def assms)

(* ── 1+ ( n -- n+1 ) ───────────────────────────────────────────────────── *)

definition forth_one_plus :: "vm_state \<Rightarrow> vm_state" where
  "forth_one_plus = unop (\<lambda>n. n + 1)"

lemma one_plus_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_one_plus vm) = (n + 1) # xs"
  by (simp add: forth_one_plus_def unop_def assms)

lemma one_plus_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_one_plus vm)"
  by (simp add: forth_one_plus_def unop_def set_error_def assms)

(* ── 1- ( n -- n-1 ) ───────────────────────────────────────────────────── *)

definition forth_one_minus :: "vm_state \<Rightarrow> vm_state" where
  "forth_one_minus = unop (\<lambda>n. n - 1)"

lemma one_minus_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_one_minus vm) = (n - 1) # xs"
  by (simp add: forth_one_minus_def unop_def assms)

lemma one_minus_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_one_minus vm)"
  by (simp add: forth_one_minus_def unop_def set_error_def assms)

(* ── 2+ ( n -- n+2 ) ───────────────────────────────────────────────────── *)

definition forth_two_plus :: "vm_state \<Rightarrow> vm_state" where
  "forth_two_plus = unop (\<lambda>n. n + 2)"

lemma two_plus_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_two_plus vm) = (n + 2) # xs"
  by (simp add: forth_two_plus_def unop_def assms)

(* ── 2- ( n -- n-2 ) ───────────────────────────────────────────────────── *)

definition forth_two_minus :: "vm_state \<Rightarrow> vm_state" where
  "forth_two_minus = unop (\<lambda>n. n - 2)"

lemma two_minus_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_two_minus vm) = (n - 2) # xs"
  by (simp add: forth_two_minus_def unop_def assms)

(* ── 2* ( n -- n*2 ) ───────────────────────────────────────────────────── *)
(* Left shift by 1 in C; multiplication by 2 in HOL.                        *)

definition forth_two_mul :: "vm_state \<Rightarrow> vm_state" where
  "forth_two_mul = unop (\<lambda>n. n * 2)"

lemma two_mul_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_two_mul vm) = (n * 2) # xs"
  by (simp add: forth_two_mul_def unop_def assms)

(* ── 2/ ( n -- n/2 ) ───────────────────────────────────────────────────── *)
(* Arithmetic right shift by 1 in C.  In HOL, div 2 on int is floor div.   *)

definition forth_two_div :: "vm_state \<Rightarrow> vm_state" where
  "forth_two_div = unop (\<lambda>n. n div 2)"

lemma two_div_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_two_div vm) = (n div 2) # xs"
  by (simp add: forth_two_div_def unop_def assms)

(* ── ABS ( n -- |n| ) ──────────────────────────────────────────────────── *)

definition forth_abs :: "vm_state \<Rightarrow> vm_state" where
  "forth_abs = unop abs"

lemma abs_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_abs vm) = \<bar>n\<bar> # xs"
  by (simp add: forth_abs_def unop_def assms)

lemma abs_nonneg:
  assumes "data_stack vm = n # xs"
  assumes "\<not> vm_error vm"
  shows "hd (data_stack (forth_abs vm)) \<ge> 0"
  by (simp add: forth_abs_def unop_def assms)

lemma abs_idempotent:
  assumes "data_stack vm = n # xs"
  assumes "\<not> vm_error vm"
  shows "hd (data_stack (forth_abs (forth_abs vm))) = hd (data_stack (forth_abs vm))"
  by (simp add: forth_abs_def unop_def assms)

lemma abs_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_abs vm)"
  by (simp add: forth_abs_def unop_def set_error_def assms)

(* ── NEGATE ( n -- -n ) ─────────────────────────────────────────────────── *)

definition forth_negate :: "vm_state \<Rightarrow> vm_state" where
  "forth_negate = unop uminus"

lemma negate_normal:
  assumes "data_stack vm = n # xs"
  shows "data_stack (forth_negate vm) = (-n) # xs"
  by (simp add: forth_negate_def unop_def assms)

lemma negate_involutive:
  assumes "data_stack vm = n # xs"
  shows "hd (data_stack (forth_negate (forth_negate vm))) = n"
  by (simp add: forth_negate_def unop_def assms)

lemma negate_underflow:
  assumes "data_stack vm = []"
  shows "vm_error (forth_negate vm)"
  by (simp add: forth_negate_def unop_def set_error_def assms)

(* ── MIN ( n1 n2 -- n3 ) ───────────────────────────────────────────────── *)

definition forth_min :: "vm_state \<Rightarrow> vm_state" where
  "forth_min = binop min"

lemma min_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_min vm) = min n1 n2 # rest"
  by (simp add: forth_min_def binop_def assms)

lemma min_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_min vm) = data_stack (forth_min vm')"
  by (simp add: forth_min_def binop_def assms min.commute)

lemma min_idempotent:
  assumes "data_stack vm = n # n # rest"
  shows "data_stack (forth_min vm) = n # rest"
  by (simp add: forth_min_def binop_def assms)

lemma min_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_min vm)"
  by (simp add: forth_min_def binop_def set_error_def assms)

(* ── MAX ( n1 n2 -- n3 ) ───────────────────────────────────────────────── *)

definition forth_max :: "vm_state \<Rightarrow> vm_state" where
  "forth_max = binop max"

lemma max_normal:
  assumes "data_stack vm = n2 # n1 # rest"
  shows "data_stack (forth_max vm) = max n1 n2 # rest"
  by (simp add: forth_max_def binop_def assms)

lemma max_commutative:
  assumes "data_stack vm = n2 # n1 # rest"
  assumes "data_stack vm' = n1 # n2 # rest"
  shows "data_stack (forth_max vm) = data_stack (forth_max vm')"
  by (simp add: forth_max_def binop_def assms max.commute)

lemma max_idempotent:
  assumes "data_stack vm = n # n # rest"
  shows "data_stack (forth_max vm) = n # rest"
  by (simp add: forth_max_def binop_def assms)

lemma max_underflow_nil:
  assumes "data_stack vm = []"
  shows "vm_error (forth_max vm)"
  by (simp add: forth_max_def binop_def set_error_def assms)

(* ── min/max algebraic relationship ────────────────────────────────────── *)

lemma min_le_max:
  "min (a::int) b \<le> max a b"
  by (simp add: min_def max_def)

end
