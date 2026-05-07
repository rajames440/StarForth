theory StarForth_Loop1_Heat
  imports StarForth_Base
begin

(* =========================================================================
   StarForth_Loop1_Heat — Execution Heat Tracking (Physics Loop #1)

   Mirrors: src/dictionary_heat_optimization.c
            src/word_source/ (heat increment on every word execution)

   Every time a word executes its de_heat field is incremented.  A background
   decay sweep (Loop #3) decrements heat over time.  Words can be FROZEN
   (heat does not decay) or PINNED (heat cannot reach zero).

   This theory proves:
     • Heat is non-negative throughout its lifecycle.
     • Heat increment strictly increases de_heat.
     • Heat is bounded above by a MAX_HEAT constant.
     • FROZEN words are immune to decay.
     • PINNED words' heat stays ≥ 1 after decay.
   ======================================================================== *)

(* =========================================================================
   Section 1: Heat constants
   ======================================================================== *)

(* Initial heat assigned when a word is first defined *)
definition HEAT_INIT         :: cell where "HEAT_INIT         = 0"
(* Maximum representable heat (arbitrary upper bound for well-formedness) *)
definition HEAT_MAX          :: cell where "HEAT_MAX          = 1000000"
(* Demote from hot-words cache below this threshold *)
definition HEAT_DEMOTION_THR :: cell where "HEAT_DEMOTION_THR = 10"

(* Word flag bits (match C macros in include/vm.h) *)
definition FLAG_FROZEN :: nat where "FLAG_FROZEN = 4"   \<comment> \<open>WORD_FROZEN 0x04\<close>
definition FLAG_PINNED :: nat where "FLAG_PINNED = 8"   \<comment> \<open>WORD_PINNED 0x08\<close>

(* =========================================================================
   Section 2: Heat predicates
   ======================================================================== *)

definition heat_frozen :: "dict_entry \<Rightarrow> bool" where
  "heat_frozen e \<longleftrightarrow> de_flags e AND FLAG_FROZEN = FLAG_FROZEN"

definition heat_pinned :: "dict_entry \<Rightarrow> bool" where
  "heat_pinned e \<longleftrightarrow> de_flags e AND FLAG_PINNED = FLAG_PINNED"

definition heat_valid :: "dict_entry \<Rightarrow> bool" where
  "heat_valid e \<longleftrightarrow> de_heat e \<ge> 0 \<and> de_heat e \<le> HEAT_MAX"

(* =========================================================================
   Section 3: Heat increment (fired on every word execution)
   ======================================================================== *)

(* Saturating increment: heat grows by 1, capped at HEAT_MAX. *)
definition heat_increment :: "dict_entry \<Rightarrow> dict_entry" where
  "heat_increment e =
     e\<lparr>de_heat := min (de_heat e + 1) HEAT_MAX\<rparr>"

lemma heat_increment_correct:
  assumes "de_heat e < HEAT_MAX"
  shows "de_heat (heat_increment e) = de_heat e + 1"
  by (simp add: heat_increment_def assms)

lemma heat_increment_saturates:
  assumes "de_heat e = HEAT_MAX"
  shows "de_heat (heat_increment e) = HEAT_MAX"
  by (simp add: heat_increment_def assms)

lemma heat_increment_non_decreasing:
  "de_heat (heat_increment e) \<ge> de_heat e"
  by (simp add: heat_increment_def)

lemma heat_increment_preserves_validity:
  assumes "heat_valid e"
  shows "heat_valid (heat_increment e)"
proof -
  have "de_heat e \<ge> 0" and "de_heat e \<le> HEAT_MAX"
    using assms by (simp_all add: heat_valid_def)
  thus ?thesis
    by (simp add: heat_valid_def heat_increment_def HEAT_MAX_def)
qed

lemma heat_increment_preserves_flags:
  "de_flags (heat_increment e) = de_flags e"
  by (simp add: heat_increment_def)

lemma heat_increment_preserves_frozen:
  "heat_frozen (heat_increment e) = heat_frozen e"
  by (simp add: heat_frozen_def heat_increment_def)

lemma heat_increment_preserves_pinned:
  "heat_pinned (heat_increment e) = heat_pinned e"
  by (simp add: heat_pinned_def heat_increment_def)

(* =========================================================================
   Section 4: Heat decay (fired by heartbeat Loop #3)
   ======================================================================== *)

(* A decay amount in cell units.  The C code uses Q48.16 slope × elapsed time,
   truncated to an integer; we abstract the amount as a parameter.           *)
definition heat_decay :: "cell \<Rightarrow> dict_entry \<Rightarrow> dict_entry" where
  "heat_decay amount e =
     (if heat_frozen e
      then e                        \<comment> \<open>FROZEN: no decay\<close>
      else if heat_pinned e
           then e\<lparr>de_heat := max 1 (de_heat e - amount)\<rparr>   \<comment> \<open>PINNED: floor at 1\<close>
           else e\<lparr>de_heat := max 0 (de_heat e - amount)\<rparr>)" \<comment> \<open>normal: floor at 0\<close>

lemma heat_decay_frozen:
  assumes "heat_frozen e"
  shows "heat_decay amount e = e"
  by (simp add: heat_decay_def assms)

lemma heat_decay_monotone:
  assumes "\<not> heat_frozen e"
  shows "de_heat (heat_decay amount e) \<le> de_heat e"
  by (simp add: heat_decay_def assms)

lemma heat_decay_non_negative:
  assumes "\<not> heat_frozen e"
  assumes "\<not> heat_pinned e"
  shows "de_heat (heat_decay amount e) \<ge> 0"
  by (simp add: heat_decay_def assms)

lemma heat_decay_pinned_positive:
  assumes "\<not> heat_frozen e"
  assumes "heat_pinned e"
  shows "de_heat (heat_decay amount e) \<ge> 1"
  by (simp add: heat_decay_def assms)

lemma heat_decay_preserves_validity:
  assumes "heat_valid e"
  assumes "\<not> heat_frozen e"
  assumes "amount \<ge> 0"
  shows "heat_valid (heat_decay amount e)"
proof (cases "heat_pinned e")
  case True
  thus ?thesis
    using assms by (simp add: heat_valid_def heat_decay_def HEAT_MAX_def)
next
  case False
  thus ?thesis
    using assms by (simp add: heat_valid_def heat_decay_def HEAT_MAX_def)
qed

lemma heat_decay_preserves_flags:
  "de_flags (heat_decay amount e) = de_flags e"
  by (simp add: heat_decay_def)

(* =========================================================================
   Section 5: Well-formedness of the dictionary heat state
   ======================================================================== *)

(* All dict entries reachable via the dictionary have valid heat. *)
definition dict_heat_wf :: "vm_state \<Rightarrow> bool" where
  "dict_heat_wf vm \<longleftrightarrow>
     \<forall>i e. dictionary vm i = Some e \<longrightarrow> heat_valid e"

(* =========================================================================
   Section 6: Heat thresholds
   ======================================================================== *)

(* The VM maintains 25th/50th/75th percentile thresholds for bucket search.
   Well-formedness: thresholds are in ascending order and non-negative.      *)
definition heat_thresholds_wf :: "vm_state \<Rightarrow> bool" where
  "heat_thresholds_wf vm \<longleftrightarrow>
     heat_threshold_25th vm \<ge> 0 \<and>
     heat_threshold_25th vm \<le> heat_threshold_50th vm \<and>
     heat_threshold_50th vm \<le> heat_threshold_75th vm \<and>
     heat_threshold_75th vm \<le> HEAT_MAX"

(* A pure data-stack word does not change heat thresholds or dictionary heat. *)
lemma ds_word_preserves_dict_heat:
  assumes "dictionary (vm\<lparr>data_stack := xs\<rparr>) = dictionary vm"
  shows "dict_heat_wf (vm\<lparr>data_stack := xs\<rparr>) = dict_heat_wf vm"
  by (simp add: dict_heat_wf_def assms)

lemma ds_word_preserves_heat_thresholds:
  "heat_thresholds_wf (vm\<lparr>data_stack := xs\<rparr>) = heat_thresholds_wf vm"
  by (simp add: heat_thresholds_wf_def)

end
