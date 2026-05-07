theory StarForth_Transition
  imports StarForth_Mutex
begin

(* =========================================================================
   SKETCH: StarForth_Transition — Labeled Transition System

   SPECIFICATION ROLE: ground truth for how ExecThread and HeartbeatThread
   interact.  C code must be written so that every execution of vm_tick()
   satisfies the heartbeat_step axioms and every word function satisfies the
   word_physics_transparent axiom.

   EXPLICIT AXIOM INVENTORY (this theory):
     A1 ×8  heartbeat_step field axioms  (C audit: src/vm_time.c)
     A4'×1  word_physics_transparent     (definition audit: each word in
                                          StarForth_Stack/Arithmetic/Logical/
                                          ReturnStack/Memory_Words.thy)
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
   Section 2: Heartbeat step axioms (A1 ×8)

   ⚠ AUDIT PROTOCOL: for each axiom below, read every code path reachable
   from vm_tick() in src/vm_time.c (including vm_tick_window_tuner,
   vm_tick_slope_validator, and any inference_engine.c callees) and verify
   that code path does NOT write to the specified field.
   ======================================================================== *)

axiomatization heartbeat_step :: "vm_state \<Rightarrow> vm_state"
  where
  heartbeat_ds   [simp]: "data_stack   (heartbeat_step vm) = data_stack vm"
  and heartbeat_rs   [simp]: "return_stack  (heartbeat_step vm) = return_stack vm"
  and heartbeat_mem  [simp]: "memory        (heartbeat_step vm) = memory vm"
  and heartbeat_dict [simp]: "dictionary    (heartbeat_step vm) = dictionary vm"
  and heartbeat_wt   [simp]: "word_table    (heartbeat_step vm) = word_table vm"
  and heartbeat_err  [simp]: "vm_error      (heartbeat_step vm) = vm_error vm"
  and heartbeat_halt [simp]: "vm_halted     (heartbeat_step vm) = vm_halted vm"
  and heartbeat_mode [simp]: "vm_mode       (heartbeat_step vm) = vm_mode vm"

(* =========================================================================
   Section 3: n-fold heartbeat field preservation (proved by induction)
   ======================================================================== *)

lemma heartbeat_n_steps_ds [simp]:
  "data_stack (heartbeat_step ^^ n $ vm) = data_stack vm"
  by (induction n) simp_all

lemma heartbeat_n_steps_rs [simp]:
  "return_stack (heartbeat_step ^^ n $ vm) = return_stack vm"
  by (induction n) simp_all

lemma heartbeat_n_steps_mem [simp]:
  "memory (heartbeat_step ^^ n $ vm) = memory vm"
  by (induction n) simp_all

lemma heartbeat_n_steps_wt [simp]:
  "word_table (heartbeat_step ^^ n $ vm) = word_table vm"
  by (induction n) simp_all

(* =========================================================================
   Section 4: Word physics transparency axiom (A4' ×1)

   This axiom states that every function in word_table depends only on the
   word-execution-relevant fields of its vm_state argument — not on any
   physics field (rolling_window, heartbeat, decay_slope_q48, etc.).

   ⚠ AUDIT PROTOCOL: for each word registered in word_table, locate its
   definition in the Level 1 theory files and confirm its body only reads:
     data_stack, return_stack, memory, word_table
   from the vm_state argument.  Level 1 words (DROP, DUP, +, AND, @, >R, …)
   satisfy this by construction; verify any new word added later.

   ○ CODE-MUST-MATCH: no word implementation in src/word_source/ may read
   rolling_window, heartbeat, decay_slope_q48, pipeline_metrics,
   last_inference, ssm_l8, tuning_lock, dict_lock, heat thresholds, or any
   physics field during normal single-word execution.                        *)
axiomatization where
  word_physics_transparent:
    "\<And> (s1 :: vm_state) (s2 :: vm_state) n.
       data_stack   s1 = data_stack   s2 \<Longrightarrow>
       return_stack s1 = return_stack s2 \<Longrightarrow>
       memory       s1 = memory       s2 \<Longrightarrow>
       word_table   s1 = word_table   s2 \<Longrightarrow>
       word_table s1 n s1 = word_table s2 n s2"

(* =========================================================================
   Section 5: Consequences of the two axiom families (all proved)
   ======================================================================== *)

(* A heartbeat before a word execution does not change the result. *)
lemma exec_after_heartbeat_eq:
  "word_table (heartbeat_step vm) n (heartbeat_step vm) = word_table vm n vm"
  by (rule word_physics_transparent) simp_all

lemma exec_after_heartbeat_ds:
  "data_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = data_stack (word_table vm n vm)"
  using exec_after_heartbeat_eq by simp

lemma exec_after_heartbeat_rs:
  "return_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = return_stack (word_table vm n vm)"
  using exec_after_heartbeat_eq by simp

(* k heartbeat steps before a word do not change the result. *)
lemma exec_after_n_heartbeats_eq:
  "word_table (heartbeat_step ^^ k $ vm) n (heartbeat_step ^^ k $ vm)
   = word_table vm n vm"
  by (rule word_physics_transparent) simp_all

(* =========================================================================
   Section 6: Single-step transition relation
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
   Section 7: Traces and reachability
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
