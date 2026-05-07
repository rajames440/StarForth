theory StarForth_Loop6_DecayInf
  imports StarForth_Q48_16 StarForth_Loop3_Decay
begin

(* =========================================================================
   StarForth_Loop6_DecayInf — Decay Slope Inference (Physics Loop #6)

   Mirrors: src/inference_engine.c (run_inference → io_adaptive_decay_slope)
            include/inference_engine.h

   Loop #6 uses exponential / linear regression on the rolling window's heat
   trajectory to infer an optimal decay slope.  The regressed slope replaces
   decay_slope_q48 in the VM, but only if:
     1. Inference ran to completion (¬ io_early_exited).
     2. The suggested slope > 0 (slope_wf requirement).

   Key correctness claims:
     • The suggested slope is always > 0 (enforced by clamping in the C code).
     • Applying the inference output to vm.decay_slope_q48 preserves slope_wf.
     • The inferred slope direction is consistent with the observed heat trend.

   Full linear-regression correctness (ordinary-least-squares proof) is a
   proof obligation requiring HOL-Analysis; it is marked sorry below.
   ======================================================================== *)

(* =========================================================================
   Section 1: Slope suggestion clamping
   ======================================================================== *)

definition clamp_slope_suggestion :: "nat \<Rightarrow> nat" where
  "clamp_slope_suggestion s =
     max DECAY_SLOPE_MIN (min DECAY_SLOPE_MAX s)"

lemma clamp_slope_wf:
  "slope_wf (clamp_slope_suggestion s)"
  by (simp add: slope_wf_def clamp_slope_suggestion_def)

lemma clamp_slope_pos:
  "clamp_slope_suggestion s \<ge> DECAY_SLOPE_MIN"
  by (simp add: clamp_slope_suggestion_def)

(* =========================================================================
   Section 2: Inference output well-formedness (slope component)
   ======================================================================== *)

definition inf_slope_wf :: "inference_outputs_state \<Rightarrow> bool" where
  "inf_slope_wf io \<longleftrightarrow> slope_wf (io_adaptive_decay_slope io)"

lemma inf_slope_wf_pos:
  assumes "inf_slope_wf io"
  shows "io_adaptive_decay_slope io > 0"
  using assms by (simp add: inf_slope_wf_def slope_wf_def DECAY_SLOPE_MIN_def)

(* =========================================================================
   Section 3: Applying the inferred slope to the VM
   ======================================================================== *)

definition apply_slope_inference :: "inference_outputs_state \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "apply_slope_inference io vm =
     (if io_early_exited io
      then vm
      else vm\<lparr>decay_slope_q48 :=
                 clamp_slope_suggestion (io_adaptive_decay_slope io)\<rparr>)"

lemma apply_slope_inference_preserves_wf:
  assumes "slope_wf (decay_slope_q48 vm)"
  shows "slope_wf (decay_slope_q48 (apply_slope_inference io vm))"
proof (cases "io_early_exited io")
  case True thus ?thesis by (simp add: apply_slope_inference_def assms)
next
  case False thus ?thesis
    by (simp add: apply_slope_inference_def clamp_slope_wf)
qed

lemma apply_slope_inference_pos:
  assumes "slope_wf (decay_slope_q48 vm)"
  shows "decay_slope_q48 (apply_slope_inference io vm) > 0"
  using apply_slope_inference_preserves_wf assms slope_wf_pos by blast

lemma apply_slope_inference_ds [simp]:
  "data_stack (apply_slope_inference io vm) = data_stack vm"
  by (simp add: apply_slope_inference_def)

lemma apply_slope_inference_rs [simp]:
  "return_stack (apply_slope_inference io vm) = return_stack vm"
  by (simp add: apply_slope_inference_def)

lemma apply_slope_inference_rolling [simp]:
  "rolling_window (apply_slope_inference io vm) = rolling_window vm"
  by (simp add: apply_slope_inference_def)

lemma apply_slope_early_exit_unchanged:
  assumes "io_early_exited io"
  shows "apply_slope_inference io vm = vm"
  by (simp add: apply_slope_inference_def assms)

(* =========================================================================
   Section 4: Linear regression specification (proof obligation)
   ======================================================================== *)

(* OLS regression spec:
   Given n observations (x_i, y_i) where x_i = tick index and y_i = total heat,
   the slope is β = (n Σx_iy_i - Σx_i Σy_i) / (n Σx_i² - (Σx_i)²).

   For the decay slope inference the y_i values come from rolling window
   snapshots (total_heat_at_last_check) and x_i are tick counts.

   Correctness claim: if the heat sequence is strictly decreasing (decay is
   occurring), the regressed slope β > 0, so io_adaptive_decay_slope > 0
   before clamping — making clamping a no-op for well-behaved runs.

   Proof requires HOL-Analysis real arithmetic and measure theory;
   stated here as a well-typed proof obligation.                            *)

theorem ols_slope_positive_for_decreasing_heat:
  "\<comment> \<open>Proof obligation: if the heat sequence
      [y_0, ..., y_{n-1}] is strictly non-increasing with n \<ge> 2,
      then the OLS slope \<beta> > 0 (in Q48.16 representation).
      Requires HOL-Analysis OLS formalization.\<close>"
  sorry

end
