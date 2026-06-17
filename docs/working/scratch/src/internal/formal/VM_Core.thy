theory VM_Core
  imports Main
begin

section \<open>VM core abstraction skeleton\<close>

text \<open>
  This stub establishes the high-level locales that future proofs will
  refine as we formalise the StarForth VM. The goal is to let upcoming
  modules snap additional invariants into place without rewriting the
  observation and ring-buffer foundations.
\<close>

record vm_control_state =
  error_flag :: bool
  halted_flag :: bool
  abort_flag :: bool
  exit_flag :: bool

record ('dict,'stack,'io,'phys) vm_core_state =
  dict_state :: 'dict
  data_stack :: 'stack
  return_stack :: 'stack
  io_context :: 'io
  physics_snapshot :: 'phys
  control_state :: vm_control_state

locale vm_core_base =
  fixes state :: "('dict,'stack,'io,'phys) vm_core_state"
  fixes stack_ok :: "'stack \<Rightarrow> bool"
  assumes data_stack_wf: "stack_ok (data_stack state)"
begin

text \<open>
  The stack predicate captured here will be instantiated by concrete
  semantics (bounded depth, pointer discipline, etc.). Future
  theories can extend this locale with additional assumes/defines while
  keeping downstream lemmas stable.
\<close>

end

subsection \<open>Pointer accounting\<close>

text \<open>
  The runtime keeps integer stack pointers (`dsp`, `rsp`) that shadow the
  logical element counts maintained by the abstract stack model.  The
  following locale packages the basic arithmetic facts we rely on when
  translating between those perspectives.  Concrete theories specialise
  the parameters to actual stack depths (`stack_depth`) and limits.
\<close>

locale vm_pointer_model =
  fixes data_limit return_limit :: nat
  fixes data_count return_count :: nat
  fixes dsp rsp :: int
  assumes data_ptr_rel: "int data_count = dsp + 1"
      and return_ptr_rel: "int return_count = rsp + 1"
      and data_within_limit: "data_count \<le> data_limit"
      and return_within_limit: "return_count \<le> return_limit"
begin

lemma dsp_underflow_iff:
  "dsp < 0 \<longleftrightarrow> data_count = 0"
proof
  assume "dsp < 0"
  then have "dsp \<le> -1" by linarith
  hence "dsp + 1 \<le> 0" by simp
  hence "int data_count \<le> 0" using data_ptr_rel by simp
  thus "data_count = 0" by simp
next
  assume "data_count = 0"
  then have "int data_count = 0" by simp
  hence "dsp + 1 = 0"
    using data_ptr_rel by simp
  thus "dsp < 0" by simp
qed

lemma rsp_underflow_iff:
  "rsp < 0 \<longleftrightarrow> return_count = 0"
  using return_ptr_rel by (cases "return_count") auto

lemma dsp_nonneg_iff:
  "dsp \<ge> 0 \<longleftrightarrow> data_count > 0"
proof
  assume "dsp \<ge> 0"
  then have "dsp + 1 \<ge> 1"
    by simp
  hence "int data_count \<ge> 1"
    using data_ptr_rel by simp
  then show "data_count > 0"
    by simp
next
  assume "data_count > 0"
  then obtain n where "data_count = Suc n" by (cases data_count) auto
  hence "int data_count = int n + 1"
    by simp
  hence "dsp + 1 = int n + 1"
    using data_ptr_rel by simp
  thus "dsp \<ge> 0"
    by simp
qed

lemma rsp_nonneg_iff:
  "rsp \<ge> 0 \<longleftrightarrow> return_count > 0"
  using return_ptr_rel by (cases "return_count") auto

lemma data_guard_ptr_iff:
  "data_count < data_limit \<longleftrightarrow> dsp + 1 < int data_limit"
  using data_ptr_rel by simp

lemma return_guard_ptr_iff:
  "return_count < return_limit \<longleftrightarrow> rsp + 1 < int return_limit"
  using return_ptr_rel by simp

lemma data_limit_int:
  "int data_count \<le> int data_limit"
  using data_within_limit by simp

lemma return_limit_int:
  "int return_count \<le> int return_limit"
  using return_within_limit by simp

end

subsection \<open>Control flag bookkeeping\<close>

text \<open>
  The C implementation tracks coarse VM state via integer flags such as
  `vm->error` and `vm->halted`.  We model those as booleans inside the
  `vm_control_state` record and provide small helpers that update the
  control portion without touching the stacks or dictionary model.
\<close>

definition vm_update_control :: "(vm_control_state \<Rightarrow> vm_control_state) \<Rightarrow> ('d,'s,'i,'p) vm_core_state \<Rightarrow> ('d,'s,'i,'p) vm_core_state" where
  "vm_update_control f st = st\<lparr> control_state := f (control_state st) \<rparr>"

definition vm_set_error :: "bool \<Rightarrow> ('d,'s,'i,'p) vm_core_state \<Rightarrow> ('d,'s,'i,'p) vm_core_state" where
  "vm_set_error flag = vm_update_control (\<lambda>c. c\<lparr> error_flag := flag \<rparr>)"

definition vm_set_halted :: "bool \<Rightarrow> ('d,'s,'i,'p) vm_core_state \<Rightarrow> ('d,'s,'i,'p) vm_core_state" where
  "vm_set_halted flag = vm_update_control (\<lambda>c. c\<lparr> halted_flag := flag \<rparr>)"

definition vm_set_abort :: "bool \<Rightarrow> ('d,'s,'i,'p) vm_core_state \<Rightarrow> ('d,'s,'i,'p) vm_core_state" where
  "vm_set_abort flag = vm_update_control (\<lambda>c. c\<lparr> abort_flag := flag \<rparr>)"

definition vm_set_exit :: "bool \<Rightarrow> ('d,'s,'i,'p) vm_core_state \<Rightarrow> ('d,'s,'i,'p) vm_core_state" where
  "vm_set_exit flag = vm_update_control (\<lambda>c. c\<lparr> exit_flag := flag \<rparr>)"

definition vm_error_active :: "('d,'s,'i,'p) vm_core_state \<Rightarrow> bool" where
  "vm_error_active st \<longleftrightarrow> error_flag (control_state st)"

definition vm_halted :: "('d,'s,'i,'p) vm_core_state \<Rightarrow> bool" where
  "vm_halted st \<longleftrightarrow> halted_flag (control_state st)"

definition vm_abort_pending :: "('d,'s,'i,'p) vm_core_state \<Rightarrow> bool" where
  "vm_abort_pending st \<longleftrightarrow> abort_flag (control_state st)"

definition vm_exit_pending :: "('d,'s,'i,'p) vm_core_state \<Rightarrow> bool" where
  "vm_exit_pending st \<longleftrightarrow> exit_flag (control_state st)"

lemma vm_update_control_preserve_stacks [simp]:
  "data_stack (vm_update_control f st) = data_stack st"
  "return_stack (vm_update_control f st) = return_stack st"
  "dict_state (vm_update_control f st) = dict_state st"
  "io_context (vm_update_control f st) = io_context st"
  "physics_snapshot (vm_update_control f st) = physics_snapshot st"
  unfolding vm_update_control_def by simp_all

lemma vm_error_after_set [simp]:
  "vm_error_active (vm_set_error flag st) = flag"
  unfolding vm_set_error_def vm_update_control_def vm_error_active_def by simp

lemma vm_halted_after_set [simp]:
  "vm_halted (vm_set_halted flag st) = flag"
  unfolding vm_set_halted_def vm_update_control_def vm_halted_def by simp

lemma vm_abort_after_set [simp]:
  "vm_abort_pending (vm_set_abort flag st) = flag"
  unfolding vm_set_abort_def vm_update_control_def vm_abort_pending_def by simp

lemma vm_exit_after_set [simp]:
  "vm_exit_pending (vm_set_exit flag st) = flag"
  unfolding vm_set_exit_def vm_update_control_def vm_exit_pending_def by simp

lemma vm_error_preserve_stacks:
  "data_stack (vm_set_error flag st) = data_stack st"
  "return_stack (vm_set_error flag st) = return_stack st"
  "control_state (vm_set_error flag st) = control_state st\<lparr>error_flag := flag\<rparr>"
  unfolding vm_set_error_def vm_update_control_def by simp_all

lemma vm_abort_clear_effect [simp]:
  "vm_abort_pending (vm_set_abort False st) = False"
  by simp

lemma vm_exit_clear_effect [simp]:
  "vm_exit_pending (vm_set_exit False st) = False"
  by simp

text \<open>
  Invariants about how the flags interact with control flow can be layered on
  later.  These helpers simply provide a uniform way to talk about the flags in
  the Isabelle models while leaving stack and dictionary components untouched.
\<close>

end
