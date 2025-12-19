theory VM_StackRuntime
  imports VM_Stacks
begin

section \<open>Runtime-facing stack operations\<close>

text \<open>
  This theory bridges the pure stack model with the concrete VM runtime
  helpers (`vm_push`, `vm_pop`, `vm_rpush`, `vm_rpop`).  Guard failures
  raise the VM error flag while leaving stacks unchanged, matching the
  C implementation.
\<close>

subsection \<open>Abstract wrappers\<close>

context vm_stack_model begin

definition vm_push_sem :: "'val \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_push_sem x st = (
     if stack_depth (data_stack st) < data_limit
     then push_data x st
     else vm_set_error True st)"

definition vm_pop_sem :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('val option \<times> ('dict,'val stack,'io,'phys) vm_core_state)" where
  "vm_pop_sem st = (
     case pop_data st of
       None \<Rightarrow> (None, vm_set_error True st)
     | Some (x, st') \<Rightarrow> (Some x, st'))"

definition vm_rpush_sem :: "'val \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_rpush_sem x st = (
     if stack_depth (return_stack st) < return_limit
     then push_return x st
     else vm_set_error True st)"

definition vm_rpop_sem :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('val option \<times> ('dict,'val stack,'io,'phys) vm_core_state)" where
  "vm_rpop_sem st = (
     case pop_return st of
       None \<Rightarrow> (None, vm_set_error True st)
     | Some (x, st') \<Rightarrow> (Some x, st'))"

lemma vm_error_active_push_data [simp]:
  "vm_error_active (push_data x st) = vm_error_active st"
  unfolding push_data_def vm_error_active_def by simp

lemma vm_error_active_pop_data_state:
  assumes "pop_data st = Some (x, st')"
  shows "vm_error_active st' = vm_error_active st"
  using assms unfolding pop_data_def vm_error_active_def by (cases "stack_pop (data_stack st)") auto

lemma vm_error_active_push_return [simp]:
  "vm_error_active (push_return x st) = vm_error_active st"
  unfolding push_return_def vm_error_active_def by simp

lemma vm_error_active_pop_return_state:
  assumes "pop_return st = Some (x, st')"
  shows "vm_error_active st' = vm_error_active st"
  using assms unfolding pop_return_def vm_error_active_def by (cases "stack_pop (return_stack st)") auto

lemma vm_push_sem_success:
  assumes guard: "stack_depth (data_stack st) < data_limit"
  shows "vm_push_sem x st = push_data x st"
    and "vm_error_active (vm_push_sem x st) = vm_error_active st"
    and "stack_depth (data_stack (vm_push_sem x st)) = Suc (stack_depth (data_stack st))"
proof -
  have "vm_push_sem x st = push_data x st"
    using guard unfolding vm_push_sem_def by simp
  moreover have "vm_error_active (vm_push_sem x st) = vm_error_active st"
    using calculation by simp
  moreover have "stack_depth (data_stack (vm_push_sem x st)) = Suc (stack_depth (data_stack st))"
    using guard calculation unfolding push_data_def stack_depth_def stack_push_def by simp
  ultimately show
    "vm_push_sem x st = push_data x st"
    "vm_error_active (vm_push_sem x st) = vm_error_active st"
    "stack_depth (data_stack (vm_push_sem x st)) = Suc (stack_depth (data_stack st))" by simp_all
qed

lemma vm_push_sem_error:
  assumes "\<not> stack_depth (data_stack st) < data_limit"
  shows "vm_push_sem x st = vm_set_error True st"
    and "vm_error_active (vm_push_sem x st)"
    and "data_stack (vm_push_sem x st) = data_stack st"
proof -
  have A: "vm_push_sem x st = vm_set_error True st"
    using assms unfolding vm_push_sem_def by simp
  moreover have "vm_error_active (vm_set_error True st)"
    by simp
  moreover have "data_stack (vm_set_error True st) = data_stack st"
    by simp
  ultimately show
    "vm_push_sem x st = vm_set_error True st"
    "vm_error_active (vm_push_sem x st)"
    "data_stack (vm_push_sem x st) = data_stack st" by simp_all
qed

lemma vm_pop_sem_success:
  assumes pop: "pop_data st = Some (x, st')"
  shows "vm_pop_sem st = (Some x, st')"
    and "vm_error_active (snd (vm_pop_sem st)) = vm_error_active st"
    and "stack_depth (data_stack st') = stack_depth (data_stack st) - 1"
proof -
  have "vm_pop_sem st = (Some x, st')"
    using pop unfolding vm_pop_sem_def by simp
  moreover have "vm_error_active st' = vm_error_active st"
    using vm_error_active_pop_data_state[OF pop]
      unfolding vm_pop_sem_def pop by simp
  moreover have "stack_depth (data_stack st') = stack_depth (data_stack st) - 1"
    using pop unfolding pop_data_def stack_pop_def stack_depth_def by (cases "stack_pop (data_stack st)") auto
  ultimately show
    "vm_pop_sem st = (Some x, st')"
    "vm_error_active (snd (vm_pop_sem st)) = vm_error_active st"
    "stack_depth (data_stack st') = stack_depth (data_stack st) - 1" by simp_all
qed

lemma vm_pop_sem_underflow:
  assumes "stack_depth (data_stack st) = 0"
  shows "vm_pop_sem st = (None, vm_set_error True st)"
    and "vm_error_active (snd (vm_pop_sem st))"
    and "data_stack (snd (vm_pop_sem st)) = data_stack st"
proof -
  have "stack_pop (data_stack st) = None"
    using assms unfolding stack_depth_def stack_pop_def by auto
  hence "pop_data st = None"
    unfolding pop_data_def by simp
  thus "vm_pop_sem st = (None, vm_set_error True st)"
    unfolding vm_pop_sem_def by simp
  moreover have "vm_error_active (vm_set_error True st)"
    by simp
  moreover have "data_stack (vm_set_error True st) = data_stack st"
    by simp
  ultimately show
    "vm_error_active (snd (vm_pop_sem st))"
    "data_stack (snd (vm_pop_sem st)) = data_stack st" by simp_all
qed

lemma vm_rpush_sem_success:
  assumes guard: "stack_depth (return_stack st) < return_limit"
  shows "vm_rpush_sem x st = push_return x st"
    and "vm_error_active (vm_rpush_sem x st) = vm_error_active st"
    and "stack_depth (return_stack (vm_rpush_sem x st)) = Suc (stack_depth (return_stack st))"
proof -
  have "vm_rpush_sem x st = push_return x st"
    using guard unfolding vm_rpush_sem_def by simp
  moreover have "vm_error_active (vm_rpush_sem x st) = vm_error_active st"
    using calculation by simp
  moreover have "stack_depth (return_stack (vm_rpush_sem x st)) = Suc (stack_depth (return_stack st))"
    using guard calculation unfolding push_return_def stack_depth_def stack_push_def by simp
  ultimately show ?thesis by simp_all
qed

lemma vm_rpush_sem_error:
  assumes "\<not> stack_depth (return_stack st) < return_limit"
  shows "vm_rpush_sem x st = vm_set_error True st"
    and "vm_error_active (vm_rpush_sem x st)"
    and "return_stack (vm_rpush_sem x st) = return_stack st"
proof -
  have A: "vm_rpush_sem x st = vm_set_error True st"
    using assms unfolding vm_rpush_sem_def by simp
  moreover have "vm_error_active (vm_set_error True st)"
    by simp
  moreover have "return_stack (vm_set_error True st) = return_stack st"
    by simp
  ultimately show ?thesis by simp_all
qed

lemma vm_rpop_sem_success:
  assumes pop: "pop_return st = Some (x, st')"
  shows "vm_rpop_sem st = (Some x, st')"
    and "vm_error_active (snd (vm_rpop_sem st)) = vm_error_active st"
    and "stack_depth (return_stack st') = stack_depth (return_stack st) - 1"
proof -
  have "vm_rpop_sem st = (Some x, st')"
    using pop unfolding vm_rpop_sem_def by simp
  moreover have "vm_error_active st' = vm_error_active st"
    using vm_error_active_pop_return_state[OF pop]
      unfolding vm_rpop_sem_def pop by simp
  moreover have "stack_depth (return_stack st') = stack_depth (return_stack st) - 1"
    using pop unfolding pop_return_def stack_pop_def stack_depth_def by (cases "stack_pop (return_stack st)") auto
  ultimately show ?thesis by simp_all
qed

lemma vm_rpop_sem_underflow:
  assumes "stack_depth (return_stack st) = 0"
  shows "vm_rpop_sem st = (None, vm_set_error True st)"
    and "vm_error_active (snd (vm_rpop_sem st))"
    and "return_stack (snd (vm_rpop_sem st)) = return_stack st"
proof -
  have "stack_pop (return_stack st) = None"
    using assms unfolding stack_depth_def stack_pop_def by auto
  hence "pop_return st = None"
    unfolding pop_return_def by simp
  thus "vm_rpop_sem st = (None, vm_set_error True st)"
    unfolding vm_rpop_sem_def by simp
  moreover have "vm_error_active (vm_set_error True st)"
    by simp
  moreover have "return_stack (vm_set_error True st) = return_stack st"
    by simp
  ultimately show
    "vm_error_active (snd (vm_rpop_sem st))"
    "return_stack (snd (vm_rpop_sem st)) = return_stack st" by simp_all
qed

end

subsection \<open>Runtime locale\<close>

locale vm_stack_runtime =
  vm_stack_model state data_limit return_limit data_underflow_ok return_underflow_ok cell_to_int int_to_cell
  for state :: "('dict,'val stack,'io,'phys) vm_core_state"
    and data_limit :: nat
    and return_limit :: nat
    and data_underflow_ok :: bool
    and return_underflow_ok :: bool
    and cell_to_int :: "'val \<Rightarrow> int"
    and int_to_cell :: "int \<Rightarrow> 'val" +
  fixes dsp rsp :: int
  assumes data_ptr_rel: "int (stack_depth (data_stack state)) = dsp + 1"
      and return_ptr_rel: "int (stack_depth (return_stack state)) = rsp + 1"

context vm_stack_runtime begin

sublocale pointer: vm_pointer_model data_limit return_limit
    "stack_depth (data_stack state)" "stack_depth (return_stack state)" dsp rsp
  using data_ptr_rel return_ptr_rel data_stack_depth return_stack_depth
  by unfold_locales simp_all

lemma dsp_underflow_iff:
  "dsp < 0 \<longleftrightarrow> stack_depth (data_stack state) = 0"
  by (simp add: pointer.dsp_underflow_iff)

lemma rsp_underflow_iff:
  "rsp < 0 \<longleftrightarrow> stack_depth (return_stack state) = 0"
  by (simp add: pointer.rsp_underflow_iff)

lemma dsp_nonneg_iff:
  "dsp \<ge> 0 \<longleftrightarrow> stack_depth (data_stack state) > 0"
  by (simp add: pointer.dsp_nonneg_iff)

lemma rsp_nonneg_iff:
  "rsp \<ge> 0 \<longleftrightarrow> stack_depth (return_stack state) > 0"
  by (simp add: pointer.rsp_nonneg_iff)

lemma data_guard_ptr_iff:
  "stack_depth (data_stack state) < data_limit \<longleftrightarrow> dsp + 1 < int data_limit"
  by (simp add: pointer.data_guard_ptr_iff)

lemma return_guard_ptr_iff:
  "stack_depth (return_stack state) < return_limit \<longleftrightarrow> rsp + 1 < int return_limit"
  by (simp add: pointer.return_guard_ptr_iff)

lemma vm_push_sem_runtime_success:
  assumes guard: "dsp + 1 < int data_limit"
  shows "vm_push_sem x state = push_data x state"
    and "vm_error_active (vm_push_sem x state) = vm_error_active state"
    and "int (stack_depth (data_stack (vm_push_sem x state))) = dsp + 2"
proof -
  have depth_guard: "stack_depth (data_stack state) < data_limit"
    using data_guard_ptr_iff guard by simp
  from vm_push_sem_success[OF depth_guard] have push_eq:
    "vm_push_sem x state = push_data x state"
    and err_eq: "vm_error_active (vm_push_sem x state) = vm_error_active state"
    and depth_suc: "stack_depth (data_stack (vm_push_sem x state)) = Suc (stack_depth (data_stack state))"
    by simp_all
  have depth_int: "int (stack_depth (data_stack (vm_push_sem x state))) = dsp + 2"
  proof -
    have "int (stack_depth (data_stack (vm_push_sem x state))) = int (Suc (stack_depth (data_stack state)))"
      using depth_suc by simp
    also have "... = int (stack_depth (data_stack state)) + 1" by simp
    also have "... = (dsp + 1) + 1"
      using data_ptr_rel by simp
    finally show ?thesis by simp
  qed
  show
    "vm_push_sem x state = push_data x state"
    "vm_error_active (vm_push_sem x state) = vm_error_active state"
    "int (stack_depth (data_stack (vm_push_sem x state))) = dsp + 2"
    using push_eq err_eq depth_int by simp_all
qed

lemma vm_push_sem_runtime_overflow:
  assumes guard_fail: "dsp + 1 \<ge> int data_limit"
  shows "vm_push_sem x state = vm_set_error True state"
    and "vm_error_active (vm_push_sem x state)"
    and "data_stack (vm_push_sem x state) = data_stack state"
proof -
  have depth_fail: "\<not> stack_depth (data_stack state) < data_limit"
    using data_guard_ptr_iff guard_fail by auto
  from vm_push_sem_error[OF depth_fail] show
    "vm_push_sem x state = vm_set_error True state"
    "vm_error_active (vm_push_sem x state)"
    "data_stack (vm_push_sem x state) = data_stack state" by simp_all
qed

lemma vm_pop_sem_runtime_success:
  assumes nonempty: "dsp \<ge> 0"
  obtains x st' where
      "vm_pop_sem state = (Some x, st')"
      "vm_error_active st' = vm_error_active state"
      "int (stack_depth (data_stack st')) = dsp"
proof -
  have depth_pos: "stack_depth (data_stack state) > 0"
    using dsp_nonneg_iff nonempty by simp
  from pop_data_some_ex[OF depth_pos] obtain x st1 where pop: "pop_data state = Some (x, st1)" by blast
  from vm_pop_sem_success[OF pop] have sem_eq:
    "vm_pop_sem state = (Some x, st1)"
    and err_eq: "vm_error_active st1 = vm_error_active state"
    and depth_rel: "stack_depth (data_stack st1) = stack_depth (data_stack state) - 1"
    by simp_all
  have depth_int: "int (stack_depth (data_stack st1)) = dsp"
  proof -
    obtain n where n_def: "stack_depth (data_stack state) = Suc n"
      using depth_pos by (cases "stack_depth (data_stack state)") auto
    then have "stack_depth (data_stack st1) = n"
      using depth_rel by simp
    hence "int (stack_depth (data_stack st1)) = int n" by simp
    also from n_def have "int n = int (stack_depth (data_stack state)) - 1" by simp
    also have "... = (dsp + 1) - 1" using data_ptr_rel by simp
    finally show ?thesis by simp
  qed
  show ?thesis
    using sem_eq err_eq depth_int by (intro that)
qed

lemma vm_pop_sem_runtime_underflow:
  assumes "dsp < 0"
  shows "vm_pop_sem state = (None, vm_set_error True state)"
    and "vm_error_active (snd (vm_pop_sem state))"
    and "data_stack (snd (vm_pop_sem state)) = data_stack state"
proof -
  have depth0: "stack_depth (data_stack state) = 0"
    using assms dsp_underflow_iff by simp
  from vm_pop_sem_underflow[OF depth0] show
    "vm_pop_sem state = (None, vm_set_error True state)"
    "vm_error_active (snd (vm_pop_sem state))"
    "data_stack (snd (vm_pop_sem state)) = data_stack state" by simp_all
qed

lemma vm_rpush_sem_runtime_success:
  assumes guard: "rsp + 1 < int return_limit"
  shows "vm_rpush_sem x state = push_return x state"
    and "vm_error_active (vm_rpush_sem x state) = vm_error_active state"
    and "int (stack_depth (return_stack (vm_rpush_sem x state))) = rsp + 2"
proof -
  have depth_guard: "stack_depth (return_stack state) < return_limit"
    using return_guard_ptr_iff guard by simp
  from vm_rpush_sem_success[OF depth_guard] have push_eq:
    "vm_rpush_sem x state = push_return x state"
    and err_eq: "vm_error_active (vm_rpush_sem x state) = vm_error_active state"
    and depth_suc: "stack_depth (return_stack (vm_rpush_sem x state)) = Suc (stack_depth (return_stack state))"
    by simp_all
  have depth_int: "int (stack_depth (return_stack (vm_rpush_sem x state))) = rsp + 2"
  proof -
    have "int (stack_depth (return_stack (vm_rpush_sem x state))) = int (Suc (stack_depth (return_stack state)))"
      using depth_suc by simp
    also have "... = int (stack_depth (return_stack state)) + 1" by simp
    also have "... = (rsp + 1) + 1"
      using return_ptr_rel by simp
    finally show ?thesis by simp
  qed
  show ?thesis
    using push_eq err_eq depth_int by simp_all
qed

lemma vm_rpush_sem_runtime_overflow:
  assumes guard_fail: "rsp + 1 \<ge> int return_limit"
  shows "vm_rpush_sem x state = vm_set_error True state"
    and "vm_error_active (vm_rpush_sem x state)"
    and "return_stack (vm_rpush_sem x state) = return_stack state"
proof -
  have depth_fail: "\<not> stack_depth (return_stack state) < return_limit"
    using return_guard_ptr_iff guard_fail by auto
  from vm_rpush_sem_error[OF depth_fail] show ?thesis by simp_all
qed

lemma vm_rpop_sem_runtime_success:
  assumes nonempty: "rsp \<ge> 0"
  obtains x st' where
      "vm_rpop_sem state = (Some x, st')"
      "vm_error_active st' = vm_error_active state"
      "int (stack_depth (return_stack st')) = rsp"
proof -
  have depth_pos: "stack_depth (return_stack state) > 0"
    using rsp_nonneg_iff nonempty by simp
  from pop_return_some_ex[OF depth_pos] obtain x st1 where pop: "pop_return state = Some (x, st1)" by blast
  from vm_rpop_sem_success[OF pop] have sem_eq:
    "vm_rpop_sem state = (Some x, st1)"
    and err_eq: "vm_error_active st1 = vm_error_active state"
    and depth_rel: "stack_depth (return_stack st1) = stack_depth (return_stack state) - 1"
    by simp_all
  have depth_int: "int (stack_depth (return_stack st1)) = rsp"
  proof -
    obtain n where n_def: "stack_depth (return_stack state) = Suc n"
      using depth_pos by (cases "stack_depth (return_stack state)") auto
    then have "stack_depth (return_stack st1) = n"
      using depth_rel by simp
    hence "int (stack_depth (return_stack st1)) = int n" by simp
    also from n_def have "int n = int (stack_depth (return_stack state)) - 1" by simp
    also have "... = (rsp + 1) - 1"
      using return_ptr_rel by simp
    finally show ?thesis by simp
  qed
  show ?thesis
    using sem_eq err_eq depth_int by (intro that)
qed

lemma vm_rpop_sem_runtime_underflow:
  assumes "rsp < 0"
  shows "vm_rpop_sem state = (None, vm_set_error True state)"
    and "vm_error_active (snd (vm_rpop_sem state))"
    and "return_stack (snd (vm_rpop_sem state)) = return_stack state"
proof -
  have depth0: "stack_depth (return_stack state) = 0"
    using assms rsp_underflow_iff by simp
  from vm_rpop_sem_underflow[OF depth0] show
    "vm_rpop_sem state = (None, vm_set_error True state)"
    "vm_error_active (snd (vm_rpop_sem state))"
    "return_stack (snd (vm_rpop_sem state)) = return_stack state" by simp_all
qed

end

end
