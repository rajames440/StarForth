theory StarForth_Loop4_Pipeline
  imports StarForth_Q48_16 StarForth_Base
begin

(* =========================================================================
   StarForth_Loop4_Pipeline — Prefetch / Pipelining Metrics (Physics Loop #4)

   Mirrors: src/physics_pipelining_metrics.c
            include/physics_pipelining_metrics.h

   The pipelining subsystem maintains word-to-word transition probabilities
   (per-word wt_transition_heat arrays) and global pipeline_metrics.  On
   every word execution, the most-likely next word is pre-fetched from the
   dictionary; the prefetch is scored as a hit or miss.

   This theory proves:
     • Prefetch accuracy (hits / (hits + misses)) ∈ [0, 1] as a Q48.16 value.
     • Transition recording increments the correct counter and preserves totals.
     • Global pipeline_metrics totals are consistent with per-step deltas.
   ======================================================================== *)

(* =========================================================================
   Section 1: Pipeline accuracy
   ======================================================================== *)

definition pm_accuracy_q48 :: "pipeline_metrics_state \<Rightarrow> nat" where
  "pm_accuracy_q48 pm =
     (if pm_prefetch_attempts pm = 0
      then 0
      else (pm_prefetch_hits pm * Q48_SCALE) div pm_prefetch_attempts pm)"

lemma pm_accuracy_zero_attempts [simp]:
  "pm_accuracy_q48 (pm\<lparr>pm_prefetch_attempts := 0\<rparr>) = 0"
  by (simp add: pm_accuracy_q48_def)

lemma pm_accuracy_upper_bound:
  assumes "pm_prefetch_hits pm \<le> pm_prefetch_attempts pm"
  shows "pm_accuracy_q48 pm \<le> Q48_ONE"
proof (cases "pm_prefetch_attempts pm = 0")
  case True thus ?thesis by (simp add: pm_accuracy_q48_def Q48_ONE_def)
next
  case False
  have "pm_prefetch_hits pm * Q48_SCALE \<le> pm_prefetch_attempts pm * Q48_SCALE"
    using assms by (simp add: mult_le_mono1)
  hence "(pm_prefetch_hits pm * Q48_SCALE) div pm_prefetch_attempts pm \<le> Q48_SCALE"
    using False by (simp add: div_le_iff_le_mult)
  thus ?thesis by (simp add: pm_accuracy_q48_def Q48_ONE_def False)
qed

lemma pm_accuracy_non_negative:
  "pm_accuracy_q48 pm \<ge> 0"
  by simp

(* =========================================================================
   Section 2: Pipeline metrics well-formedness
   ======================================================================== *)

definition pm_wf :: "pipeline_metrics_state \<Rightarrow> bool" where
  "pm_wf pm \<longleftrightarrow>
     pm_prefetch_hits pm \<le> pm_prefetch_attempts pm \<and>
     (pm_prefetch_attempts pm = 0 \<longrightarrow>
       pm_last_accuracy_num pm = 0) \<and>
     (pm_prefetch_attempts pm > 0 \<longrightarrow>
       pm_last_accuracy_den pm > 0)"

lemma pm_wf_accuracy_range:
  assumes "pm_wf pm"
  shows "pm_accuracy_q48 pm \<le> Q48_ONE"
  using assms by (simp add: pm_accuracy_upper_bound pm_wf_def)

(* =========================================================================
   Section 3: Recording a prefetch hit or miss
   ======================================================================== *)

definition pm_record_hit :: "pipeline_metrics_state \<Rightarrow> pipeline_metrics_state" where
  "pm_record_hit pm =
     pm\<lparr>pm_prefetch_attempts := pm_prefetch_attempts pm + 1,
         pm_prefetch_hits     := pm_prefetch_hits pm + 1\<rparr>"

definition pm_record_miss :: "pipeline_metrics_state \<Rightarrow> pipeline_metrics_state" where
  "pm_record_miss pm =
     pm\<lparr>pm_prefetch_attempts := pm_prefetch_attempts pm + 1\<rparr>"

lemma pm_record_hit_attempts:
  "pm_prefetch_attempts (pm_record_hit pm) = pm_prefetch_attempts pm + 1"
  by (simp add: pm_record_hit_def)

lemma pm_record_hit_hits:
  "pm_prefetch_hits (pm_record_hit pm) = pm_prefetch_hits pm + 1"
  by (simp add: pm_record_hit_def)

lemma pm_record_miss_attempts:
  "pm_prefetch_attempts (pm_record_miss pm) = pm_prefetch_attempts pm + 1"
  by (simp add: pm_record_miss_def)

lemma pm_record_miss_hits_unchanged:
  "pm_prefetch_hits (pm_record_miss pm) = pm_prefetch_hits pm"
  by (simp add: pm_record_miss_def)

lemma pm_record_hit_preserves_wf:
  assumes "pm_wf pm"
  shows "pm_wf (pm_record_hit pm)"
  using assms by (simp add: pm_wf_def pm_record_hit_def)

lemma pm_record_miss_preserves_wf:
  assumes "pm_wf pm"
  shows "pm_wf (pm_record_miss pm)"
  using assms by (simp add: pm_wf_def pm_record_miss_def)

(* After a hit, hits ≤ attempts still holds. *)
lemma pm_record_hit_hits_le_attempts:
  assumes "pm_prefetch_hits pm \<le> pm_prefetch_attempts pm"
  shows "pm_prefetch_hits (pm_record_hit pm) \<le> pm_prefetch_attempts (pm_record_hit pm)"
  using assms by (simp add: pm_record_hit_def)

(* After a miss, hits ≤ attempts still holds. *)
lemma pm_record_miss_hits_le_attempts:
  assumes "pm_prefetch_hits pm \<le> pm_prefetch_attempts pm"
  shows "pm_prefetch_hits (pm_record_miss pm) \<le> pm_prefetch_attempts (pm_record_miss pm)"
  using assms by (simp add: pm_record_miss_def)

(* =========================================================================
   Section 4: Word transition metrics (per-word hot-path transition table)
   ======================================================================== *)

(* Record a transition from the current context to word_id next. *)
definition wt_record_transition :: "nat \<Rightarrow> word_transition_metrics \<Rightarrow> word_transition_metrics" where
  "wt_record_transition next wt =
     wt\<lparr>wt_transition_heat :=
             (wt_transition_heat wt)(next := wt_transition_heat wt next + 1),
         wt_total_transitions := wt_total_transitions wt + 1\<rparr>"

lemma wt_transition_heat_updated:
  "wt_transition_heat (wt_record_transition n wt) n = wt_transition_heat wt n + 1"
  by (simp add: wt_record_transition_def)

lemma wt_transition_heat_other_unchanged:
  assumes "m \<noteq> n"
  shows "wt_transition_heat (wt_record_transition n wt) m = wt_transition_heat wt m"
  by (simp add: wt_record_transition_def assms)

lemma wt_total_transitions_increases:
  "wt_total_transitions (wt_record_transition n wt) = wt_total_transitions wt + 1"
  by (simp add: wt_record_transition_def)

(* =========================================================================
   Section 5: Pipeline fields are preserved by pure data-stack word execution
   ======================================================================== *)

lemma ds_word_preserves_pipeline:
  "pipeline_metrics (vm\<lparr>data_stack := xs\<rparr>) = pipeline_metrics vm"
  by simp

end
