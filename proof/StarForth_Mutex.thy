theory StarForth_Mutex
  imports StarForth_Base
begin

(* =========================================================================
   StarForth_Mutex — Mutex / lock-state model

   Mirrors: sf_mutex_t logic protecting tuning_lock and dict_lock in the VM.

   The VM has two locks:
     tuning_lock  — guards physics-loop parameter updates (decay slope, window)
     dict_lock    — guards dictionary writes (word definitions, SMUDGE, REVEAL)

   Modelled as first-class vm_state fields of type lock_state
   (defined in StarForth_Base as: LockFree | LockHeld nat).

   Safety property: mutual exclusion — at most one thread holds a lock.
   Since StarForth has exactly two threads (exec + heartbeat), thread IDs
   are 0 (ExecThread) and 1 (HeartbeatThread).
   ======================================================================== *)

(* =========================================================================
   Section 1: Thread identifiers
   ======================================================================== *)

definition EXEC_THREAD      :: nat where "EXEC_THREAD      = 0"
definition HEARTBEAT_THREAD :: nat where "HEARTBEAT_THREAD = 1"

(* All valid thread IDs *)
definition valid_thread :: "nat \<Rightarrow> bool" where
  "valid_thread t \<longleftrightarrow> t = EXEC_THREAD \<or> t = HEARTBEAT_THREAD"

lemma threads_distinct: "EXEC_THREAD \<noteq> HEARTBEAT_THREAD"
  by (simp add: EXEC_THREAD_def HEARTBEAT_THREAD_def)

(* =========================================================================
   Section 2: Lock operations
   ======================================================================== *)

(* acquire_lock t s: thread t acquires lock in state s.
   Precondition: lock is free (s = LockFree).
   Result: lock held by t.                                                    *)
definition acquire_lock :: "nat \<Rightarrow> lock_state \<Rightarrow> lock_state" where
  "acquire_lock t s = (case s of LockFree \<Rightarrow> LockHeld t | LockHeld h \<Rightarrow> LockHeld h)"

(* release_lock t s: thread t releases lock in state s.
   Precondition: lock held by t (s = LockHeld t).
   Result: lock free.                                                          *)
definition release_lock :: "nat \<Rightarrow> lock_state \<Rightarrow> lock_state" where
  "release_lock t s = (case s of LockFree \<Rightarrow> LockFree | LockHeld h \<Rightarrow> (if h = t then LockFree else LockHeld h))"

(* is_free, is_held predicates *)
definition lock_free :: "lock_state \<Rightarrow> bool" where
  "lock_free s \<longleftrightarrow> s = LockFree"

definition lock_held_by :: "nat \<Rightarrow> lock_state \<Rightarrow> bool" where
  "lock_held_by t s \<longleftrightarrow> s = LockHeld t"

(* =========================================================================
   Section 3: Basic lock lemmas
   ======================================================================== *)

lemma acquire_when_free:
  "acquire_lock t LockFree = LockHeld t"
  by (simp add: acquire_lock_def)

lemma acquire_when_held_noop:
  "acquire_lock t (LockHeld h) = LockHeld h"
  by (simp add: acquire_lock_def)

lemma release_own_lock:
  "release_lock t (LockHeld t) = LockFree"
  by (simp add: release_lock_def)

lemma release_other_lock:
  assumes "h \<noteq> t"
  shows "release_lock t (LockHeld h) = LockHeld h"
  by (simp add: release_lock_def assms)

lemma release_free_noop:
  "release_lock t LockFree = LockFree"
  by (simp add: release_lock_def)

lemma acquire_then_release:
  "release_lock t (acquire_lock t LockFree) = LockFree"
  by (simp add: acquire_lock_def release_lock_def)

(* =========================================================================
   Section 4: Mutual exclusion — at most one holder
   ======================================================================== *)

(* A lock is in a valid state: either free or held by exactly one thread.     *)
definition lock_wf :: "lock_state \<Rightarrow> bool" where
  "lock_wf s \<longleftrightarrow> (case s of LockFree \<Rightarrow> True | LockHeld t \<Rightarrow> valid_thread t)"

lemma lock_wf_free [simp]: "lock_wf LockFree"
  by (simp add: lock_wf_def)

lemma lock_wf_held:
  assumes "valid_thread t"
  shows "lock_wf (LockHeld t)"
  by (simp add: lock_wf_def assms)

lemma acquire_preserves_wf:
  assumes "lock_wf s" "valid_thread t"
  shows "lock_wf (acquire_lock t s)"
  by (cases s; simp add: acquire_lock_def lock_wf_def assms)

lemma release_preserves_wf:
  assumes "lock_wf s"
  shows "lock_wf (release_lock t s)"
  by (cases s; simp add: release_lock_def lock_wf_def assms)

(* Two distinct threads cannot both hold the same lock. *)
lemma mutex_exclusive:
  assumes "lock_held_by t s" "lock_held_by u s"
  shows "t = u"
  by (cases s; simp add: lock_held_by_def assms; blast)

(* =========================================================================
   Section 5: VM-level lock accessors
   ======================================================================== *)

(* Acquire/release tuning_lock in the full vm_state record. *)
definition vm_acquire_tuning :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "vm_acquire_tuning t vm = vm\<lparr>tuning_lock := acquire_lock t (tuning_lock vm)\<rparr>"

definition vm_release_tuning :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "vm_release_tuning t vm = vm\<lparr>tuning_lock := release_lock t (tuning_lock vm)\<rparr>"

(* Acquire/release dict_lock in the full vm_state record. *)
definition vm_acquire_dict :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "vm_acquire_dict t vm = vm\<lparr>dict_lock := acquire_lock t (dict_lock vm)\<rparr>"

definition vm_release_dict :: "nat \<Rightarrow> vm_state \<Rightarrow> vm_state" where
  "vm_release_dict t vm = vm\<lparr>dict_lock := release_lock t (dict_lock vm)\<rparr>"

(* Acquiring/releasing a lock does not change the data or return stacks. *)
lemma acquire_tuning_ds [simp]:
  "data_stack (vm_acquire_tuning t vm) = data_stack vm"
  by (simp add: vm_acquire_tuning_def)

lemma acquire_tuning_rs [simp]:
  "return_stack (vm_acquire_tuning t vm) = return_stack vm"
  by (simp add: vm_acquire_tuning_def)

lemma release_tuning_ds [simp]:
  "data_stack (vm_release_tuning t vm) = data_stack vm"
  by (simp add: vm_release_tuning_def)

lemma release_tuning_rs [simp]:
  "return_stack (vm_release_tuning t vm) = return_stack vm"
  by (simp add: vm_release_tuning_def)

lemma acquire_dict_ds [simp]:
  "data_stack (vm_acquire_dict t vm) = data_stack vm"
  by (simp add: vm_acquire_dict_def)

lemma acquire_dict_rs [simp]:
  "return_stack (vm_acquire_dict t vm) = return_stack vm"
  by (simp add: vm_acquire_dict_def)

lemma release_dict_ds [simp]:
  "data_stack (vm_release_dict t vm) = data_stack vm"
  by (simp add: vm_release_dict_def)

lemma release_dict_rs [simp]:
  "return_stack (vm_release_dict t vm) = return_stack vm"
  by (simp add: vm_release_dict_def)

(* Acquire-then-release restores the tuning_lock to its original value
   (assuming the lock was free before acquisition). *)
lemma tuning_lock_round_trip:
  assumes "tuning_lock vm = LockFree"
  shows "tuning_lock (vm_release_tuning t (vm_acquire_tuning t vm)) = LockFree"
  by (simp add: vm_acquire_tuning_def vm_release_tuning_def acquire_lock_def release_lock_def assms)

(* =========================================================================
   Section 6: Well-formedness of VM lock state
   ======================================================================== *)

definition vm_locks_wf :: "vm_state \<Rightarrow> bool" where
  "vm_locks_wf vm \<longleftrightarrow>
     lock_wf (tuning_lock vm) \<and>
     lock_wf (dict_lock vm)"

lemma vm_locks_wf_initial:
  assumes "tuning_lock vm = LockFree"
  assumes "dict_lock vm = LockFree"
  shows "vm_locks_wf vm"
  by (simp add: vm_locks_wf_def assms)

end
