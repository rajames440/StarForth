theory StarForth_Transition
  imports StarForth_Mutex
begin

(* =========================================================================
   SKETCH: StarForth_Transition — Labeled Transition System

   This is a PROOF SKETCH.  The definitions model the intended concurrency
   architecture; several sub-lemmas are marked sorry or axiom.

   SPECIFICATION ROLE: This theory is the ground truth for how the two
   threads (ExecThread, HeartbeatThread) interact.  C code must be written
   so that every execution of vm_tick() satisfies the heartbeat_step axioms.

   HOW TO USE THIS SKETCH:
     1. Read the heartbeat_step axioms (Section 2) carefully — each axiom is
        a human-verifiable claim about src/vm_time.c:vm_tick().
     2. When the C heartbeat implementation changes, re-audit each axiom.
     3. The non-interference theorems in StarForth_Concurrent.thy follow
        from these axioms alone — so the axioms are the sole load-bearing
        assumptions for the concurrency layer.
   ======================================================================== *)

(* =========================================================================
   Section 1: Thread and action datatypes
   ======================================================================== *)

datatype thread = ExecThread | HeartbeatThread

(* Actions performed by each thread. *)
datatype action =
    ExecWord nat             \<comment> \<open>ExecThread executes word with given word_id\<close>
  | HeartTick                \<comment> \<open>HeartbeatThread fires one tick (may run all loops)\<close>
  | AcquireTuning nat        \<comment> \<open>thread nat acquires tuning_lock\<close>
  | ReleaseTuning nat        \<comment> \<open>thread nat releases tuning_lock\<close>
  | AcquireDict nat          \<comment> \<open>thread nat acquires dict_lock\<close>
  | ReleaseDict nat          \<comment> \<open>thread nat releases dict_lock\<close>

type_synonym event = "thread \<times> action"

(* =========================================================================
   Section 2: Heartbeat step axioms
   ── ⚠ HUMAN REVIEW REQUIRED FOR EVERY AXIOM BELOW ─────────────────────────

   Each axiom says: "executing vm_tick() (one heartbeat tick) does NOT change
   this field."  These are LOAD-BEARING ASSUMPTIONS.  The non-interference
   theorem (StarForth_Concurrent.heartbeat_noninterference) is a mathematical
   consequence of these axioms — but the axioms themselves are not proved in
   Isabelle; they must be verified by reading src/vm_time.c.

   AUDIT PROTOCOL for each axiom:
     □ Find every code path reachable from vm_tick() in src/vm_time.c.
     □ Verify that code path does NOT write to the specified field.
     □ Include vm_tick_window_tuner(), vm_tick_slope_validator(), and
       any inference_engine.c calls triggered from vm_tick().
     □ Sign off when complete; record git commit of src/vm_time.c audited.

   IF ANY AXIOM IS VIOLATED (heartbeat_step touches a "preserved" field):
     The non-interference proof breaks and word correctness cannot be claimed
     independent of heartbeat timing.  Fix the C code, then remove or modify
     the affected axiom and re-derive the consequences.
   ─────────────────────────────────────────────────────────────────────── *)

axiomatization heartbeat_step :: "vm_state \<Rightarrow> vm_state"
  where
  (* ⚠ AUDIT: vm_tick() must never write to vm->data_stack[] or vm->dsp.
     Heartbeat functions have no reason to touch the data stack.            *)
  heartbeat_ds   [simp]: "data_stack   (heartbeat_step vm) = data_stack vm"

  (* ⚠ AUDIT: vm_tick() must never write to vm->return_stack[] or vm->rsp. *)
  and heartbeat_rs   [simp]: "return_stack  (heartbeat_step vm) = return_stack vm"

  (* ⚠ AUDIT: vm_tick() must never write to vm->memory[] or vm->memory_size.
     The heartbeat thread does not allocate or access user memory.          *)
  and heartbeat_mem  [simp]: "memory        (heartbeat_step vm) = memory vm"

  (* ⚠ AUDIT: vm_tick() must never write any DictEntry or vm->here/latest_id.
     Dictionary writes happen only under dict_lock held by ExecThread.
     Exception: dictionary heat is updated by the decay sweep — but heat is
     part of dict_entry, not the dictionary map itself.
     ⚠ SKETCH NOTE: This axiom may need revision if the heartbeat decay sweep
     is found to modify dictionary structure (not just DictEntry.heat).      *)
  and heartbeat_dict [simp]: "dictionary    (heartbeat_step vm) = dictionary vm"

  (* ⚠ AUDIT: vm_tick() must never modify the word_table.
     Word semantics are fixed at startup; heartbeat cannot redefine words.  *)
  and heartbeat_wt   [simp]: "word_table    (heartbeat_step vm) = word_table vm"

  (* ⚠ AUDIT: vm_tick() must not set vm->error.  A heartbeat tick that
     encounters an internal error should handle it internally (e.g., skip the
     problematic word in the decay sweep) and NOT propagate it to vm_error.  *)
  and heartbeat_err  [simp]: "vm_error      (heartbeat_step vm) = vm_error vm"

  (* ⚠ AUDIT: vm_tick() must not set vm->halted.  Only BYE / fatal C-level
     errors should set vm_halted.                                            *)
  and heartbeat_halt [simp]: "vm_halted     (heartbeat_step vm) = vm_halted vm"

  (* ⚠ AUDIT: vm_tick() must not change vm->vm_mode (interpret / compile).
     If SSM L8 mode changes affect vm_mode, this axiom is violated.
     ⚠ SKETCH NOTE: Verify that SSM mode changes go through ssm_l8 only, not
     the top-level vm_mode field.                                            *)
  and heartbeat_mode [simp]: "vm_mode       (heartbeat_step vm) = vm_mode vm"

(* =========================================================================
   Section 3: Single-step relation
   ======================================================================== *)

inductive vm_step :: "vm_state \<Rightarrow> event \<Rightarrow> vm_state \<Rightarrow> bool"
  ("_ \<rightarrow>[_] _" [60, 0, 61] 60)
where
  (* ExecThread executes word_id n by looking up word_table vm n
     ○ CODE-MUST-MATCH: C interpreter dispatch in src/vm.c must call
       exactly the function pointer indexed by word_id.                     *)
  StepExec:
    "\<not> vm_error vm \<Longrightarrow> \<not> vm_halted vm \<Longrightarrow>
     vm' = word_table vm n vm \<Longrightarrow>
     vm \<rightarrow>[(ExecThread, ExecWord n)] vm'"

  (* HeartbeatThread fires a tick
     ○ CODE-MUST-MATCH: C: pthread heartbeat thread calls vm_tick(vm).      *)
| StepHeartTick:
    "\<not> vm_halted vm \<Longrightarrow>
     vm' = heartbeat_step vm \<Longrightarrow>
     vm \<rightarrow>[(HeartbeatThread, HeartTick)] vm'"

  (* Lock transitions — see StarForth_Mutex.thy for safety proofs.         *)
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
   Section 4: Traces and reachability
   ======================================================================== *)

type_synonym trace = "(vm_state \<times> event \<times> vm_state) list"

fun valid_trace :: "trace \<Rightarrow> bool" where
  "valid_trace [] = True"
| "valid_trace [(s, e, s')] = (s \<rightarrow>[e] s')"
| "valid_trace ((s, e, s') # rest) =
     (s \<rightarrow>[e] s' \<and> (case rest of [] \<Rightarrow> True | (s0, _, _) # _ \<Rightarrow> s0 = s') \<and> valid_trace rest)"

definition reachable :: "vm_state \<Rightarrow> vm_state \<Rightarrow> bool" where
  "reachable s0 s' \<longleftrightarrow>
     s0 = s' \<or>
     (\<exists>tr. valid_trace tr \<and> tr \<noteq> [] \<and>
           (fst (hd tr)) = s0 \<and>
           (snd (snd (last tr))) = s')"

(* =========================================================================
   Section 5: Exec-thread projection
   ======================================================================== *)

definition exec_events :: "trace \<Rightarrow> (vm_state \<times> nat \<times> vm_state) list" where
  "exec_events tr =
     [(s, n, s'). (s, (ExecThread, ExecWord n), s') \<leftarrow> tr]"

definition same_exec_trace :: "trace \<Rightarrow> trace \<Rightarrow> bool" where
  "same_exec_trace tr1 tr2 \<longleftrightarrow> exec_events tr1 = exec_events tr2"

(* =========================================================================
   Section 6: Immediate consequences of heartbeat axioms

   These follow by simp from the axioms; no human review needed here.
   They are used in StarForth_Concurrent.thy non-interference proofs.       *)

lemma exec_after_heartbeat_ds:
  "data_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = data_stack (word_table vm n vm)"
  by simp

lemma exec_after_heartbeat_rs:
  "return_stack (word_table (heartbeat_step vm) n (heartbeat_step vm))
   = return_stack (word_table vm n vm)"
  by simp

end
