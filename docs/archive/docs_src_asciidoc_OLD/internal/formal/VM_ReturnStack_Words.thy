theory VM_ReturnStack_Words
  imports VM_Words
begin

section \<open>Bridging runtime helpers with abstract words\<close>

text \<open>
  The C implementations of `>R`, `R>`, and `R@` perform the same guard
  checks we model abstractly: validate stack capacity, then shuttle a
  single cell between the parameter stack, the return stack, or both.
  We capture their control flow using the runtime stack primitives from
  @{theory VM_StackRuntime} and prove the resulting functions coincide
  with the abstract word semantics from @{theory VM_Words}.
\<close>

context vm_stack_model begin

subsection \<open>Concrete-style definitions\<close>

definition vm_word_to_r_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_word_to_r_impl st = (
      if stack_depth (data_stack st) = 0 then vm_set_error True st
      else if stack_depth (return_stack st) = return_limit then vm_set_error True st
      else case vm_pop_sem st of
             (Some x, st1) \<Rightarrow> vm_rpush_sem x st1
           | (_, st1) \<Rightarrow> st1)"

definition vm_word_r_from_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_word_r_from_impl st = (
      if stack_depth (return_stack st) = 0 then vm_set_error True st
      else if stack_depth (data_stack st) = data_limit then vm_set_error True st
      else case vm_rpop_sem st of
             (Some x, st1) \<Rightarrow> vm_push_sem x st1
           | (_, st1) \<Rightarrow> st1)"

definition vm_word_r_fetch_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_word_r_fetch_impl st = (
      if stack_depth (return_stack st) = 0 then vm_set_error True st
      else if stack_depth (data_stack st) = data_limit then vm_set_error True st
      else case stack_top (return_stack st) of
             None \<Rightarrow> vm_set_error True st
           | Some x \<Rightarrow> vm_push_sem x st)
"

subsection \<open>Soundness with abstract semantics\<close>

lemma vm_word_to_r_impl_eq_word:
  "vm_word_to_r_impl state = word_to_return state"
proof (cases "stack_depth (data_stack state) = 0")
  case True
  then show ?thesis
    unfolding vm_word_to_r_impl_def by (simp add: word_to_return_error_data_empty)
next
  case False
  then have depth_pos: "stack_depth (data_stack state) > 0" by simp
  show ?thesis
  proof (cases "stack_depth (return_stack state) = return_limit")
    case True
    then show ?thesis
      unfolding vm_word_to_r_impl_def by (simp add: depth_pos word_to_return_error_return_full)
  next
    case False
    with return_stack_depth have guard_ret: "stack_depth (return_stack state) < return_limit"
      by (simp add: le_neq_implies_less)

    from pop_data_some_ex[OF depth_pos]
    obtain x st1 where pop: "pop_data state = Some (x, st1)" by blast

    from vm_pop_sem_success[OF pop]
    have pop_sem: "vm_pop_sem state = (Some x, st1)" and
         err_preserve: "vm_error_active st1 = vm_error_active state" and
         data_depth: "stack_depth (data_stack st1) = stack_depth (data_stack state) - 1"
      by simp_all

    from pop obtain ds where pop_decomp: "stack_pop (data_stack state) = Some (x, ds)"
      and st1_def: "st1 = state\<lparr> data_stack := ds \<rparr>"
      unfolding pop_data_def by (cases "stack_pop (data_stack state)") auto

    have return_stack_st1: "return_stack st1 = return_stack state"
      unfolding st1_def by simp

    from vm_rpush_sem_success[of st1 x]
      have push_sem: "vm_rpush_sem x st1 = push_return x st1"
      using guard_ret return_stack_st1 by (simp add: st1_def)

    have transfer_eval: "transfer_data_to_return state = Some (push_return x st1)"
      using pop guard_ret unfolding transfer_data_to_return_def by simp

    have word_eval: "word_to_return state = push_return x st1"
      using transfer_eval unfolding word_to_return_def by simp

    have impl_eval: "vm_word_to_r_impl state = vm_rpush_sem x st1"
      using depth_pos guard_ret pop_sem unfolding vm_word_to_r_impl_def by simp

    thus ?thesis
      using push_sem word_eval by simp
  qed
qed

lemma vm_word_r_from_impl_eq_word:
  "vm_word_r_from_impl state = word_from_return state"
proof (cases "stack_depth (return_stack state) = 0")
  case True
  then show ?thesis
    unfolding vm_word_r_from_impl_def by (simp add: word_from_return_error_return_empty)
next
  case False
  then have ret_pos: "stack_depth (return_stack state) > 0" by simp
  show ?thesis
  proof (cases "stack_depth (data_stack state) = data_limit")
    case True
    then show ?thesis
      unfolding vm_word_r_from_impl_def by (simp add: ret_pos word_from_return_error_data_full)
  next
    case False
    with stack_ok_limit_data have guard_data: "stack_depth (data_stack state) < data_limit"
      by (simp add: stack_ok_limit_def le_neq_implies_less)

    from pop_return_some_ex[OF ret_pos]
    obtain x st1 where pop: "pop_return state = Some (x, st1)" by blast

    from vm_rpop_sem_success[OF pop]
    have pop_sem: "vm_rpop_sem state = (Some x, st1)" and
         err_preserve: "vm_error_active st1 = vm_error_active state" and
         ret_depth: "stack_depth (return_stack st1) = stack_depth (return_stack state) - 1"
      by simp_all

    from pop obtain rs where pop_decomp: "stack_pop (return_stack state) = Some (x, rs)"
      and st1_def: "st1 = state\<lparr> return_stack := rs \<rparr>"
      unfolding pop_return_def by (cases "stack_pop (return_stack state)") auto

    have data_stack_st1: "data_stack st1 = data_stack state"
      unfolding st1_def by simp

    from vm_push_sem_success[of st1 x]
      have push_sem: "vm_push_sem x st1 = push_data x st1"
      using guard_data data_stack_st1 by (simp add: st1_def)

    have transfer_eval: "transfer_return_to_data state = Some (push_data x st1)"
      using pop guard_data unfolding transfer_return_to_data_def by simp

    have word_eval: "word_from_return state = push_data x st1"
      using transfer_eval unfolding word_from_return_def by simp

    have impl_eval: "vm_word_r_from_impl state = vm_push_sem x st1"
      using ret_pos guard_data pop_sem unfolding vm_word_r_from_impl_def by simp

    thus ?thesis
      using push_sem word_eval by simp
  qed
qed

lemma vm_word_r_fetch_impl_eq_word:
  "vm_word_r_fetch_impl state = word_peek_return state"
proof (cases "stack_depth (return_stack state) = 0")
  case True
  then show ?thesis
    unfolding vm_word_r_fetch_impl_def by (simp add: word_peek_return_error_empty)
next
  case False
  then have ret_pos: "stack_depth (return_stack state) > 0" by simp
  show ?thesis
  proof (cases "stack_depth (data_stack state) = data_limit")
    case True
    then show ?thesis
      unfolding vm_word_r_fetch_impl_def by (simp add: ret_pos word_peek_return_error_data_full)
  next
    case False
    with stack_ok_limit_data have guard_data: "stack_depth (data_stack state) < data_limit"
      by (simp add: stack_ok_limit_def le_neq_implies_less)

    obtain x xs where rs_decomp: "return_stack state = x # xs"
      using ret_pos unfolding stack_depth_def by (cases "return_stack state") auto

    have top_eval: "stack_top (return_stack state) = Some x"
      using rs_decomp unfolding stack_top_def by simp

    from vm_push_sem_success[of state x]
      have push_sem: "vm_push_sem x state = push_data x state"
      using guard_data by simp

    have word_eval: "word_peek_return state = push_data x state"
      using top_eval guard_data unfolding word_peek_return_def peek_return_to_data_def by simp

    have impl_eval: "vm_word_r_fetch_impl state = vm_push_sem x state"
      using ret_pos guard_data top_eval unfolding vm_word_r_fetch_impl_def by simp

    thus ?thesis
      using push_sem word_eval by simp
  qed
qed

end

end

