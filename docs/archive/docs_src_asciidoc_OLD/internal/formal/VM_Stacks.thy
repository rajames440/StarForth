theory VM_Stacks
  imports VM_Core
begin

section \<open>Stack model scaffolding\<close>

text \<open>
  This theory introduces a lightweight list-based model for StarForth
  stacks. It is intentionally conservative so later work can refine the
  representation (e.g., pointer-aware arrays) without disrupting the
  core locales.
\<close>

type_synonym 'a stack = "'a list"

definition stack_depth :: "'a stack \<Rightarrow> nat" where
  "stack_depth s = length s"

definition stack_top :: "'a stack \<Rightarrow> 'a option" where
  "stack_top s = (case s of [] \<Rightarrow> None | x # _ \<Rightarrow> Some x)"

definition stack_push :: "'a \<Rightarrow> 'a stack \<Rightarrow> 'a stack" where
  "stack_push x s = x # s"

definition stack_pop :: "'a stack \<Rightarrow> ('a * 'a stack) option" where
  "stack_pop s = (case s of [] \<Rightarrow> None | x # xs \<Rightarrow> Some (x, xs))"

definition stack_trim :: "nat \<Rightarrow> 'a stack \<Rightarrow> 'a stack" where
  "stack_trim n s = take n s"

definition push_data :: "'val \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "push_data x st = st\<lparr> data_stack := stack_push x (data_stack st) \<rparr>"

definition pop_data :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> (('val * ('dict,'val stack,'io,'phys) vm_core_state)) option" where
  "pop_data st = (case stack_pop (data_stack st) of None \<Rightarrow> None | Some (x, s) \<Rightarrow> Some (x, st\<lparr> data_stack := s \<rparr>))"

definition push_return :: "'val \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "push_return x st = st\<lparr> return_stack := stack_push x (return_stack st) \<rparr>"

definition pop_return :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> (('val * ('dict,'val stack,'io,'phys) vm_core_state)) option" where
  "pop_return st = (case stack_pop (return_stack st) of None \<Rightarrow> None | Some (x, s) \<Rightarrow> Some (x, st\<lparr> return_stack := s \<rparr>))"


locale vm_stack_model =
  fixes state :: "('dict,'val stack,'io,'phys) vm_core_state"
  fixes data_limit :: nat
  fixes return_limit :: nat
  fixes data_underflow_ok :: bool
  fixes return_underflow_ok :: bool
  fixes cell_to_int :: "'val \<Rightarrow> int"
  fixes int_to_cell :: "int \<Rightarrow> 'val"
  assumes data_stack_depth: "stack_depth (data_stack state) \<le> data_limit"
      and return_stack_depth: "stack_depth (return_stack state) \<le> return_limit"
      and data_underflow_guard: "data_underflow_ok \<longleftrightarrow> data_stack state = []"
      and return_underflow_guard: "return_underflow_ok \<longleftrightarrow> return_stack state = []"
      and cell_roundtrip: "int_to_cell (cell_to_int v) = v"
      and cell_encode_decode: "cell_to_int (int_to_cell i) = i"
begin

definition stack_ok_limit :: "'val stack \<Rightarrow> bool" where
  "stack_ok_limit s \<longleftrightarrow> stack_depth s \<le> data_limit"

definition transfer_data_to_return :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state option" where
  "transfer_data_to_return st =
     (case pop_data st of
        None \<Rightarrow> None
      | Some (x, st1) \<Rightarrow>
          if stack_depth (return_stack st) < return_limit
          then Some (push_return x st1)
          else None)"

definition transfer_return_to_data :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state option" where
  "transfer_return_to_data st =
     (case pop_return st of
        None \<Rightarrow> None
      | Some (x, st1) \<Rightarrow>
          if stack_depth (data_stack st) < data_limit
          then Some (push_data x st1)
          else None)"

definition peek_return_to_data :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state option" where
  "peek_return_to_data st =
     (case stack_top (return_stack st) of
        None \<Rightarrow> None
      | Some x \<Rightarrow>
          if stack_depth (data_stack st) < data_limit
          then Some (push_data x st)
          else None)"

lemma stack_ok_limit_data:
  "stack_ok_limit (data_stack state)"
  using data_stack_depth unfolding stack_ok_limit_def by simp

definition stack_ok_limit_return :: "'val stack \<Rightarrow> bool" where
  "stack_ok_limit_return s \<longleftrightarrow> stack_depth s \<le> return_limit"

lemma stack_ok_limit_return_state:
  "stack_ok_limit_return (return_stack state)"
  using return_stack_depth unfolding stack_ok_limit_return_def by simp

definition return_stack_ok :: "'val stack \<Rightarrow> bool" where
  "return_stack_ok s \<longleftrightarrow> stack_depth s \<le> return_limit"

lemma return_stack_ok_state:
  "return_stack_ok (return_stack state)"
  using return_stack_depth unfolding return_stack_ok_def by simp

definition data_underflow_safe :: bool where
  "data_underflow_safe \<longleftrightarrow> (stack_depth (data_stack state) = 0 \<longrightarrow> data_underflow_ok)"

definition return_underflow_safe :: bool where
  "return_underflow_safe \<longleftrightarrow> (stack_depth (return_stack state) = 0 \<longrightarrow> return_underflow_ok)"

lemma data_underflow_implies_flag:
  assumes "stack_depth (data_stack state) = 0"
  shows "data_underflow_ok"
  using data_underflow_guard assms unfolding stack_depth_def by (cases "data_stack state") auto

lemma return_underflow_implies_flag:
  assumes "stack_depth (return_stack state) = 0"
  shows "return_underflow_ok"
  using return_underflow_guard assms unfolding stack_depth_def by (cases "return_stack state") auto

lemma data_underflow_safeI: "data_underflow_safe"
  unfolding data_underflow_safe_def using data_underflow_implies_flag by blast

lemma return_underflow_safeI: "return_underflow_safe"
  unfolding return_underflow_safe_def using return_underflow_implies_flag by blast

lemma stack_push_within_limit:
  assumes "stack_depth s < data_limit"
  shows "stack_depth (stack_push x s) \<le> data_limit"
  using assms unfolding stack_depth_def stack_push_def by simp

lemma stack_pop_underflow_guard:
  assumes "stack_pop (data_stack state) = Some (x, s')"
  shows "\<not> data_underflow_ok"
  using assms data_underflow_guard unfolding stack_pop_def stack_depth_def by (cases "data_stack state") auto

lemma stack_pop_preserves_limit:
  assumes "stack_pop (data_stack state) = Some (x, s')"
  shows "stack_depth s' \<le> data_limit"
  using assms data_stack_depth unfolding stack_pop_def stack_depth_def by (cases "data_stack state") auto

lemma return_stack_pop_underflow_guard:
  assumes "stack_pop (return_stack state) = Some (x, s')"
  shows "\<not> return_underflow_ok"
  using assms return_underflow_guard unfolding stack_pop_def stack_depth_def by (cases "return_stack state") auto

lemma return_stack_pop_preserves_limit:
  assumes "stack_pop (return_stack state) = Some (x, s')"
  shows "stack_depth s' \<le> return_limit"
  using assms return_stack_depth unfolding stack_pop_def stack_depth_def by (cases "return_stack state") auto

lemma push_data_respects_limit:
  assumes "stack_depth (data_stack state) < data_limit"
  shows "stack_depth (data_stack (push_data x state)) \<le> data_limit"
  using assms unfolding push_data_def stack_depth_def stack_push_def by simp

lemma push_return_respects_limit:
  assumes "stack_depth (return_stack state) < return_limit"
  shows "stack_depth (return_stack (push_return x state)) \<le> return_limit"
  using assms unfolding push_return_def stack_depth_def stack_push_def by simp

lemma push_data_preserves_ok:
  assumes "stack_depth (data_stack state) < data_limit"
  shows "stack_ok_limit (data_stack (push_data x state))"
  using push_data_respects_limit[OF assms] unfolding stack_ok_limit_def by simp

lemma push_return_preserves_ok:
  assumes "stack_depth (return_stack state) < return_limit"
  shows "stack_ok_limit_return (return_stack (push_return x state))"
  using push_return_respects_limit[OF assms] unfolding stack_ok_limit_return_def by simp

lemma pop_data_None_iff_underflow:
  "pop_data state = None \<longleftrightarrow> data_underflow_ok"
  unfolding pop_data_def stack_pop_def using data_underflow_guard by (cases "data_stack state") auto

lemma pop_return_None_iff_underflow:
  "pop_return state = None \<longleftrightarrow> return_underflow_ok"
  unfolding pop_return_def stack_pop_def using return_underflow_guard by (cases "return_stack state") auto

lemma pop_data_some_ex:
  assumes "stack_depth (data_stack state) > 0"
  shows "\<exists>x st'. pop_data state = Some (x, st')"
proof (cases "pop_data state")
  case None
  then have "data_underflow_ok" using pop_data_None_iff_underflow by simp
  moreover have "data_stack state = []"
    using data_underflow_guard calculation by simp
  hence "stack_depth (data_stack state) = 0"
    unfolding stack_depth_def by simp
  with assms have False by simp
  then show ?thesis by blast
next
  case (Some p)
  then obtain x st' where "pop_data state = Some (x, st')" by (cases p) auto
  thus ?thesis by blast
qed

lemma pop_return_some_ex:
  assumes "stack_depth (return_stack state) > 0"
  shows "\<exists>x st'. pop_return state = Some (x, st')"
proof (cases "pop_return state")
  case None
  then have "return_underflow_ok" using pop_return_None_iff_underflow by simp
  moreover have "return_stack state = []"
    using return_underflow_guard calculation by simp
  hence "stack_depth (return_stack state) = 0"
    unfolding stack_depth_def by simp
  with assms have False by simp
  then show ?thesis by blast
next
  case (Some p)
  then obtain x st' where "pop_return state = Some (x, st')" by (cases p) auto
  thus ?thesis by blast
qed

lemma pop_data_preserves_limit:
  assumes "pop_data state = Some (x, st')"
  shows "stack_depth (data_stack st') \<le> data_limit"
proof -
  obtain s where "stack_pop (data_stack state) = Some (x, s)" and "st' = state\<lparr>data_stack := s\<rparr>"
    using assms unfolding pop_data_def by (cases "stack_pop (data_stack state)") auto
  hence "stack_depth s \<le> data_limit"
    by (simp add: stack_pop_preserves_limit)
  thus ?thesis using `st' = _` by simp
qed

lemma pop_return_preserves_limit:
  assumes "pop_return state = Some (x, st')"
  shows "stack_depth (return_stack st') \<le> return_limit"
proof -
  obtain s where "stack_pop (return_stack state) = Some (x, s)" and "st' = state\<lparr>return_stack := s\<rparr>"
    using assms unfolding pop_return_def by (cases "stack_pop (return_stack state)") auto
  hence "stack_depth s \<le> return_limit"
    by (simp add: return_stack_pop_preserves_limit)
  thus ?thesis using `st' = _` by simp
qed

lemma pop_data_preserves_ok:
  assumes "pop_data state = Some (x, st')"
  shows "stack_ok_limit (data_stack st')"
  using pop_data_preserves_limit[OF assms] unfolding stack_ok_limit_def by simp

lemma pop_return_preserves_ok:
  assumes "pop_return state = Some (x, st')"
  shows "stack_ok_limit_return (return_stack st')"
  using pop_return_preserves_limit[OF assms] unfolding stack_ok_limit_return_def by simp

lemma pop_data_decomp:
  assumes "pop_data state = Some (x, st')"
  obtains s where "stack_pop (data_stack state) = Some (x, s)" "st' = state\<lparr> data_stack := s \<rparr>"
  using assms unfolding pop_data_def by (cases "stack_pop (data_stack state)") auto

lemma pop_return_decomp:
  assumes "pop_return state = Some (x, st')"
  obtains s where "stack_pop (return_stack state) = Some (x, s)" "st' = state\<lparr> return_stack := s \<rparr>"
  using assms unfolding pop_return_def by (cases "stack_pop (return_stack state)") auto

lemma transfer_data_to_return_ok:
  assumes data_nonempty: "stack_depth (data_stack state) > 0"
      and return_room: "stack_depth (return_stack state) < return_limit"
  obtains st' where
    "transfer_data_to_return state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (data_stack st') = stack_depth (data_stack state) - 1"
    "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))"
proof -
  from pop_data_some_ex[OF data_nonempty] obtain x st1 where pop: "pop_data state = Some (x, st1)" by blast
  from pop_data_decomp[OF pop] obtain s where pop_stack: "stack_pop (data_stack state) = Some (x, s)" and st1_def: "st1 = state\<lparr> data_stack := s \<rparr>" .
  have st1_data_eq: "data_stack st1 = s" unfolding st1_def by simp
  have st1_return_eq: "return_stack st1 = return_stack state" unfolding st1_def by simp
  have st1_data_depth: "stack_depth (data_stack st1) = stack_depth (data_stack state) - 1"
    using pop_stack unfolding st1_def stack_depth_def stack_pop_def by (cases "data_stack state") auto
  have st1_data_depth_s: "stack_depth s = stack_depth (data_stack state) - 1"
    using st1_data_depth unfolding st1_data_eq by simp
  have st1_data_ok: "stack_ok_limit (data_stack st1)"
    using pop_data_preserves_ok[OF pop] by simp
  have st1_return_depth: "stack_depth (return_stack st1) = stack_depth (return_stack state)"
    using st1_return_eq by simp
  have transfer_eq: "transfer_data_to_return state = Some (push_return x st1)"
    unfolding transfer_data_to_return_def pop st1_return_eq using return_room by simp
  define st' where "st' = push_return x st1"
  have data_ok': "stack_ok_limit (data_stack st')"
    unfolding st'_def push_return_def by (simp add: st1_data_ok)
  have return_depth': "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))"
    unfolding st'_def push_return_def st1_return_eq stack_depth_def stack_push_def by simp
  have return_ok': "stack_ok_limit_return (return_stack st')"
    unfolding stack_ok_limit_return_def return_depth'
    using return_room by simp
  have data_depth': "stack_depth (data_stack st') = stack_depth (data_stack state) - 1"
  proof -
    have "stack_depth (data_stack st') = stack_depth (data_stack st1)"
      unfolding st'_def push_return_def by simp
    also have "... = stack_depth (data_stack state) - 1"
      using st1_data_depth by simp
    finally show ?thesis .
  qed
  show ?thesis
  proof
    show "transfer_data_to_return state = Some st'"
      using transfer_eq st'_def by simp
    show "stack_ok_limit (data_stack st')" by (simp add: data_ok')
    show "stack_ok_limit_return (return_stack st')" using return_ok' by simp
    show "stack_depth (data_stack st') = stack_depth (data_stack state) - 1" by (simp add: data_depth')
    show "stack_depth (return_stack st') = Suc (stack_depth (return_stack state))" by (simp add: return_depth')
  qed
qed

lemma transfer_return_to_data_ok:
  assumes return_nonempty: "stack_depth (return_stack state) > 0"
      and data_room: "stack_depth (data_stack state) < data_limit"
  obtains st' where
    "transfer_return_to_data state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state) - 1"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
proof -
  from pop_return_some_ex[OF return_nonempty] obtain x st1 where pop: "pop_return state = Some (x, st1)" by blast
  from pop_return_decomp[OF pop] obtain s where pop_stack: "stack_pop (return_stack state) = Some (x, s)" and st1_def: "st1 = state\<lparr> return_stack := s \<rparr>" .
  have st1_data_eq: "data_stack st1 = data_stack state" unfolding st1_def by simp
  have st1_return_eq: "return_stack st1 = s" unfolding st1_def by simp
  have st1_return_depth: "stack_depth (return_stack st1) = stack_depth (return_stack state) - 1"
    using pop_stack unfolding st1_def stack_depth_def stack_pop_def by (cases "return_stack state") auto
  have st1_return_depth_s: "stack_depth s = stack_depth (return_stack state) - 1"
    using st1_return_depth unfolding st1_return_eq by simp
  have st1_return_ok: "stack_ok_limit_return (return_stack st1)"
    using pop_return_preserves_ok[OF pop] by simp
  have st1_data_depth: "stack_depth (data_stack st1) = stack_depth (data_stack state)"
    using st1_data_eq by simp
  have transfer_eq: "transfer_return_to_data state = Some (push_data x st1)"
    unfolding transfer_return_to_data_def pop st1_data_eq using data_room by simp
  define st' where "st' = push_data x st1"
  have return_depth': "stack_depth (return_stack st') = stack_depth (return_stack state) - 1"
  proof -
    have "stack_depth (return_stack st') = stack_depth (return_stack st1)"
      unfolding st'_def push_data_def by simp
    also have "... = stack_depth (return_stack state) - 1"
      using st1_return_depth by simp
    finally show ?thesis .
  qed
  have data_depth': "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
    unfolding st'_def push_data_def st1_data_eq stack_depth_def stack_push_def by simp
  have return_ok': "stack_ok_limit_return (return_stack st')"
  proof -
    have "stack_ok_limit_return s"
      using st1_return_ok unfolding st1_return_eq by simp
    moreover have "return_stack st' = s"
    proof -
      have "return_stack st' = return_stack st1"
        unfolding st'_def push_data_def by simp
      thus ?thesis using st1_return_eq by simp
    qed
    ultimately show ?thesis by simp
  qed
  have data_ok': "stack_ok_limit (data_stack st')"
    unfolding stack_ok_limit_def data_depth'
    using data_room by simp
  show ?thesis
  proof
    show "transfer_return_to_data state = Some st'"
      using transfer_eq st'_def by simp
    show "stack_ok_limit (data_stack st')" by (simp add: data_ok')
    show "stack_ok_limit_return (return_stack st')" using return_ok' by simp
    show "stack_depth (return_stack st') = stack_depth (return_stack state) - 1" by (simp add: return_depth')
    show "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by (simp add: data_depth')
  qed
qed

lemma peek_return_to_data_ok:
  assumes return_nonempty: "stack_depth (return_stack state) > 0"
      and data_room: "stack_depth (data_stack state) < data_limit"
  obtains st' where
    "peek_return_to_data state = Some st'"
    "stack_ok_limit (data_stack st')"
    "stack_ok_limit_return (return_stack st')"
    "stack_depth (return_stack st') = stack_depth (return_stack state)"
    "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
proof -
  obtain x xs where ret_decomp: "return_stack state = x # xs"
  proof (cases "return_stack state")
    case Nil
    then show ?thesis using return_nonempty unfolding stack_depth_def by simp
  next
    case (Cons x xs)
    then show ?thesis by blast
  qed
  have top_eq: "stack_top (return_stack state) = Some x"
    unfolding ret_decomp stack_top_def by simp
  have peek_eq: "peek_return_to_data state = Some (push_data x state)"
    unfolding peek_return_to_data_def top_eq using data_room by simp
  define st' where "st' = push_data x state"
  have data_ok: "stack_ok_limit (data_stack st')"
    unfolding st'_def using push_data_respects_limit[OF data_room]
    by (simp add: stack_ok_limit_def)
  have return_ok: "stack_ok_limit_return (return_stack st')"
    unfolding st'_def push_data_def using stack_ok_limit_return_state by simp
  have return_depth: "stack_depth (return_stack st') = stack_depth (return_stack state)"
    unfolding st'_def push_data_def by simp
  have data_depth: "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))"
    unfolding st'_def push_data_def stack_depth_def stack_push_def by simp
  show ?thesis
  proof
    show "peek_return_to_data state = Some st'"
      using peek_eq unfolding st'_def by simp
    show "stack_ok_limit (data_stack st')" by (rule data_ok)
    show "stack_ok_limit_return (return_stack st')" by (rule return_ok)
    show "stack_depth (return_stack st') = stack_depth (return_stack state)" by (rule return_depth)
    show "stack_depth (data_stack st') = Suc (stack_depth (data_stack state))" by (rule data_depth)
  qed
qed

lemma stack_depth_push[simp]: "stack_depth (stack_push x s) = Suc (stack_depth s)"
  unfolding stack_depth_def stack_push_def by simp

lemma stack_depth_pop:
  "stack_pop s = Some (x, s') \<Longrightarrow> stack_depth s' = stack_depth s - 1"
  unfolding stack_pop_def stack_depth_def by (cases s) auto

lemma stack_top_push[simp]: "stack_top (stack_push x s) = Some x"
  unfolding stack_top_def stack_push_def by simp

lemma stack_pop_push[simp]: "stack_pop (stack_push x s) = Some (x, s)"
  unfolding stack_pop_def stack_push_def by simp

lemma stack_trim_bounds:
  assumes "n \<le> data_limit" "stack_depth (data_stack state) \<le> data_limit"
  shows "stack_depth (stack_trim n (data_stack state)) \<le> data_limit"
  using assms unfolding stack_trim_def stack_depth_def by simp

end

end
