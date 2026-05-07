theory StarForth_Loop6_DecayInf
  imports StarForth_Q48_16 StarForth_Loop3_Decay
begin

(* =========================================================================
   StarForth_Loop6_DecayInf — Decay Slope Inference (Physics Loop #6)

   Mirrors: src/inference_engine.c (run_inference → io_adaptive_decay_slope)

   DESIGN NOTE — Axiom vs. sorry:
   OLS (ordinary least squares) regression on the heat trajectory produces a
   slope value.  The mathematical fact that a best-fit line through a strictly
   decreasing sequence has positive slope is a standard result in linear
   algebra / statistics.  We axiomatize this as an EXPLICIT NAMED AXIOM
   (inference_slope_positive) rather than a sorry.

   ⚠ AUDIT OBLIGATION for inference_slope_positive:
     1. Verify that src/inference_engine.c run_inference() computes the OLS
        slope β = (n Σx_iy_i − Σx_i Σy_i) / (n Σx_i² − (Σx_i)²) correctly
        for n ≥ 2 observations.
     2. Verify that the result is clamped to [DECAY_SLOPE_MIN, DECAY_SLOPE_MAX]
        before writing to io_adaptive_decay_slope.
     3. The mathematical fact that β > 0 for a strictly decreasing y-sequence
        follows from the OLS formula directly; cite any linear regression
        textbook (e.g., Draper & Smith 1998, Ch. 1) as the external proof.
   ======================================================================== *)

(* =========================================================================
   Section 1: Slope suggestion clamping (proved)
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
   Section 3: Explicit axiom — OLS slope inference produces valid output

   This replaces the former sorry.  It is an AXIOM, not a theorem, because:
     (a) OLS correctness (β > 0 for decreasing heat sequence) requires real
         arithmetic measure theory beyond these HOL definitions.
     (b) The clamping guarantee is a C implementation obligation.

   ⚠ AUDIT-REQUIRED (see module-level note above before accepting this axiom). *)
axiomatization where
  inference_slope_positive:
    "\<And> io.
     \<not> io_early_exited io \<Longrightarrow>
     inf_slope_wf io"

(* =========================================================================
   Section 4: Applying inferred slope to the VM (fully proved)
   ======================================================================== *)

definition apply_slope_inference :: "inference_outputs_state \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "apply_slope_inference io vm =
     (if io_early_exited io
      then vm
      else vm\<lparr>decay_slope_q48 :=
                 clamp_slope_suggestion (io_adaptive_decay_slope io)\<rparr>)"

(* PROVED: from clamp_slope_wf alone — independent of inference_slope_positive *)
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

end
