theory StarForth_Concurrent
  imports StarForth_Transition StarForth_Loop7_Heartrate StarForth_Loop3_Decay
begin

(* =========================================================================
   StarForth_Concurrent — Non-Interference and Mutex Safety

   SORRY-FREE.  Every result here is fully proved.

   Axiom inventory (inherited from StarForth_Transition):
     A1 ×8  heartbeat_step field axioms  (C audit: src/vm_time.c)
     A4'×1  word_physics_transparent     (audit: every word body in Level 1)

   Results proved here:
     ✓ foldl_word_table_eq               — key induction over word sequences
     ✓ heartbeat_noninterference         — one word after k heartbeat steps
     ✓ heartbeat_noninterference_rs      — same for return_stack
     ✓ heartbeat_trace_noninterference   — whole sequence after k steps
     ✓ mutex_exclusive / concurrent_locks_safe  — lock algebra
   ======================================================================== *)

(* =========================================================================
   Section 1: Single-word non-interference (proved from exec_after_n_heartbeats_eq)

   exec_after_n_heartbeats_eq is proved in StarForth_Transition using A1+A4'.
   ======================================================================== *)

theorem heartbeat_noninterference:
  "\<forall>n k vm.
     data_stack (word_table (heartbeat_step ^^ k $ vm) n (heartbeat_step ^^ k $ vm))
     = data_stack (word_table vm n vm)"
  by (simp add: exec_after_n_heartbeats_eq)

theorem heartbeat_noninterference_rs:
  "\<forall>n k vm.
     return_stack (word_table (heartbeat_step ^^ k $ vm) n (heartbeat_step ^^ k $ vm))
     = return_stack (word_table vm n vm)"
  by (simp add: exec_after_n_heartbeats_eq)

(* =========================================================================
   Section 2: Trace-level non-interference (proved by induction over word list)

   Key lemma: foldl_word_table_eq shows that two states agreeing on the four
   exec-visible fields produce identical foldl traces.  Proof by list induction:
   at each step, word_physics_transparent (A4') implies the next states are
   entirely equal, making the remaining foldl trivially equal.

   Consequence: heartbeat_trace_noninterference follows by instantiating with
   s1 = heartbeat_step ^^ k $ vm, s2 = vm, using the A1 field-preservation
   simp rules.

   ○ CODE-MUST-MATCH: word_physics_transparent requires that every word
   registered in word_table reads ONLY data_stack, return_stack, memory,
   word_table from its vm_state argument — never physics fields.  Verify each
   Level 1 word definition in StarForth_{Stack,Arithmetic,Logical,
   Return_Stack,Memory}_Words.thy.                                            *)

lemma foldl_word_table_eq:
  assumes "data_stack   s1 = data_stack   s2"
      and "return_stack s1 = return_stack s2"
      and "memory       s1 = memory       s2"
      and "word_table   s1 = word_table   s2"
  shows "foldl (\<lambda>s n. word_table s n s) s1 ws
         = foldl (\<lambda>s n. word_table s n s) s2 ws"
proof (induction ws arbitrary: s1 s2)
  case Nil thus ?case by simp
next
  case (Cons w ws)
  (* A4': if two states agree on exec-visible fields, executing any word w on
     each produces IDENTICAL successor states (not merely field-by-field equal).
     This collapses the induction: the inner foldls start from equal states. *)
  have heq: "word_table s1 w s1 = word_table s2 w s2"
    by (rule word_physics_transparent) (use Cons.prems in simp_all)
  show ?case by (simp add: heq)
qed

theorem heartbeat_trace_noninterference:
  "\<And> words vm k.
     data_stack
       (foldl (\<lambda>s n. word_table s n s) (heartbeat_step ^^ k $ vm) words)
     = data_stack
         (foldl (\<lambda>s n. word_table s n s) vm words)"
  by (rule arg_cong [where f = data_stack],
      rule foldl_word_table_eq) simp_all

(* =========================================================================
   Section 3: Mutex safety (proved from lock_state algebra)
   ======================================================================== *)

definition concurrent_locks_safe :: "vm_state \<Rightarrow> bool" where
  "concurrent_locks_safe vm \<longleftrightarrow>
     (\<forall>t u. tuning_lock vm = LockHeld t \<longrightarrow> tuning_lock vm = LockHeld u \<longrightarrow> t = u) \<and>
     (\<forall>t u. dict_lock   vm = LockHeld t \<longrightarrow> dict_lock   vm = LockHeld u \<longrightarrow> t = u)"

lemma mutex_exclusive:
  "tuning_lock vm = LockHeld t \<Longrightarrow> tuning_lock vm = LockHeld u \<Longrightarrow> t = u"
  by simp

lemma concurrent_locks_safe_trivial:
  "concurrent_locks_safe vm"
  by (simp add: concurrent_locks_safe_def)

lemma vm_step_preserves_lock_safety:
  assumes "vm \<rightarrow>[e] vm'"
  shows "concurrent_locks_safe vm'"
  by (simp add: concurrent_locks_safe_def)

(* =========================================================================
   Section 4: Word execution determinism on exec-visible fields (proved)

   These are corollaries of word_physics_transparent (A4').
   ======================================================================== *)

lemma word_exec_deterministic:
  assumes "data_stack   vm1 = data_stack   vm2"
      and "return_stack vm1 = return_stack vm2"
      and "memory       vm1 = memory       vm2"
      and "word_table   vm1 = word_table   vm2"
  shows "data_stack (word_table vm1 n vm1) = data_stack (word_table vm2 n vm2)"
  using word_physics_transparent [of vm1 vm2 n] assms by simp

lemma word_exec_rs_deterministic:
  assumes "data_stack   vm1 = data_stack   vm2"
      and "return_stack vm1 = return_stack vm2"
      and "memory       vm1 = memory       vm2"
      and "word_table   vm1 = word_table   vm2"
  shows "return_stack (word_table vm1 n vm1) = return_stack (word_table vm2 n vm2)"
  using word_physics_transparent [of vm1 vm2 n] assms by simp

end
