theory StarForth_Loop2_Window
  imports StarForth_Base
begin

(* =========================================================================
   StarForth_Loop2_Window — Rolling Window of Truth (Physics Loop #2)

   Mirrors: src/rolling_window_of_truth.c
            include/rolling_window_of_truth.h

   The Rolling Window of Truth is a circular buffer of the last N word IDs
   executed, used for entropy / diversity analysis and for seeding the
   inference engine.

   Two window sizes co-exist in the model:
     rw_eff_window  — effective_window_size (adaptive target, mutated by
                      Loops #2, #5, #6; ∈ [ADAPTIVE_MIN, ROLLING_WINDOW_SIZE])
     rw_act_window  — actual_window_size = min(total_executions, ROLLING_WINDOW_SIZE)
                      (true fill level of the ring buffer, monotonically grows
                      until it saturates at ROLLING_WINDOW_SIZE)

   This theory proves:
     • window_invariant holds after every advance step.
     • rw_act_window is monotone and bounded.
     • rw_eff_window stays in [ADAPTIVE_MIN, ROLLING_WINDOW_SIZE].
     • Shrink / grow transitions preserve the invariant.
   ======================================================================== *)

(* =========================================================================
   Section 1: Window invariant
   ======================================================================== *)

definition window_invariant :: "rolling_window_state \<Rightarrow> bool" where
  "window_invariant rw \<longleftrightarrow>
     rw_eff_window rw \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
     rw_eff_window rw \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window rw \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window rw = min (rw_total_exec rw) ROLLING_WINDOW_SIZE"

lemma window_invariant_eff_lb:
  assumes "window_invariant rw"
  shows "rw_eff_window rw \<ge> ADAPTIVE_MIN_WINDOW_SIZE"
  using assms by (simp add: window_invariant_def)

lemma window_invariant_eff_ub:
  assumes "window_invariant rw"
  shows "rw_eff_window rw \<le> ROLLING_WINDOW_SIZE"
  using assms by (simp add: window_invariant_def)

lemma window_invariant_act_bound:
  assumes "window_invariant rw"
  shows "rw_act_window rw \<le> ROLLING_WINDOW_SIZE"
  using assms by (simp add: window_invariant_def)

lemma window_invariant_act_formula:
  assumes "window_invariant rw"
  shows "rw_act_window rw = min (rw_total_exec rw) ROLLING_WINDOW_SIZE"
  using assms by (simp add: window_invariant_def)

(* =========================================================================
   Section 2: Advance step — recording one word execution in the window
   ======================================================================== *)

(* Record word_id w in the ring buffer, advance position, increment counters. *)
definition window_advance :: "nat \<Rightarrow> rolling_window_state \<Rightarrow> rolling_window_state" where
  "window_advance w rw =
     rw\<lparr>rw_history    := (rw_history rw)(rw_window_pos rw := w),
        rw_window_pos := (rw_window_pos rw + 1) mod ROLLING_WINDOW_SIZE,
        rw_total_exec := rw_total_exec rw + 1,
        rw_act_window := min (rw_total_exec rw + 1) ROLLING_WINDOW_SIZE,
        rw_is_warm    := rw_total_exec rw + 1 \<ge> ROLLING_WINDOW_SIZE\<rparr>"

lemma window_advance_total_increases:
  "rw_total_exec (window_advance w rw) = rw_total_exec rw + 1"
  by (simp add: window_advance_def)

lemma window_advance_act_window:
  "rw_act_window (window_advance w rw) = min (rw_total_exec rw + 1) ROLLING_WINDOW_SIZE"
  by (simp add: window_advance_def)

lemma window_advance_act_monotone:
  "rw_act_window (window_advance w rw) \<ge> rw_act_window rw"
  by (simp add: window_advance_def min_def)

lemma window_advance_act_bounded:
  "rw_act_window (window_advance w rw) \<le> ROLLING_WINDOW_SIZE"
  by (simp add: window_advance_def)

lemma window_advance_eff_preserved:
  "rw_eff_window (window_advance w rw) = rw_eff_window rw"
  by (simp add: window_advance_def)

lemma window_advance_preserves_invariant:
  assumes "window_invariant rw"
  shows "window_invariant (window_advance w rw)"
  using assms
  by (simp add: window_invariant_def window_advance_def min_def ROLLING_WINDOW_SIZE_def
                ADAPTIVE_MIN_WINDOW_SIZE_def)

(* =========================================================================
   Section 3: Adaptive window resizing (triggered by diversity checks)
   ======================================================================== *)

(* Shrink: reduce rw_eff_window by ADAPTIVE_SHRINK_RATE, floor at ADAPTIVE_MIN. *)
definition window_shrink :: "rolling_window_state \<Rightarrow> rolling_window_state" where
  "window_shrink rw =
     rw\<lparr>rw_eff_window :=
           max ADAPTIVE_MIN_WINDOW_SIZE (rw_eff_window rw - ADAPTIVE_SHRINK_RATE)\<rparr>"

(* Grow: increase rw_eff_window toward ROLLING_WINDOW_SIZE. *)
definition window_grow :: "rolling_window_state \<Rightarrow> rolling_window_state" where
  "window_grow rw =
     rw\<lparr>rw_eff_window :=
           min ROLLING_WINDOW_SIZE (rw_eff_window rw + ADAPTIVE_GROWTH_THRESHOLD)\<rparr>"

lemma window_shrink_lb:
  "rw_eff_window (window_shrink rw) \<ge> ADAPTIVE_MIN_WINDOW_SIZE"
  by (simp add: window_shrink_def)

lemma window_shrink_ub:
  assumes "rw_eff_window rw \<le> ROLLING_WINDOW_SIZE"
  shows "rw_eff_window (window_shrink rw) \<le> ROLLING_WINDOW_SIZE"
  using assms by (simp add: window_shrink_def)

lemma window_shrink_mono:
  "rw_eff_window (window_shrink rw) \<le> rw_eff_window rw"
  by (simp add: window_shrink_def)

lemma window_shrink_preserves_invariant:
  assumes "window_invariant rw"
  shows "window_invariant (window_shrink rw)"
  using assms by (simp add: window_invariant_def window_shrink_def)

lemma window_grow_lb:
  assumes "rw_eff_window rw \<ge> ADAPTIVE_MIN_WINDOW_SIZE"
  shows "rw_eff_window (window_grow rw) \<ge> ADAPTIVE_MIN_WINDOW_SIZE"
  using assms by (simp add: window_grow_def)

lemma window_grow_ub:
  "rw_eff_window (window_grow rw) \<le> ROLLING_WINDOW_SIZE"
  by (simp add: window_grow_def)

lemma window_grow_mono:
  "rw_eff_window (window_grow rw) \<ge> rw_eff_window rw"
  by (simp add: window_grow_def)

lemma window_grow_preserves_invariant:
  assumes "window_invariant rw"
  shows "window_invariant (window_grow rw)"
  using assms by (simp add: window_invariant_def window_grow_def)

(* =========================================================================
   Section 4: Forced resize from inference engine (Loop #5 or #6 override)
   ======================================================================== *)

(* set_eff_window: directly set rw_eff_window to a new value, clamped to range. *)
definition set_eff_window :: "nat \<Rightarrow> rolling_window_state \<Rightarrow> rolling_window_state" where
  "set_eff_window n rw =
     rw\<lparr>rw_eff_window :=
           max ADAPTIVE_MIN_WINDOW_SIZE (min ROLLING_WINDOW_SIZE n)\<rparr>"

lemma set_eff_window_in_range:
  "rw_eff_window (set_eff_window n rw) \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
   rw_eff_window (set_eff_window n rw) \<le> ROLLING_WINDOW_SIZE"
  by (simp add: set_eff_window_def ROLLING_WINDOW_SIZE_def ADAPTIVE_MIN_WINDOW_SIZE_def)

lemma set_eff_window_preserves_invariant:
  assumes "window_invariant rw"
  shows "window_invariant (set_eff_window n rw)"
  using assms
  by (simp add: window_invariant_def set_eff_window_def
                ROLLING_WINDOW_SIZE_def ADAPTIVE_MIN_WINDOW_SIZE_def)

(* =========================================================================
   Section 5: VM-level window state preservation by word execution
   ======================================================================== *)

lemma word_exec_preserves_window:
  "rolling_window (vm\<lparr>data_stack := xs\<rparr>) = rolling_window vm"
  by simp

lemma word_exec_preserves_window_invariant:
  assumes "window_invariant (rolling_window vm)"
  shows "window_invariant (rolling_window (vm\<lparr>data_stack := xs\<rparr>))"
  using assms by simp

end
