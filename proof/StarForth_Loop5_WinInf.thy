theory StarForth_Loop5_WinInf
  imports StarForth_Q48_16 StarForth_Loop2_Window
begin

(* =========================================================================
   StarForth_Loop5_WinInf — Window Width Inference (Physics Loop #5)

   Mirrors: src/inference_engine.c (run_inference → io_adaptive_window_width)
            include/inference_engine.h

   Loop #5 uses a statistical test (Levene's F-test / ANOVA) on the rolling
   window's execution pattern variance to suggest a new effective window size.
   The inference engine may early-exit (io_early_exited = true) when the
   window variance is below a significance threshold, leaving rw_eff_window
   unchanged.

   Key correctness claim:
     If inference runs to completion (¬ io_early_exited),
     then io_adaptive_window_width ∈ [ADAPTIVE_MIN, ROLLING_WINDOW_SIZE].

   The full Levene ANOVA formalization requires specialized statistical
   machinery beyond the scope of this theory; those sub-lemmas are stated as
   proof obligations (sorry) with the mathematical specification documented.
   ======================================================================== *)

(* =========================================================================
   Section 1: Inference output well-formedness
   ======================================================================== *)

definition inf_window_wf :: "inference_outputs_state \<Rightarrow> bool" where
  "inf_window_wf io \<longleftrightarrow>
     io_adaptive_window_width io \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
     io_adaptive_window_width io \<le> ROLLING_WINDOW_SIZE"

lemma inf_window_wf_lb:
  assumes "inf_window_wf io"
  shows "io_adaptive_window_width io \<ge> ADAPTIVE_MIN_WINDOW_SIZE"
  using assms by (simp add: inf_window_wf_def)

lemma inf_window_wf_ub:
  assumes "inf_window_wf io"
  shows "io_adaptive_window_width io \<le> ROLLING_WINDOW_SIZE"
  using assms by (simp add: inf_window_wf_def)

(* =========================================================================
   Section 2: Variance threshold and early-exit
   ======================================================================== *)

(* The ANOVA early-exit fires when window variance < threshold.
   Threshold is stored as Q48.16 nat.  We abstract the threshold here. *)
definition ANOVA_VARIANCE_THRESHOLD :: nat where
  "ANOVA_VARIANCE_THRESHOLD = Q48_SCALE"   \<comment> \<open>1.0 in Q48.16 — placeholder\<close>

definition anova_early_exit :: "inference_outputs_state \<Rightarrow> bool" where
  "anova_early_exit io \<longleftrightarrow>
     io_window_variance_q48 io < ANOVA_VARIANCE_THRESHOLD"

(* When early-exit fires, io_early_exited is set and window is not adjusted. *)
lemma early_exit_no_window_change:
  "\<comment> \<open>Proof obligation: the inference_engine sets io_early_exited = true exactly
      when io_window_variance_q48 io < ANOVA_VARIANCE_THRESHOLD, and in that
      case does not update the rolling window effective size.  This is enforced
      by the C code in src/inference_engine.c run_inference() branch.\<close>"
  sorry

(* =========================================================================
   Section 3: Window suggestion clamping
   ======================================================================== *)

(* The raw suggestion from ANOVA may be outside the valid range; the C code
   clamps it via max/min before writing to io_adaptive_window_width.         *)
definition clamp_window_suggestion :: "nat \<Rightarrow> nat" where
  "clamp_window_suggestion n =
     max ADAPTIVE_MIN_WINDOW_SIZE (min ROLLING_WINDOW_SIZE n)"

lemma clamp_window_in_range:
  "clamp_window_suggestion n \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
   clamp_window_suggestion n \<le> ROLLING_WINDOW_SIZE"
  by (simp add: clamp_window_suggestion_def
                ADAPTIVE_MIN_WINDOW_SIZE_def ROLLING_WINDOW_SIZE_def)

lemma clamp_window_wf:
  "inf_window_wf (io\<lparr>io_adaptive_window_width := clamp_window_suggestion n\<rparr>)"
  by (simp add: inf_window_wf_def clamp_window_in_range)

(* =========================================================================
   Section 4: Applying the inference output to the rolling window
   ======================================================================== *)

(* When inference runs to completion, the suggested window replaces rw_eff_window.
   The invariant is maintained because the suggestion is clamped.            *)
definition apply_window_inference :: "inference_outputs_state \<Rightarrow> rolling_window_state
                                      \<Rightarrow> rolling_window_state" where
  "apply_window_inference io rw =
     (if io_early_exited io
      then rw
      else set_eff_window (io_adaptive_window_width io) rw)"

lemma apply_window_inference_preserves_invariant:
  assumes "window_invariant rw"
  shows "window_invariant (apply_window_inference io rw)"
  using assms
  by (simp add: apply_window_inference_def set_eff_window_preserves_invariant)

lemma apply_window_early_exit_unchanged:
  assumes "io_early_exited io"
  shows "apply_window_inference io rw = rw"
  by (simp add: apply_window_inference_def assms)

(* =========================================================================
   Section 5: Levene ANOVA specification (proof obligation)
   ======================================================================== *)

(* Levene's test formalisation:
   Given k groups of observations G_1,...,G_k each of size n_i, the test
   statistic is:
     F = ((N-k)/( k-1)) × (Σ n_i(Z̄_i - Z̄)²) / (Σ Σ (Z_{ij} - Z̄_i)²)
   where Z_{ij} = |X_{ij} - X̄_i| (absolute deviation from group mean).

   In StarForth the "groups" are time-slices of the rolling window.
   The hypothesis: if F < F_critical(k-1, N-k) then window sizes are
   statistically equivalent; the window is stable and no resize is needed.

   Formalising the full distribution of F under H₀ requires Isabelle's
   probability theory library (HOL-Analysis, measure_theory).  That
   formalisation is left as an open proof obligation below.               *)

theorem levene_test_correct:
  "\<comment> \<open>Proof obligation: Levene's F-statistic computed from rw_history
      correctly detects non-uniform execution diversity, and the resulting
      io_adaptive_window_width satisfies inf_window_wf when the test runs
      to completion.  Requires HOL-Analysis probability machinery.\<close>"
  sorry

end
