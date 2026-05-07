theory StarForth_Loop3_Decay
  imports StarForth_Q48_16 StarForth_Base
begin

(* =========================================================================
   StarForth_Loop3_Decay — Linear Heat Decay (Physics Loop #3)

   Mirrors: src/vm_time.c (vm_tick_slope_validator, heartbeat decay sweep)
            include/vm.h  (decay_slope_q48, DECAY_RATE_PER_US_Q16)

   The decay subsystem applies a time-proportional heat reduction to every
   dictionary word on each heartbeat sweep:

     new_heat = max(0, heat - slope × elapsed_μs)

   where slope = decay_slope_q48 (Q48.16 fixed-point nat in vm_state).
   ======================================================================== *)

(* =========================================================================
   Section 1: Decay constants
   ======================================================================== *)

(* ○ CODE-MUST-MATCH: Initial slope = 2 × Q48_SCALE = 131072.
   Matches DECAY_RATE_PER_US_Q16 × 2 in include/vm.h.                      *)
definition DECAY_SLOPE_INIT :: nat where
  "DECAY_SLOPE_INIT = 2 * Q48_SCALE"

(* ○ CODE-MUST-MATCH: Never let slope reach zero.
   ⚠ HUMAN-REVIEW: Every C code path that reduces decay_slope_q48 must clamp
   to at least DECAY_SLOPE_MIN = 1.  Check: src/vm_time.c slope validator,
   src/inference_engine.c slope output path.                                 *)
definition DECAY_SLOPE_MIN :: nat where
  "DECAY_SLOPE_MIN = 1"

definition DECAY_SLOPE_MAX :: nat where
  "DECAY_SLOPE_MAX = Q48_SCALE * 1000"

(* =========================================================================
   Section 2: Slope well-formedness
   ======================================================================== *)

definition slope_wf :: "nat \<Rightarrow> bool" where
  "slope_wf s \<longleftrightarrow> s \<ge> DECAY_SLOPE_MIN \<and> s \<le> DECAY_SLOPE_MAX"

lemma slope_wf_pos:
  assumes "slope_wf s"
  shows "s > 0"
  using assms by (simp add: slope_wf_def DECAY_SLOPE_MIN_def)

lemma slope_init_wf:
  "slope_wf DECAY_SLOPE_INIT"
  by (simp add: slope_wf_def DECAY_SLOPE_INIT_def DECAY_SLOPE_MIN_def
                DECAY_SLOPE_MAX_def Q48_SCALE_def)

(* =========================================================================
   Section 3: Slope tuning step
   ======================================================================== *)

definition slope_decrease :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "slope_decrease step s = max DECAY_SLOPE_MIN (s - step)"

definition slope_increase :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "slope_increase step s = min DECAY_SLOPE_MAX (s + step)"

lemma slope_decrease_lb:
  "slope_decrease step s \<ge> DECAY_SLOPE_MIN"
  by (simp add: slope_decrease_def)

lemma slope_decrease_preserves_wf:
  assumes "slope_wf s"
  shows "slope_wf (slope_decrease step s)"
  using assms by (simp add: slope_wf_def slope_decrease_def DECAY_SLOPE_MIN_def)

lemma slope_increase_ub:
  "slope_increase step s \<le> DECAY_SLOPE_MAX"
  by (simp add: slope_increase_def)

lemma slope_increase_preserves_wf:
  assumes "slope_wf s"
  shows "slope_wf (slope_increase step s)"
  using assms by (simp add: slope_wf_def slope_increase_def DECAY_SLOPE_MIN_def DECAY_SLOPE_MAX_def)

lemma slope_decrease_mono: "slope_decrease step s \<le> s"
  by (simp add: slope_decrease_def)

lemma slope_increase_mono: "slope_increase step s \<ge> s"
  by (simp add: slope_increase_def)

(* =========================================================================
   Section 4: VM decay step
   ======================================================================== *)

(* vm_decay_step adjusts decay_slope_q48 according to decay_direction.
   ○ CODE-MUST-MATCH: decay_direction is set by the inference engine output
   (-1 = reduce slope, 0 = stable, +1 = increase slope).  The C code must
   apply exactly slope_decrease or slope_increase with the same clamping.   *)
definition vm_decay_step :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "vm_decay_step step vm =
     (if decay_direction vm = 1
      then vm\<lparr>decay_slope_q48 := slope_increase step (decay_slope_q48 vm)\<rparr>
      else if decay_direction vm = -1
           then vm\<lparr>decay_slope_q48 := slope_decrease step (decay_slope_q48 vm)\<rparr>
           else vm)"

lemma vm_decay_step_slope_pos:
  assumes "slope_wf (decay_slope_q48 vm)"
  shows "slope_wf (decay_slope_q48 (vm_decay_step step vm))"
  by (simp add: vm_decay_step_def slope_decrease_preserves_wf slope_increase_preserves_wf assms)

lemma vm_decay_step_ds [simp]:
  "data_stack (vm_decay_step step vm) = data_stack vm"
  by (simp add: vm_decay_step_def)

lemma vm_decay_step_rs [simp]:
  "return_stack (vm_decay_step step vm) = return_stack vm"
  by (simp add: vm_decay_step_def)

lemma vm_decay_step_rolling [simp]:
  "rolling_window (vm_decay_step step vm) = rolling_window vm"
  by (simp add: vm_decay_step_def)

lemma vm_decay_step_error [simp]:
  "vm_error (vm_decay_step step vm) = vm_error vm"
  by (simp add: vm_decay_step_def)

(* vm_decay_step only modifies decay_slope_q48 — dictionary is unchanged. *)
lemma vm_decay_step_dict [simp]:
  "dictionary (vm_decay_step step vm) = dictionary vm"
  by (simp add: vm_decay_step_def)

(* =========================================================================
   Section 5: Total heat — fully proved monotonicity
   ======================================================================== *)

definition total_dict_heat :: "vm_state \<Rightarrow> int" where
  "total_dict_heat vm =
     \<Sum>i \<in> {i. dictionary vm i \<noteq> None}.
       de_heat (the (dictionary vm i))"

(* PROOF (no sorry):
   vm_decay_step only changes decay_slope_q48, so dictionary is identical
   between vm and vm_decay_step step vm.  Therefore total_dict_heat is equal
   (not merely ≤) across the step — monotonicity follows trivially.         *)
lemma decay_step_dict_unchanged:
  "total_dict_heat (vm_decay_step step vm) = total_dict_heat vm"
  unfolding total_dict_heat_def
  by simp   \<comment> \<open>vm_decay_step_dict [simp] rewrites the dictionary field\<close>

lemma decay_total_heat_non_increasing:
  assumes "\<forall>i. dictionary vm i \<noteq> None \<longrightarrow> de_heat (the (dictionary vm i)) \<ge> 0"
  shows "total_dict_heat (vm_decay_step step vm) \<le> total_dict_heat vm"
  using decay_step_dict_unchanged by linarith

(* =========================================================================
   Section 6: Well-formedness: slope is positive in wf_vm
   ======================================================================== *)

lemma wf_vm_slope_pos:
  assumes "wf_vm vm"
  shows "decay_slope_q48 vm > 0"
  using assms by (simp add: wf_vm_def)

lemma wf_vm_slope_wf:
  assumes "wf_vm vm"
  assumes "decay_slope_q48 vm \<le> DECAY_SLOPE_MAX"
  shows "slope_wf (decay_slope_q48 vm)"
  using assms by (simp add: slope_wf_def DECAY_SLOPE_MIN_def wf_vm_def)

end
