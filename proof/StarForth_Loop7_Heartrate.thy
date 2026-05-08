theory StarForth_Loop7_Heartrate
  imports StarForth_Base
begin

(* =========================================================================
   StarForth_Loop7_Heartrate — Adaptive Heartrate (Physics Loop #7)

   Mirrors: src/vm_time.c (heartbeat thread, tick_target_ns updates)
            include/vm.h  (HeartbeatState.tick_target_ns)

   The heartbeat thread wakes on a nanosecond timer (tick_target_ns cadence).
   Loop #7 adjusts tick_target_ns based on observed jitter and load:
     • If ticks are arriving too slowly (jitter > threshold), shorten period.
     • If ticks arrive fast enough, lengthen period to save CPU.

   Invariants:
     • tick_target_ns > 0 at all times.
     • tick_target_ns ∈ [TICK_MIN_NS, TICK_MAX_NS].
     • hb_tick_count increases monotonically.
   ======================================================================== *)

(* =========================================================================
   Section 1: Heartrate bounds
   ======================================================================== *)

(* Minimum tick period: 100 μs = 100,000 ns (matches HEARTBEAT_WINDOW_TUNING_FREQUENCY
   comment: tune every 1000 ticks, implying a ~1ms default tick period).    *)
definition TICK_MIN_NS :: nat where "TICK_MIN_NS = 100000"       \<comment> \<open>100 μs\<close>
definition TICK_MAX_NS :: nat where "TICK_MAX_NS = 100000000"    \<comment> \<open>100 ms\<close>
definition TICK_DEFAULT_NS :: nat where "TICK_DEFAULT_NS = 1000000"  \<comment> \<open>1 ms\<close>

lemma tick_min_pos: "TICK_MIN_NS > 0"
  by (simp add: TICK_MIN_NS_def)

lemma tick_default_in_range:
  "TICK_MIN_NS \<le> TICK_DEFAULT_NS \<and> TICK_DEFAULT_NS \<le> TICK_MAX_NS"
  by (simp add: TICK_MIN_NS_def TICK_DEFAULT_NS_def TICK_MAX_NS_def)

(* =========================================================================
   Section 2: Heartbeat state well-formedness
   ======================================================================== *)

definition hb_state_wf :: "heartbeat_state \<Rightarrow> bool" where
  "hb_state_wf hb \<longleftrightarrow>
     hb_tick_target_ns hb \<ge> TICK_MIN_NS \<and>
     hb_tick_target_ns hb \<le> TICK_MAX_NS \<and>
     hb_tick_target_ns hb > 0"

lemma hb_state_wf_pos:
  assumes "hb_state_wf hb"
  shows "hb_tick_target_ns hb > 0"
  using assms by (simp add: hb_state_wf_def)

lemma hb_state_wf_lb:
  assumes "hb_state_wf hb"
  shows "hb_tick_target_ns hb \<ge> TICK_MIN_NS"
  using assms by (simp add: hb_state_wf_def)

lemma hb_state_wf_ub:
  assumes "hb_state_wf hb"
  shows "hb_tick_target_ns hb \<le> TICK_MAX_NS"
  using assms by (simp add: hb_state_wf_def)

(* =========================================================================
   Section 3: Tick period adjustment
   ======================================================================== *)

(* Shorten tick period (increase tick rate) — saturate at TICK_MIN_NS. *)
definition hb_shorten_period :: "nat \<Rightarrow> heartbeat_state \<Rightarrow> heartbeat_state" where
  "hb_shorten_period delta hb =
     hb\<lparr>hb_tick_target_ns := max TICK_MIN_NS (hb_tick_target_ns hb - delta)\<rparr>"

(* Lengthen tick period (decrease tick rate) — saturate at TICK_MAX_NS. *)
definition hb_lengthen_period :: "nat \<Rightarrow> heartbeat_state \<Rightarrow> heartbeat_state" where
  "hb_lengthen_period delta hb =
     hb\<lparr>hb_tick_target_ns := min TICK_MAX_NS (hb_tick_target_ns hb + delta)\<rparr>"

lemma hb_shorten_lb:
  "hb_tick_target_ns (hb_shorten_period delta hb) \<ge> TICK_MIN_NS"
  by (simp add: hb_shorten_period_def)

lemma hb_shorten_pos:
  "hb_tick_target_ns (hb_shorten_period delta hb) > 0"
  using tick_min_pos by (simp add: hb_shorten_period_def)

lemma hb_shorten_preserves_wf:
  assumes "hb_state_wf hb"
  shows "hb_state_wf (hb_shorten_period delta hb)"
  using assms by (simp add: hb_state_wf_def hb_shorten_period_def TICK_MAX_NS_def TICK_MIN_NS_def)

lemma hb_lengthen_ub:
  "hb_tick_target_ns (hb_lengthen_period delta hb) \<le> TICK_MAX_NS"
  by (simp add: hb_lengthen_period_def)

lemma hb_lengthen_preserves_pos:
  assumes "hb_tick_target_ns hb > 0"
  shows "hb_tick_target_ns (hb_lengthen_period delta hb) > 0"
  using assms by (simp add: hb_lengthen_period_def)

lemma hb_lengthen_preserves_wf:
  assumes "hb_state_wf hb"
  shows "hb_state_wf (hb_lengthen_period delta hb)"
  using assms by (simp add: hb_state_wf_def hb_lengthen_period_def TICK_MIN_NS_def TICK_MAX_NS_def)

(* =========================================================================
   Section 4: Tick counter monotonicity
   ======================================================================== *)

definition hb_fire_tick :: "heartbeat_state \<Rightarrow> heartbeat_state" where
  "hb_fire_tick hb = hb\<lparr>hb_tick_count := hb_tick_count hb + 1\<rparr>"

lemma hb_fire_tick_count_increases:
  "hb_tick_count (hb_fire_tick hb) = hb_tick_count hb + 1"
  by (simp add: hb_fire_tick_def)

lemma hb_fire_tick_count_monotone:
  "hb_tick_count (hb_fire_tick hb) \<ge> hb_tick_count hb"
  by (simp add: hb_fire_tick_def)

lemma hb_fire_tick_preserves_target:
  "hb_tick_target_ns (hb_fire_tick hb) = hb_tick_target_ns hb"
  by (simp add: hb_fire_tick_def)

lemma hb_fire_tick_preserves_wf:
  assumes "hb_state_wf hb"
  shows "hb_state_wf (hb_fire_tick hb)"
  using assms by (simp add: hb_state_wf_def hb_fire_tick_def)

(* =========================================================================
   Section 5: VM-level heartbeat well-formedness
   ======================================================================== *)

lemma wf_vm_hb_pos:
  assumes "wf_vm vm"
  shows "hb_tick_target_ns (heartbeat vm) > 0"
  using assms by (simp add: wf_vm_def)

lemma ds_word_preserves_heartbeat_wf:
  assumes "hb_state_wf (heartbeat vm)"
  shows "hb_state_wf (heartbeat (vm\<lparr>data_stack := xs\<rparr>))"
  using assms by simp

end
