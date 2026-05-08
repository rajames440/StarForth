theory StarForth_Transition
  imports StarForth_Mutex
begin

(* =========================================================================
   StarForth_Transition — Labeled Transition System

   SPECIFICATION ROLE: ground truth for how ExecThread and HeartbeatThread
   interact.  C code must be written so that every execution of vm_tick()
   satisfies heartbeat_exec_neutral and every word satisfies
   word_physics_transparent.

   EXPLICIT AXIOM INVENTORY (this theory) — 2 axioms total:
     A1 ×1  heartbeat_exec_neutral  (C audit: src/vm_time.c)
     A4'×1  word_physics_transparent (definition audit: Level 1 word theories)

   The former 8 field axioms are now PROVED from A1 via exec_equiv.  The
   physics substate (rolling_window, heartbeat rate, decay_slope_q48, …) is
   intentionally unconstrained — the heartbeat is designed to evolve it
   non-monotonically.  exec_equiv makes that silence explicit and structural.
   ======================================================================== *)

(* =========================================================================
   Section 1: Thread and action datatypes
   ======================================================================== *)

datatype thread = ExecThread | HeartbeatThread

datatype action =
    ExecWord nat
  | HeartTick
  | AcquireTuning nat
  | ReleaseTuning nat
  | AcquireDict nat
  | ReleaseDict nat

type_synonym event = "thread \<times> action"

(* =========================================================================
   Section 2: Exec-equivalence — the quotient that separates exec from physics

   Two vm_states are exec-equivalent when they agree on every field that word
   execution depends on.  Physics fields are ABSENT by design: the heartbeat
   is an adaptive non-monotone controller of those fields, and no ordering or
   preservation claim is made about them here.

   exec_equiv is the congruence that collapses heartbeat_step to the identity
   in the exec quotient vm_state/≃, and under which word execution is a
   well-defined endomorphism.
   ======================================================================== *)

definition exec_equiv :: "vm_state \<Rightarrow> vm_state \<Rightarrow> bool" (infix "\<simeq>" 50) where
  "s1 \<simeq> s2 \<longleftrightarrow>
     data_stack   s1 = data_stack   s2 \<and>
     return_stack s1 = return_stack s2 \<and>
     memory       s1 = memory       s2 \<and>
     word_table   s1 = word_table   s2"

lemma exec_equiv_refl [simp, intro]: "vm \<simeq> vm"
  by (simp add: exec_equiv_def)

lemma exec_equiv_sym: "s1 \<simeq> s2 \<Longrightarrow> s2 \<simeq> s1"
  by (simp add: exec_equiv_def)

lemma exec_equiv_trans: "s1 \<simeq> s2 \<Longrightarrow> s2 \<simeq> s3 \<Longrightarrow> s1 \<simeq> s3"
  by (simp add: exec_equiv_def)

lemma exec_equiv_ds: "s1 \<simeq> s2 \<Longrightarrow> data_stack   s1 = data_stack   s2" by (simp add: exec_equiv_def)
lemma exec_equiv_rs: "s1 \<simeq> s2 \<Longrightarrow> return_stack s1 = return_stack s2" by (simp add: exec_equiv_def)
lemma exec_equiv_mem:"s1 \<simeq> s2 \<Longrightarrow> memory       s1 = memory       s2" by (simp add: exec_equiv_def)
lemma exec_equiv_wt: "s1 \<simeq> s2 \<Longrightarrow> word_table   s1 = word_table   s2" by (simp add: exec_equiv_def)

(* =========================================================================
   Section 3: Heartbeat axiom — A1 ×1 (collapsed from ×8)

   The heartbeat step is the IDENTITY in the exec quotient: heartbeat_step vm
   and vm are exec-equivalent.  This is the only claim the proof framework
   makes about heartbeat_step relative to exec-visible state.

   The physics substate (rolling_window, heartbeat rate, decay_slope_q48,
   pipeline_metrics, last_inference, ssm_l8, tuning_lock, dict_lock, heat
   thresholds, dictionary, vm_error, vm_halted, vm_mode) is not mentioned —
   the heartbeat may evolve it freely, non-monotonically, as the adaptive
   feedback loops require.  That freedom is the design intent.

   ⚠ AUDIT OBLIGATION (src/vm_time.c):
     Read every code path reachable from vm_tick() — including
     vm_tick_window_tuner, vm_tick_slope_validator, and all
     inference_engine.c callees — and verify that NONE of those paths write
     to the following four fields:
       data_stack   (vm->data_stack / vm->ds_top)
       return_stack (vm->return_stack / vm->rs_top)
       memory       (vm->memory[])
       word_table   (vm->word_table)
     Those four fields are exactly exec_equiv.  Everything else is free.

   ○ CODE-MUST-MATCH: if a future refactor moves any of those four fields into
   the heartbeat's write set, exec_equiv must be updated and a new audit
   performed before the axiom can be accepted.
   ======================================================================== *)

axiomatization heartbeat_step :: "vm_state \<Rightarrow> vm_state"
  where
  heartbeat_exec_neutral: "heartbeat_step vm \<simeq> vm"

(* =========================================================================
   Section 4: Single-step field lemmas — PROVED from A1 (not axioms)

   These carry [simp] so downstream proofs continue to work without change.
   ======================================================================== *)

lemma heartbeat_ds [simp]: "data_stack   (heartbeat_step vm) = data_stack vm"
  using heartbeat_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_rs [simp]: "return_stack (heartbeat_step vm) = return_stack vm"
  using heartbeat_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_mem [simp]: "memory       (heartbeat_step vm) = memory vm"
  using heartbeat_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_wt [simp]: "word_table   (heartbeat_step vm) = word_table vm"
  using heartbeat_exec_neutral by (simp add: exec_equiv_def)

(* =========================================================================
   Section 5: n-fold heartbeat exec-neutrality and field preservation
   ======================================================================== *)

lemma heartbeat_n_exec_neutral: "heartbeat_step ^^ n $ vm \<simeq> vm"
proof (induction n)
  case 0 show ?case by simp
next
  case (Suc k)
  show "heartbeat_step ^^ Suc k $ vm \<simeq> vm"
    using exec_equiv_trans [OF heartbeat_exec_neutral Suc.IH] by simp
qed

lemma heartbeat_n_steps_ds [simp]: "data_stack   (heartbeat_step ^^ n $ vm) = data_stack vm"
  using heartbeat_n_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_n_steps_rs [simp]: "return_stack (heartbeat_step ^^ n $ vm) = return_stack vm"
  using heartbeat_n_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_n_steps_mem [simp]: "memory       (heartbeat_step ^^ n $ vm) = memory vm"
  using heartbeat_n_exec_neutral by (simp add: exec_equiv_def)

lemma heartbeat_n_steps_wt [simp]: "word_table   (heartbeat_step ^^ n $ vm) = word_table vm"
  using heartbeat_n_exec_neutral by (simp add: exec_equiv_def)

(* =========================================================================
   Section 6: Word physics transparency — A4' ×1 (congruence form)

   Word execution is a well-defined endomorphism on the exec quotient: if two
   states are exec-equivalent, executing any word on each produces identical
   results.  This is the congruence law that makes word sequences independent
   of interleaved heartbeat ticks.

   ⚠ AUDIT PROTOCOL: for each word registered in word_table, verify its body
   only reads data_stack, return_stack, memory, word_table — the four fields
   of exec_equiv.  These are exactly the fields a FORTH word may observe.

   ○ CODE-MUST-MATCH: no word in src/word_source/ may read rolling_window,
   heartbeat, decay_slope_q48, pipeline_metrics, last_inference, ssm_l8,
   tuning_lock, dict_lock, heat thresholds, or any other physics field.
   ======================================================================== *)

axiomatization where
  word_physics_transparent:
    "\<And> (s1 :: vm_state) (s2 :: vm_state) n.
       s1 \<simeq> s2 \<Longrightarrow> word_table s1 n s1 = word_table s2 n s2"

(* =========================================================================
   Section 7: Consequences — all proved from A1 + A4'
   ======================================================================== *)

lemma exec_after_heartbeat_eq:
  "word_table (heartbeat_step vm) n (heartbeat_step vm) = word_table vm n vm"
  by (rule word_physics_transparent [OF heartbeat_exec_neutral])

lemma exec_after_heartbeat_ds:
  "data_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = data_stack (word_table vm n vm)"
  using exec_after_heartbeat_eq by simp

lemma exec_after_heartbeat_rs:
  "return_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = return_stack (word_table vm n vm)"
  using exec_after_heartbeat_eq by simp

lemma exec_after_n_heartbeats_eq:
  "word_table (heartbeat_step ^^ k $ vm) n (heartbeat_step ^^ k $ vm)
   = word_table vm n vm"
  by (rule word_physics_transparent [OF heartbeat_n_exec_neutral])

(* =========================================================================
   Section 8: Single-step transition relation
   ======================================================================== *)

inductive vm_step :: "vm_state \<Rightarrow> event \<Rightarrow> vm_state \<Rightarrow> bool"
  ("_ \<rightarrow>[_] _" [60, 0, 61] 60)
where
  StepExec:
    "\<not> vm_error vm \<Longrightarrow> \<not> vm_halted vm \<Longrightarrow>
     vm' = word_table vm n vm \<Longrightarrow>
     vm \<rightarrow>[(ExecThread, ExecWord n)] vm'"

| StepHeartTick:
    "\<not> vm_halted vm \<Longrightarrow>
     vm' = heartbeat_step vm \<Longrightarrow>
     vm \<rightarrow>[(HeartbeatThread, HeartTick)] vm'"

| StepAcqTuning:
    "tuning_lock vm = LockFree \<Longrightarrow>
     vm' = vm\<lparr>tuning_lock := LockHeld t\<rparr> \<Longrightarrow>
     vm \<rightarrow>[(if t = EXEC_THREAD then ExecThread else HeartbeatThread, AcquireTuning t)] vm'"

| StepRelTuning:
    "tuning_lock vm = LockHeld t \<Longrightarrow>
     vm' = vm\<lparr>tuning_lock := LockFree\<rparr> \<Longrightarrow>
     vm \<rightarrow>[(if t = EXEC_THREAD then ExecThread else HeartbeatThread, ReleaseTuning t)] vm'"

| StepAcqDict:
    "dict_lock vm = LockFree \<Longrightarrow>
     vm' = vm\<lparr>dict_lock := LockHeld t\<rparr> \<Longrightarrow>
     vm \<rightarrow>[(if t = EXEC_THREAD then ExecThread else HeartbeatThread, AcquireDict t)] vm'"

| StepRelDict:
    "dict_lock vm = LockHeld t \<Longrightarrow>
     vm' = vm\<lparr>dict_lock := LockFree\<rparr> \<Longrightarrow>
     vm \<rightarrow>[(if t = EXEC_THREAD then ExecThread else HeartbeatThread, ReleaseDict t)] vm'"

(* =========================================================================
   Section 9: Traces and reachability
   ======================================================================== *)

type_synonym trace = "(vm_state \<times> event \<times> vm_state) list"

fun valid_trace :: "trace \<Rightarrow> bool" where
  "valid_trace [] = True"
| "valid_trace [(s, e, s')] = (s \<rightarrow>[e] s')"
| "valid_trace ((s, e, s') # rest) =
     (s \<rightarrow>[e] s' \<and>
      (case rest of [] \<Rightarrow> True | (s0, _, _) # _ \<Rightarrow> s0 = s') \<and>
      valid_trace rest)"

definition reachable :: "vm_state \<Rightarrow> vm_state \<Rightarrow> bool" where
  "reachable s0 s' \<longleftrightarrow>
     s0 = s' \<or>
     (\<exists>tr. valid_trace tr \<and> tr \<noteq> [] \<and>
           fst (hd tr) = s0 \<and>
           snd (snd (last tr)) = s')"

definition exec_events :: "trace \<Rightarrow> (vm_state \<times> nat \<times> vm_state) list" where
  "exec_events tr =
     [(s, n, s'). (s, (ExecThread, ExecWord n), s') \<leftarrow> tr]"

end
