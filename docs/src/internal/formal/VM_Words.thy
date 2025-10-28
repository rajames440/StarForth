theory VM_Words
  imports Physics_Formal.VM_Stacks
begin

section \<open>Abstract Forth word semantics\<close>

text \<open>
  This theory wraps the stack transfer helpers from @{theory Physics_Formal.VM_Stacks}
  into simple, total word semantics representing `>R` (data→return) and
  `R>` (return→data). We also provide a lightweight locale that plugs
  the abstract stack depths into the runtime integer pointers (`dsp`,
  `rsp`) so interpreter proofs can reason directly about the C
  implementation.
\<close>

context vm_stack_model begin

definition word_to_return :: "('dict,'val stack,'io) vm_core_state \<Rightarrow> ('dict,'val stack,'io) vm_core_state" where
  "word_to_return st = (case transfer_data_to_return st of None \<Rightarrow> st | Some st' \<Rightarrow> st')"

definition word_from_return :: "('dict,'val stack,'io) vm_core_state \<Rightarrow> ('dict,'val stack,'io) vm_core_state" where
  "word_from_return st = (case transfer_return_to_data st of None \<Rightarrow> st | Some st' \<Rightarrow> st')"

definition word_peek_return :: "('dict,'val stack,'io) vm_core_state \<Rightarrow> ('dict,'val stack,'io) vm_core_state" where
  "word_peek_return st = (case peek_return_to_data st of None \<Rightarrow> st | Some st' \<Rightarrow> st')"

lemma word_to_return_success:
  assumes depth: "stack_depth (data_stack state) > 0"
      and space: "stack_depth (return_stack state) < return_limit"
  obtains st' where
      "word_to_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "stack_depth (data_stack st') = stack_depth (data_stack state) - 1"
      "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))"
proof -
  from transfer_data_to_return_ok[OF depth space]
  obtain st' where td:
    "transfer_data_to_return state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (data_stack st') = stack_depth (data_stack state) - 1"
    "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))" by blast
  show ?thesis
  proof
    show "word_to_return state = st'"
      unfolding word_to_return_def td(1) by simp
    show "stack_ok_limit (data_stack st')" by (rule td(2))
    show "stack_ok_limit_return (return_stack st')" by (rule td(3))
    show "stack_depth (data_stack st') = stack_depth (data_stack state) - 1" by (rule td(4))
    show "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))" by (rule td(5))
  qed
qed

lemma word_from_return_success:
  assumes depth: "stack_depth (return_stack state) > 0"
      and space: "stack_depth (data_stack state) < data_limit"
  obtains st' where
      "word_from_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "stack_depth (return_stack st') = stack_depth (return_stack state) - 1"
      "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
proof -
  from transfer_return_to_data_ok[OF depth space]
  obtain st' where td:
    "transfer_return_to_data state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state) - 1"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by blast
  show ?thesis
  proof
    show "word_from_return state = st'"
      unfolding word_from_return_def td(1) by simp
    show "stack_ok_limit (data_stack st')" by (rule td(2))
    show "stack_ok_limit_return (return_stack st')" by (rule td(3))
    show "stack_depth (return_stack st') = stack_depth (return_stack state) - 1" by (rule td(4))
    show "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by (rule td(5))
  qed
qed

lemma word_peek_return_success:
  assumes depth: "stack_depth (return_stack state) > 0"
      and space: "stack_depth (data_stack state) < data_limit"
  obtains st' where
      "word_peek_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "stack_depth (return_stack st') = stack_depth (return_stack state)"
      "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
proof -
  from peek_return_to_data_ok[OF depth space]
  obtain st' where pr:
    "peek_return_to_data state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state)"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by blast
  show ?thesis
  proof
    show "word_peek_return state = st'"
      unfolding word_peek_return_def pr(1) by simp
    show "stack_ok_limit (data_stack st')" by (rule pr(2))
    show "stack_ok_limit_return (return_stack st')" by (rule pr(3))
    show "stack_depth (return_stack st') = stack_depth (return_stack state)" by (rule pr(4))
    show "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by (rule pr(5))
  qed
qed

lemma word_to_return_noop_empty:
  assumes "stack_depth (data_stack state) = 0"
  shows "word_to_return state = state"
proof -
  have "stack_pop (data_stack state) = None"
    using assms unfolding stack_pop_def stack_depth_def by auto
  hence "pop_data state = None"
    unfolding pop_data_def by simp
  thus ?thesis
    unfolding word_to_return_def transfer_data_to_return_def by simp
qed

lemma word_to_return_noop_return_full:
  assumes depth: "stack_depth (data_stack state) > 0"
      and full: "stack_depth (return_stack state) = return_limit"
  shows "word_to_return state = state"
proof -
  obtain x st1 where "pop_data state = Some (x, st1)"
    using pop_data_some_ex[OF depth] by blast
  moreover have "\<not> (stack_depth (return_stack state) < return_limit)"
    using full by simp
  ultimately show ?thesis
    unfolding word_to_return_def transfer_data_to_return_def by simp
qed

lemma word_from_return_noop_empty:
  assumes "stack_depth (return_stack state) = 0"
  shows "word_from_return state = state"
proof -
  have "stack_pop (return_stack state) = None"
    using assms unfolding stack_pop_def stack_depth_def by auto
  hence "pop_return state = None"
    unfolding pop_return_def by simp
  thus ?thesis
    unfolding word_from_return_def transfer_return_to_data_def by simp
qed

lemma word_from_return_noop_data_full:
  assumes depth: "stack_depth (return_stack state) > 0"
      and full: "stack_depth (data_stack state) = data_limit"
  shows "word_from_return state = state"
proof -
  obtain x st1 where "pop_return state = Some (x, st1)"
    using pop_return_some_ex[OF depth] by blast
  moreover have "\<not> (stack_depth (data_stack state) < data_limit)"
    using full by simp
  ultimately show ?thesis
    unfolding word_from_return_def transfer_return_to_data_def by simp
qed

lemma word_peek_return_noop_empty:
  assumes "stack_depth (return_stack state) = 0"
  shows "word_peek_return state = state"
proof -
  have "stack_top (return_stack state) = None"
    using assms unfolding stack_top_def stack_depth_def by (cases "return_stack state") auto
  thus ?thesis
    unfolding word_peek_return_def peek_return_to_data_def by simp
qed

lemma word_peek_return_noop_data_full:
  assumes depth: "stack_depth (return_stack state) > 0"
      and full: "stack_depth (data_stack state) = data_limit"
  shows "word_peek_return state = state"
proof -
  obtain x xs where ret_decomp: "return_stack state = x # xs"
  proof (cases "return_stack state")
    case Nil
    then show ?thesis using depth unfolding stack_depth_def by simp
  next
    case (Cons x xs)
    then show ?thesis by blast
  qed
  have top_eq: "stack_top (return_stack state) = Some x"
    unfolding ret_decomp stack_top_def by simp
  have guard_fail: "\<not> (stack_depth (data_stack state) < data_limit)"
    using full by simp
  thus ?thesis
    unfolding word_peek_return_def peek_return_to_data_def top_eq by simp
qed

end

locale vm_stack_runtime =
  vm_stack_model state data_limit return_limit data_underflow_ok return_underflow_ok
  for state :: "('dict,'val stack,'io) vm_core_state"
    and data_limit :: nat
    and return_limit :: nat
    and data_underflow_ok :: bool
    and return_underflow_ok :: bool +
  fixes dsp rsp :: int
  assumes data_ptr_rel: "int (stack_depth (data_stack state)) = dsp + 1"
      and return_ptr_rel: "int (stack_depth (return_stack state)) = rsp + 1"

context vm_stack_runtime begin

lemma dsp_underflow_iff:
  "dsp < 0 \<longleftrightarrow> stack_depth (data_stack state) = 0"
  using data_ptr_rel by auto

lemma rsp_underflow_iff:
  "rsp < 0 \<longleftrightarrow> stack_depth (return_stack state) = 0"
  using return_ptr_rel by auto

lemma return_guard_ptr_iff:
  "stack_depth (return_stack state) < return_limit \<longleftrightarrow> rsp + 1 < int return_limit"
  using return_ptr_rel by simp

lemma data_guard_ptr_iff:
  "stack_depth (data_stack state) < data_limit \<longleftrightarrow> dsp + 1 < int data_limit"
  using data_ptr_rel by simp

lemma word_to_return_runtime_success:
  assumes "dsp \<ge> 0" "rsp + 1 < int return_limit"
  obtains st' where
      "word_to_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "int (stack_depth (data_stack st')) = dsp"
      "int (stack_depth (return_stack st')) = rsp + 2"
proof -
  have depth_pos: "stack_depth (data_stack state) > 0"
    using data_ptr_rel assms(1) by simp
  have space_guard: "stack_depth (return_stack state) < return_limit"
    using return_ptr_rel assms(2) by simp
  from word_to_return_success[OF depth_pos space_guard]
  obtain st' where st:
    "word_to_return state = st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (data_stack st') = stack_depth (data_stack state) - 1"
    "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))" by blast
  obtain n where depth_repr: "stack_depth (data_stack state) = Suc n"
    using depth_pos by (cases "stack_depth (data_stack state)") auto
  have data_depth': "int (stack_depth (data_stack st')) = dsp"
    using st(4) data_ptr_rel depth_repr by simp
  have ret_depth': "int (stack_depth (return_stack st')) = rsp + 2"
    using st(5) return_ptr_rel by simp
  show ?thesis
  proof
    show "word_to_return state = st'" by fact
    show "stack_ok_limit (data_stack st')" by fact
    show "stack_ok_limit_return (return_stack st')" by fact
    show "int (stack_depth (data_stack st')) = dsp" by (rule data_depth')
    show "int (stack_depth (return_stack st')) = rsp + 2" by (rule ret_depth')
  qed
qed

lemma word_to_return_runtime_underflow:
  assumes "dsp < 0"
  shows "word_to_return state = state"
  using word_to_return_noop_empty dsp_underflow_iff assms by simp

lemma word_to_return_runtime_overflow:
  assumes "dsp \<ge> 0" "rsp + 1 \<ge> int return_limit"
  shows "word_to_return state = state"
proof (cases "dsp < 0")
  case True
  then show ?thesis using word_to_return_runtime_underflow by simp
next
  case False
  have guard_fail: "\<not> (stack_depth (return_stack state) < return_limit)"
    using return_ptr_rel assms(2) by auto
  have depth_pos: "stack_depth (data_stack state) > 0"
    using data_ptr_rel assms(1) by simp
  obtain x st1 where pop: "pop_data state = Some (x, st1)"
    using pop_data_some_ex[OF depth_pos] by blast
  hence "transfer_data_to_return state = None"
    unfolding transfer_data_to_return_def using guard_fail pop by simp
  thus ?thesis
    unfolding word_to_return_def by simp
qed

lemma word_from_return_runtime_success:
  assumes "rsp \<ge> 0" "dsp + 1 < int data_limit"
  obtains st' where
      "word_from_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "int (stack_depth (return_stack st')) = rsp"
      "int (stack_depth (data_stack st')) = dsp + 2"
proof -
  have ret_pos: "stack_depth (return_stack state) > 0"
    using return_ptr_rel assms(1) by simp
  have data_guard: "stack_depth (data_stack state) < data_limit"
    using data_ptr_rel assms(2) by simp
  from word_from_return_success[OF ret_pos data_guard]
  obtain st' where st:
    "word_from_return state = st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state) - 1"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by blast
  obtain n where ret_repr: "stack_depth (return_stack state) = Suc n"
    using ret_pos by (cases "stack_depth (return_stack state)") auto
  have ret_depth': "int (stack_depth (return_stack st')) = rsp"
    using st(4) return_ptr_rel ret_repr by simp
  have data_depth': "int (stack_depth (data_stack st')) = dsp + 2"
    using st(5) data_ptr_rel by simp
  show ?thesis
  proof
    show "word_from_return state = st'" by fact
    show "stack_ok_limit (data_stack st')" by fact
    show "stack_ok_limit_return (return_stack st')" by fact
    show "int (stack_depth (return_stack st')) = rsp" by (rule ret_depth')
    show "int (stack_depth (data_stack st')) = dsp + 2" by (rule data_depth')
  qed
qed

lemma word_from_return_runtime_underflow:
  assumes "rsp < 0"
  shows "word_from_return state = state"
  using word_from_return_noop_empty rsp_underflow_iff assms by simp

lemma word_from_return_runtime_overflow:
  assumes "rsp \<ge> 0" "dsp + 1 \<ge> int data_limit"
  shows "word_from_return state = state"
proof (cases "rsp < 0")
  case True
  then show ?thesis using word_from_return_runtime_underflow by simp
next
  case False
  have guard_fail: "\<not> (stack_depth (data_stack state) < data_limit)"
    using data_ptr_rel assms(2) by auto
  have ret_pos: "stack_depth (return_stack state) > 0"
    using return_ptr_rel assms(1) by simp
  obtain x st1 where pop: "pop_return state = Some (x, st1)"
    using pop_return_some_ex[OF ret_pos] by blast
  hence "transfer_return_to_data state = None"
    unfolding transfer_return_to_data_def using guard_fail pop by simp
  thus ?thesis
    unfolding word_from_return_def by simp
qed

lemma word_peek_return_runtime_success:
  assumes "rsp \<ge> 0" "dsp + 1 < int data_limit"
  obtains st' where
      "word_peek_return state = st'"
      "stack_ok_limit (data_stack st')"
      "stack_ok_limit_return (return_stack st')"
      "int (stack_depth (return_stack st')) = rsp + 1"
      "int (stack_depth (data_stack st')) = dsp + 2"
proof -
  have ret_pos: "stack_depth (return_stack state) > 0"
    using return_ptr_rel assms(1) by simp
  have data_guard: "stack_depth (data_stack state) < data_limit"
    using data_ptr_rel assms(2) by simp
  from word_peek_return_success[OF ret_pos data_guard]
  obtain st' where st:
    "word_peek_return state = st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state)"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by blast
  have ret_depth': "int (stack_depth (return_stack st')) = rsp + 1"
    using st(4) return_ptr_rel by simp
  have data_depth': "int (stack_depth (data_stack st')) = dsp + 2"
    using st(5) data_ptr_rel by simp
  show ?thesis
  proof
    show "word_peek_return state = st'" by fact
    show "stack_ok_limit (data_stack st')" by fact
    show "stack_ok_limit_return (return_stack st')" by fact
    show "int (stack_depth (return_stack st')) = rsp + 1" by (rule ret_depth')
    show "int (stack_depth (data_stack st')) = dsp + 2" by (rule data_depth')
  qed
qed

lemma word_peek_return_runtime_underflow:
  assumes "rsp < 0"
  shows "word_peek_return state = state"
  using word_peek_return_noop_empty rsp_underflow_iff assms by simp

lemma word_peek_return_runtime_overflow:
  assumes "rsp \<ge> 0" "dsp + 1 \<ge> int data_limit"
  shows "word_peek_return state = state"
proof -
  have guard_fail: "\<not> (stack_depth (data_stack state) < data_limit)"
    using data_ptr_rel assms(2) by auto
  have ret_pos: "stack_depth (return_stack state) > 0"
    using return_ptr_rel assms(1) by simp
  obtain x xs where ret_decomp: "return_stack state = x # xs"
  proof (cases "return_stack state")
    case Nil
    then show ?thesis using ret_pos unfolding stack_depth_def by simp
  next
    case (Cons x xs)
    then show ?thesis by blast
  qed
  have top_eq: "stack_top (return_stack state) = Some x"
    unfolding ret_decomp stack_top_def by simp
  show ?thesis
    unfolding word_peek_return_def peek_return_to_data_def top_eq using guard_fail by simp
qed

end
end
