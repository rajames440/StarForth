(*
  File:     Real_Asymp_Approx.thy
  Author:   Manuel Eberl, TU München
  
  Integrates the approximation method into the real_asymp framework as a sign oracle
  to automatically prove positivity/negativity of real constants
*)
theory Real_Asymp_Approx
  imports Real_Asymp "HOL-Decision_Procs.Approximation"
begin

text \<open>
  For large enough constants (such as \<^term>\<open>exp (exp 10000 :: real)\<close>), the 
  @{method approximation} method can require a huge amount of time and memory, effectively not
  terminating and causing the entire prover environment to crash.

  To avoid causing such situations, we first check the plausibility of the fact to prove using
  floating-point arithmetic and only run the approximation method if the floating point evaluation
  supports the claim. In particular, exorbitantly large numbers like the one above will lead to
  floating point overflow, which makes the check fail.
\<close>

ML \<open>
signature REAL_ASYMP_APPROX = 
sig
  val real_asymp_approx : bool Config.T
  val sign_oracle_tac : Proof.context -> int -> tactic
  val eval : term -> real
end

structure Real_Asymp_Approx : REAL_ASYMP_APPROX =
struct

val real_asymp_approx =
  Attrib.setup_config_bool \<^binding>\<open>real_asymp_approx\<close> (K true)

val nan = Real.fromInt 0 / Real.fromInt 0
fun clamp n = if n < 0 then 0 else n

fun eval_nat (\<^term>\<open>(+) :: nat => _\<close> $ a $ b) = bin (op +) (a, b)
  | eval_nat (\<^term>\<open>(-) :: nat => _\<close> $ a $ b) = bin (clamp o op -) (a, b)
  | eval_nat (\<^term>\<open>(*) :: nat => _\<close> $ a $ b) = bin (op *) (a, b)
  | eval_nat (\<^term>\<open>(div) :: nat => _\<close> $ a $ b) = bin Int.div (a, b)
  | eval_nat (\<^term>\<open>(^) :: nat => _\<close> $ a $ b) = bin (fn (a,b) => Integer.pow a b) (a, b)
  | eval_nat (t as (\<^term>\<open>numeral :: _ => nat\<close> $ _)) = snd (HOLogic.dest_number t)
  | eval_nat (\<^term>\<open>0 :: nat\<close>) = 0
  | eval_nat (\<^term>\<open>1 :: nat\<close>) = 1
  | eval_nat (\<^term>\<open>Nat.Suc\<close> $ a) = un (fn n => n + 1) a
  | eval_nat _ = ~1
and un f a =
      let
        val a = eval_nat a 
      in
        if a >= 0 then f a else ~1
      end
and bin f (a, b) =
      let
        val (a, b) = apply2 eval_nat (a, b) 
      in
        if a >= 0 andalso b >= 0 then f (a, b) else ~1
      end

fun sgn n =
  if n < Real.fromInt 0 then Real.fromInt (~1) else Real.fromInt 1

fun eval (\<^term>\<open>(+) :: real => _\<close> $ a $ b) = eval a + eval b
  | eval (\<^term>\<open>(-) :: real => _\<close> $ a $ b) = eval a - eval b
  | eval (\<^term>\<open>(*) :: real => _\<close> $ a $ b) = eval a * eval b
  | eval (\<^term>\<open>(/) :: real => _\<close> $ a $ b) =
      let val a = eval a; val b = eval b in
        if Real.==(b, Real.fromInt 0) then nan else a / b
      end
  | eval (\<^term>\<open>inverse :: real => _\<close> $ a) = Real.fromInt 1 / eval a
  | eval (\<^term>\<open>uminus :: real => _\<close> $ a) = Real.~ (eval a)
  | eval (\<^term>\<open>exp :: real => _\<close> $ a) = Math.exp (eval a)
  | eval (\<^term>\<open>ln :: real => _\<close> $ a) =
      let val a = eval a in if a > Real.fromInt 0 then Math.ln a else nan end
  | eval (\<^term>\<open>(powr) :: real => _\<close> $ a $ b) =
      let
        val a = eval a; val b = eval b
      in
        if a < Real.fromInt 0 orelse not (Real.isFinite a) orelse not (Real.isFinite b) then
          nan
        else if Real.==(a, Real.fromInt 0) then
          Real.fromInt 0
        else 
          Math.pow (a, b)
      end
  | eval (\<^term>\<open>(^) :: real => _\<close> $ a $ b) =
      let
        fun powr x y = 
          if not (Real.isFinite x) orelse y < 0 then
            nan
          else if x < Real.fromInt 0 andalso y mod 2 = 1 then 
            Real.~ (Math.pow (Real.~ x, Real.fromInt y))
          else
            Math.pow (Real.abs x, Real.fromInt y)
      in
        powr (eval a) (eval_nat b)
      end
  | eval (\<^term>\<open>root :: nat => real => _\<close> $ n $ a) =
      let val a = eval a; val n = eval_nat n in
        if n = 0 then Real.fromInt 0
          else sgn a * Math.pow (Real.abs a, Real.fromInt 1 / Real.fromInt n) end
  | eval (\<^term>\<open>sqrt :: real => _\<close> $ a) =
      let val a = eval a in sgn a * Math.sqrt (abs a) end
  | eval (\<^term>\<open>log :: real => _\<close> $ a $ b) =
      let
        val (a, b) = apply2 eval (a, b)
      in
        if b > Real.fromInt 0 andalso a > Real.fromInt 0 andalso Real.!= (a, Real.fromInt 1) then
          Math.ln b / Math.ln a
        else
          nan
      end
  | eval (t as (\<^term>\<open>numeral :: _ => real\<close> $ _)) =
      Real.fromInt (snd (HOLogic.dest_number t))
  | eval (\<^term>\<open>0 :: real\<close>) = Real.fromInt 0
  | eval (\<^term>\<open>1 :: real\<close>) = Real.fromInt 1
  | eval _ = nan

fun sign_oracle_tac ctxt i =
  let
    fun tac {context = ctxt, concl, ...} =
      let
        val b =
          case HOLogic.dest_Trueprop (Thm.term_of concl) of
            \<^term>\<open>(<) :: real \<Rightarrow> _\<close> $ lhs $ rhs =>
              let
                val (x, y) = apply2 eval (lhs, rhs)
              in
                Real.isFinite x andalso Real.isFinite y andalso x < y
              end
          | \<^term>\<open>HOL.Not\<close> $ (\<^term>\<open>(=) :: real \<Rightarrow> _\<close> $ lhs $ rhs) =>
              let
                val (x, y) = apply2 eval (lhs, rhs)
              in
                Real.isFinite x andalso Real.isFinite y andalso Real.!= (x, y)
              end
          | _ => false
     in
       if b then HEADGOAL (Approximation.approximation_tac 10 [] NONE ctxt) else no_tac
     end
  in
    if Config.get ctxt real_asymp_approx then
      Subgoal.FOCUS tac ctxt i
    else
      no_tac
  end

end
\<close>

setup \<open>
  Context.theory_map (
    Multiseries_Expansion.register_sign_oracle
      (\<^binding>\<open>approximation_tac\<close>, Real_Asymp_Approx.sign_oracle_tac))
\<close>

lemma "filterlim (\<lambda>n. (1 + (2 / 3 :: real) ^ (n + 1)) ^ 2 ^ n / 2 powr (4 / 3) ^ (n - 1))
         at_top at_top"
proof -
  have [simp]: "ln 4 = 2 * ln (2 :: real)"
    using ln_realpow[of 2 2] by simp
  show ?thesis by (real_asymp simp: field_simps ln_div)
qed

end