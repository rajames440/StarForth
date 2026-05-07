theory StarForth_Correctness
  imports
    StarForth_Stack_Words
    StarForth_Arithmetic_Words
    StarForth_Logical_Words
    StarForth_Return_Stack_Words
    StarForth_Memory_Words
    StarForth_Loop1_Heat
    StarForth_Loop2_Window
    StarForth_Loop3_Decay
    StarForth_Loop4_Pipeline
    StarForth_Loop5_WinInf
    StarForth_Loop6_DecayInf
    StarForth_Loop7_Heartrate
    StarForth_Concurrent
begin

(* =========================================================================
   StarForth_Correctness — Top-Level Totality Theorem

   SORRY-FREE.  Every claim in this file is either:
     ✓ proved from definitions (fully mechanised)
     ○ proved from an explicit named axiom (audit-required, no sorry)

   EXPLICIT AXIOM INVENTORY (2 axioms total — no sorry):

     A1 ×1. heartbeat_exec_neutral    (StarForth_Transition)
              heartbeat_step vm ≃ vm
              The heartbeat is the identity in vm_state/≃ (exec_equiv).
              C audit: src/vm_time.c vm_tick() must NOT write to
              data_stack, return_stack, memory, or word_table.
              The physics substate is unconstrained — the heartbeat may
              evolve it freely and non-monotonically.
              The former 8 field axioms are now proved lemmas from A1.

     A4'×1. word_physics_transparent  (StarForth_Transition)
              s1 ≃ s2 ⟹ word_table s1 n s1 = word_table s2 n s2
              Word execution is a congruence law for ≃.
              Word audit: every Level 1 word body must read ONLY the four
              fields in exec_equiv — never any physics field.

   Physics invariants (A2/A3) remain removed — clamping suffices.
   heartbeat_trace_noninterference is a proved theorem (foldl induction).

   TRANSITIVITY CHAIN:
     word semantics (✓ Level 1)
       → physics invariants (✓ Level 2, proved by clamping — axiom-free)
         → concurrent non-interference (✓ A1 + A4', fully proved)
           → OS-level correctness (by transitivity, not in scope here)
   ======================================================================== *)

(* =========================================================================
   Section 1: Level 1 — Primitive word correctness (all ✓)
   ======================================================================== *)

thm drop_normal           \<comment> \<open>✓ DROP pops TOS\<close>
thm dup_normal            \<comment> \<open>✓ DUP duplicates TOS\<close>
thm swap_normal           \<comment> \<open>✓ SWAP exchanges top two\<close>
thm over_normal           \<comment> \<open>✓ OVER copies second to top\<close>
thm rot_normal            \<comment> \<open>✓ ROT cycles top 3\<close>
thm swap_involutive       \<comment> \<open>✓ SWAP ∘ SWAP = identity\<close>
thm rot_neg_rot_identity  \<comment> \<open>✓ ROT ∘ -ROT = identity\<close>
thm add_normal            \<comment> \<open>✓ + pops 2, pushes sum\<close>
thm mul_comm              \<comment> \<open>✓ * is commutative\<close>
thm negate_involutive     \<comment> \<open>✓ NEGATE ∘ NEGATE = identity\<close>
thm abs_non_negative      \<comment> \<open>✓ ABS result ≥ 0\<close>
thm and_normal            \<comment> \<open>✓ AND bitwise\<close>
thm zero_eq_true          \<comment> \<open>✓ 0= of 0 is FORTH_TRUE\<close>
thm zero_lt_exhaustion    \<comment> \<open>✓ {0=, 0<, 0>} partition ℤ\<close>
thm to_r_then_from_r      \<comment> \<open>✓ >R then R> round-trip\<close>
thm store_then_fetch      \<comment> \<open>✓ ! then @ identity\<close>
thm cstore_then_cfetch    \<comment> \<open>✓ C! then C@ round-trip\<close>

(* =========================================================================
   Section 2: Physics field preservation by word execution (all ✓)
   ======================================================================== *)

lemma pure_ds_word_preserves_physics:
  "rolling_window   (vm\<lparr>data_stack := xs\<rparr>) = rolling_window vm"
  "heartbeat        (vm\<lparr>data_stack := xs\<rparr>) = heartbeat vm"
  "decay_slope_q48  (vm\<lparr>data_stack := xs\<rparr>) = decay_slope_q48 vm"
  "pipeline_metrics (vm\<lparr>data_stack := xs\<rparr>) = pipeline_metrics vm"
  "last_inference   (vm\<lparr>data_stack := xs\<rparr>) = last_inference vm"
  "ssm_l8           (vm\<lparr>data_stack := xs\<rparr>) = ssm_l8 vm"
  "dictionary       (vm\<lparr>data_stack := xs\<rparr>) = dictionary vm"
  "word_table       (vm\<lparr>data_stack := xs\<rparr>) = word_table vm"
  by simp_all

(* =========================================================================
   Section 3: Level 2 — Physics invariants (all ✓ or ○)
   ======================================================================== *)

thm window_advance_preserves_invariant  \<comment> \<open>✓ advance preserves window_invariant\<close>
thm window_grow_preserves_invariant     \<comment> \<open>✓ grow preserves window_invariant\<close>
thm window_shrink_preserves_invariant   \<comment> \<open>✓ shrink preserves window_invariant\<close>
thm vm_decay_step_slope_pos             \<comment> \<open>✓ decay step preserves slope_wf\<close>
thm decay_step_dict_unchanged           \<comment> \<open>✓ decay does not touch dictionary\<close>
thm pm_accuracy_upper_bound             \<comment> \<open>✓ accuracy ≤ Q48_ONE\<close>
thm clamp_window_in_range               \<comment> \<open>✓ clamped window ∈ [MIN,MAX]\<close>
thm apply_window_inference_preserves_invariant  \<comment> \<open>✓ proved by clamping (set_eff_window)\<close>
thm clamp_slope_wf                      \<comment> \<open>✓ clamped slope satisfies slope_wf\<close>
thm apply_slope_inference_preserves_wf  \<comment> \<open>✓ slope inference preserves slope_wf\<close>
thm hb_shorten_preserves_wf             \<comment> \<open>✓ shorten preserves hb_state_wf\<close>
thm hb_lengthen_preserves_wf            \<comment> \<open>✓ lengthen preserves hb_state_wf\<close>
thm hb_fire_tick_preserves_wf           \<comment> \<open>✓ tick increment preserves hb_state_wf\<close>

(* =========================================================================
   Section 4: Level 3 — Concurrency (all ✓ or ○)
   ======================================================================== *)

thm heartbeat_noninterference      \<comment> \<open>✓ one word: heartbeat does not affect result\<close>
thm heartbeat_noninterference_rs   \<comment> \<open>✓ one word: return_stack unaffected\<close>
thm heartbeat_trace_noninterference \<comment> \<open>✓ proved (foldl induction + A4')\<close>
thm mutex_exclusive                \<comment> \<open>✓ at most one holder per lock\<close>
thm concurrent_locks_safe_trivial  \<comment> \<open>✓ trivial from lock_state algebra\<close>

(* =========================================================================
   Section 5: Master correctness theorem (proved — no sorry)
   ======================================================================== *)

(* The master theorem collects all correctness invariants into a single
   provable conjunction.  Every conjunct is either directly proved or follows
   from an explicitly named axiom (A1, A4' listed in the header).           *)
theorem starforth_correctness_totality:
  fixes vm :: vm_state
  assumes wf: "wf_vm vm"
  shows
    (* Level 1: key wf_vm invariants hold *)
    "length (data_stack vm)   \<le> STACK_SIZE \<and>
     length (return_stack vm) \<le> STACK_SIZE \<and>
     \<not> vm_error vm \<and>
     (* Level 2: physics invariants from wf_vm *)
     rw_eff_window (rolling_window vm) \<ge> ADAPTIVE_MIN_WINDOW_SIZE \<and>
     rw_eff_window (rolling_window vm) \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window (rolling_window vm) \<le> ROLLING_WINDOW_SIZE \<and>
     rw_act_window (rolling_window vm) =
       min (rw_total_exec (rolling_window vm)) ROLLING_WINDOW_SIZE \<and>
     decay_slope_q48 vm > 0 \<and>
     hb_tick_target_ns (heartbeat vm) > 0"
  using wf by (simp add: wf_vm_def)

(* Corollary: non-interference for a word executed after k heartbeat ticks. *)
corollary word_result_heartbeat_independent:
  "\<forall> n k vm.
     data_stack (word_table (heartbeat_step ^^ k $ vm) n (heartbeat_step ^^ k $ vm))
     = data_stack (word_table vm n vm)"
  using heartbeat_noninterference by blast

(* Corollary: trace-level non-interference (proved theorem). *)
corollary trace_result_heartbeat_independent:
  "\<And> words vm k.
     data_stack (foldl (\<lambda>s n. word_table s n s) (heartbeat_step ^^ k $ vm) words)
     = data_stack (foldl (\<lambda>s n. word_table s n s) vm words)"
  using heartbeat_trace_noninterference by blast

end
