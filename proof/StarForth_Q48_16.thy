theory StarForth_Q48_16
  imports Main
begin

(* =========================================================================
   StarForth_Q48_16 — Q48.16 Fixed-Point Arithmetic Model

   Mirrors: include/q48_16.h

   Format: nat (modelling uint64_t) with implicit decimal point after bit 15.
     Bits  0-15 : fractional part (resolution 1/65536)
     Bits 16-63 : integer part (up to 2^48 - 1)

   This theory defines the HOL model of every q48_* operation used by the
   inference engine and physics loops, then proves the algebraic properties
   that the physics-loop invariant theories depend on.
   ======================================================================== *)

(* =========================================================================
   Section 1: Scale factor and fundamental constants
   ======================================================================== *)

definition Q48_SCALE :: nat where "Q48_SCALE = 65536"   \<comment> \<open>2^16\<close>
definition Q48_ONE   :: nat where "Q48_ONE   = 65536"   \<comment> \<open>1.0 in Q48.16\<close>
definition Q48_HALF  :: nat where "Q48_HALF  = 32768"   \<comment> \<open>0.5 in Q48.16\<close>

lemma Q48_SCALE_pos  [simp]: "Q48_SCALE > 0"
  by (simp add: Q48_SCALE_def)

lemma Q48_ONE_eq_SCALE [simp]: "Q48_ONE = Q48_SCALE"
  by (simp add: Q48_ONE_def Q48_SCALE_def)

(* =========================================================================
   Section 2: Conversion operations
   ======================================================================== *)

(* q48_from_u64: integer → Q48.16 (multiply by 2^16)
   C: static inline q48_16_t q48_from_u64(uint64_t u) { return u << 16; }   *)
definition q48_from_u64 :: "nat \<Rightarrow> nat" where
  "q48_from_u64 n = n * Q48_SCALE"

(* q48_to_u64: Q48.16 → integer (divide by 2^16, truncate)
   C: static inline uint64_t q48_to_u64(q48_16_t q) { return q >> 16; }     *)
definition q48_to_u64 :: "nat \<Rightarrow> nat" where
  "q48_to_u64 q = q div Q48_SCALE"

lemma q48_round_trip:
  "q48_to_u64 (q48_from_u64 n) = n"
  by (simp add: q48_to_u64_def q48_from_u64_def Q48_SCALE_def)

lemma q48_from_u64_mono:
  assumes "a \<le> b"
  shows "q48_from_u64 a \<le> q48_from_u64 b"
  by (simp add: q48_from_u64_def assms)

lemma q48_from_u64_zero [simp]:
  "q48_from_u64 0 = 0"
  by (simp add: q48_from_u64_def)

lemma q48_to_u64_zero [simp]:
  "q48_to_u64 0 = 0"
  by (simp add: q48_to_u64_def)

(* =========================================================================
   Section 3: Arithmetic operations
   ======================================================================== *)

(* q48_add: pointwise addition — Q48.16 + Q48.16 → Q48.16
   C: static inline q48_16_t q48_add(q48_16_t a, q48_16_t b) { return a + b; } *)
definition q48_add :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "q48_add a b = a + b"

lemma q48_add_comm: "q48_add a b = q48_add b a"
  by (simp add: q48_add_def)

lemma q48_add_assoc: "q48_add (q48_add a b) c = q48_add a (q48_add b c)"
  by (simp add: q48_add_def)

lemma q48_add_zero_right [simp]: "q48_add a 0 = a"
  by (simp add: q48_add_def)

lemma q48_add_zero_left  [simp]: "q48_add 0 a = a"
  by (simp add: q48_add_def)

lemma q48_add_mono:
  assumes "a \<le> a'" "b \<le> b'"
  shows "q48_add a b \<le> q48_add a' b'"
  by (simp add: q48_add_def add_le_mono assms)

(* q48_mul: Q48.16 × Q48.16 → Q48.16
   C: q48_16_t q48_mul(q48_16_t a, q48_16_t b) { return (a * b) >> 16; }
   Math: (a/2^16) * (b/2^16) = (a*b)/2^32, then re-scale to Q48.16: ×2^16  *)
definition q48_mul :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "q48_mul a b = (a * b) div Q48_SCALE"

lemma q48_mul_comm: "q48_mul a b = q48_mul b a"
  by (simp add: q48_mul_def mult.commute)

lemma q48_mul_zero_right [simp]: "q48_mul a 0 = 0"
  by (simp add: q48_mul_def)

lemma q48_mul_zero_left  [simp]: "q48_mul 0 a = 0"
  by (simp add: q48_mul_def)

lemma q48_mul_one_right [simp]: "q48_mul a Q48_ONE = a"
  by (simp add: q48_mul_def Q48_ONE_def Q48_SCALE_def)

lemma q48_mul_one_left  [simp]: "q48_mul Q48_ONE a = a"
  by (simp add: q48_mul_def Q48_ONE_def Q48_SCALE_def)

lemma q48_mul_mono:
  assumes "a \<le> a'" "b \<le> b'"
  shows "q48_mul a b \<le> q48_mul a' b'"
  by (simp add: q48_mul_def div_le_mono mult_le_mono assms)

(* q48_div: Q48.16 ÷ Q48.16 → Q48.16 (undefined when b = 0)
   C: return (a << 16) / b;  (with saturation guard)
   Math: (a/2^16) / (b/2^16) = a/b, re-scaled: × 2^16 → (a×2^16)/b          *)
definition q48_div :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "q48_div a b = (if b = 0 then 0 else (a * Q48_SCALE) div b)"

lemma q48_div_zero_denom [simp]: "q48_div a 0 = 0"
  by (simp add: q48_div_def)

lemma q48_div_one_denom [simp]: "q48_div a Q48_ONE = a"
  by (simp add: q48_div_def Q48_ONE_def Q48_SCALE_def)

lemma q48_div_self [simp]:
  assumes "a > 0"
  shows "q48_div a a = Q48_ONE"
  by (simp add: q48_div_def Q48_ONE_def assms)

(* =========================================================================
   Section 4: Integer-to-Q48.16 arithmetic lifting
   ======================================================================== *)

lemma q48_from_add:
  "q48_from_u64 (a + b) = q48_add (q48_from_u64 a) (q48_from_u64 b)"
  by (simp add: q48_from_u64_def q48_add_def distrib_right)

lemma q48_from_mul:
  "q48_from_u64 (a * b) = q48_mul (q48_from_u64 a) (q48_from_u64 b)"
  by (simp add: q48_from_u64_def q48_mul_def Q48_SCALE_def)

(* =========================================================================
   Section 5: Accuracy ratio in Q48.16 (used by Loop #4 / Loop #5)

   The prefetch accuracy is hits / (hits + misses), represented in Q48.16.
   accuracy_q48 = q48_div (hits * Q48_SCALE) (hits + misses)                 *)

definition q48_accuracy :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "q48_accuracy hits total =
     (if total = 0 then 0 else (hits * Q48_SCALE) div total)"

lemma q48_accuracy_zero_total [simp]: "q48_accuracy hits 0 = 0"
  by (simp add: q48_accuracy_def)

lemma q48_accuracy_upper_bound:
  assumes "hits \<le> total"
  shows "q48_accuracy hits total \<le> Q48_ONE"
proof (cases "total = 0")
  case True thus ?thesis by (simp add: q48_accuracy_def Q48_ONE_def)
next
  case False
  have "hits * Q48_SCALE \<le> total * Q48_SCALE"
    using assms by (simp add: mult_le_mono1)
  hence "(hits * Q48_SCALE) div total \<le> Q48_SCALE"
    using False by (simp add: div_le_iff_le_mult)
  thus ?thesis by (simp add: q48_accuracy_def Q48_ONE_def False)
qed

lemma q48_accuracy_non_negative: "q48_accuracy hits total \<ge> 0"
  by simp

(* =========================================================================
   Section 6: Variance proxy (used by Loop #5 Levene / Loop #6 regression)

   The inference engine computes window variance as a Q48.16 quantity.
   We abstract it here as an opaque function with range constraints.          *)

axiomatization q48_variance :: "nat \<Rightarrow> nat \<Rightarrow> nat"
  where q48_variance_non_negative: "q48_variance n s \<ge> 0"

end
