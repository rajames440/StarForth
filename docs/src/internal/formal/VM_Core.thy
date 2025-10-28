theory VM_Core
  imports Physics_Observation
begin

section \<open>VM core abstraction skeleton\<close>

text \<open>
  This stub establishes the high-level locales that future proofs will
  refine as we formalise the StarForth VM. The goal is to let upcoming
  modules snap additional invariants into place without rewriting the
  observation and ring-buffer foundations.
\<close>

record ('dict,'stack,'io) vm_core_state =
  dict_state :: 'dict
  data_stack :: 'stack
  return_stack :: 'stack
  io_context :: 'io
  physics_snapshot :: word_physics

locale vm_core_base =
  fixes state :: "('dict,'stack,'io) vm_core_state"
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

end
