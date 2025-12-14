theory VM_DataStack_Words
  imports VM_StackRuntime
begin

section \<open>Core data-stack manipulation words\<close>

text \<open>
  We formalise several basic FORTH-79 stack operators (`DROP`, `DUP`,
  `?DUP`, `SWAP`, `OVER`, `ROT`, and `-ROT`).  The definitions follow the
  C implementation: guard failures set the VM error flag via
  @{const vm_set_error} while leaving the data stack untouched.
\<close>

context vm_stack_model begin

subsection \<open>Definitions\<close>

definition word_drop :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_drop st = snd (vm_pop_sem st)"

definition word_dup :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_dup st = (case data_stack st of [] \<Rightarrow> vm_set_error True st | x # _ \<Rightarrow> vm_push_sem x st)"

definition word_qdup :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_qdup st = (case data_stack st of [] \<Rightarrow> vm_set_error True st | x # _ \<Rightarrow> if x = 0 then st else vm_push_sem x st)"

definition word_swap :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_swap st = (case data_stack st of x # y # xs \<Rightarrow> st\<lparr> data_stack := y # x # xs \<rparr> | _ \<Rightarrow> vm_set_error True st)"

definition word_over :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_over st = (case data_stack st of x # y # xs \<Rightarrow> vm_push_sem y st | _ \<Rightarrow> vm_set_error True st)"

definition word_rot :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_rot st = (case data_stack st of x # y # z # xs \<Rightarrow> st\<lparr> data_stack := z # x # y # xs \<rparr> | _ \<Rightarrow> vm_set_error True st)"

definition word_minus_rot :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_minus_rot st = (case data_stack st of x # y # z # xs \<Rightarrow> st\<lparr> data_stack := y # z # x # xs \<rparr> | _ \<Rightarrow> vm_set_error True st)"

definition word_depth :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_depth st = (if stack_depth (data_stack st) < data_limit
     then vm_push_sem (int_to_cell (int (stack_depth (data_stack st)))) st
     else vm_set_error True st)"

definition word_pick :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_pick st = (case data_stack st of
      [] \<Rightarrow> vm_set_error True st
    | idx # xs \<Rightarrow>
        let i = cell_to_int idx in
        if i < 0 then vm_set_error True st
        else let n = nat i; stack = idx # xs in
          if n \<ge> length stack then vm_set_error True st
          else let v = stack ! n in
            st\<lparr> data_stack := v # xs \<rparr>)"

definition roll_list :: "nat \<Rightarrow> 'val list \<Rightarrow> 'val list" where
  "roll_list n xs = (if n < length xs then (xs ! n) # take n xs @ drop (Suc n) xs else xs)"

definition word_roll :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "word_roll st = (case data_stack st of
      [] \<Rightarrow> vm_set_error True st
    | idx # xs \<Rightarrow>
        let i = cell_to_int idx in
        if i < 0 then vm_set_error True st
        else let n = nat i in
          if n \<ge> length xs then vm_set_error True st
          else st\<lparr> data_stack := roll_list n xs \<rparr>)"

subsection \<open>Error cases\<close>

lemma word_drop_underflow:
  assumes "data_stack st = []"
  shows "word_drop st = vm_set_error True st"
    and "vm_error_active (word_drop st)"
    and "data_stack (word_drop st) = data_stack st"
  using assms unfolding word_drop_def vm_pop_sem_def by simp_all

lemma word_dup_underflow:
  assumes "data_stack st = []"
  shows "word_dup st = vm_set_error True st"
    and "vm_error_active (word_dup st)"
    and "data_stack (word_dup st) = data_stack st"
  using assms unfolding word_dup_def by simp_all

lemma word_qdup_underflow:
  assumes "data_stack st = []"
  shows "word_qdup st = vm_set_error True st"
    and "vm_error_active (word_qdup st)"
    and "data_stack (word_qdup st) = data_stack st"
  using assms unfolding word_qdup_def by simp_all

lemma word_swap_underflow:
  assumes "length (data_stack st) < 2"
  shows "word_swap st = vm_set_error True st"
    and "vm_error_active (word_swap st)"
    and "data_stack (word_swap st) = data_stack st"
  using assms unfolding word_swap_def by (cases "data_stack st") simp_all

lemma word_over_underflow:
  assumes "length (data_stack st) < 2"
  shows "word_over st = vm_set_error True st"
    and "vm_error_active (word_over st)"
    and "data_stack (word_over st) = data_stack st"
  using assms unfolding word_over_def by (cases "data_stack st") simp_all

lemma word_rot_underflow:
  assumes "length (data_stack st) < 3"
  shows "word_rot st = vm_set_error True st"
    and "vm_error_active (word_rot st)"
    and "data_stack (word_rot st) = data_stack st"
  using assms unfolding word_rot_def by (cases "data_stack st") simp_all

lemma word_minus_rot_underflow:
  assumes "length (data_stack st) < 3"
  shows "word_minus_rot st = vm_set_error True st"
    and "vm_error_active (word_minus_rot st)"
    and "data_stack (word_minus_rot st) = data_stack st"
  using assms unfolding word_minus_rot_def by (cases "data_stack st") simp_all

subsection \<open>Success properties\<close>

lemma word_drop_success:
  assumes "data_stack st = x # xs"
  shows "word_drop st = st\<lparr> data_stack := xs \<rparr>"
    and "vm_error_active (word_drop st) = vm_error_active st"
    and "stack_depth (data_stack (word_drop st)) = stack_depth (data_stack st) - 1"
proof -
  have "pop_data st = Some (x, st\<lparr> data_stack := xs \<rparr>)"
    using assms unfolding pop_data_def stack_pop_def by simp
  hence "vm_pop_sem st = (Some x, st\<lparr> data_stack := xs \<rparr>)"
    unfolding vm_pop_sem_def by simp
  thus ?thesis
    unfolding word_drop_def vm_error_active_def stack_depth_def by simp_all
qed

lemma word_dup_success:
  assumes stk: "data_stack st = x # xs"
      and guard: "stack_depth (data_stack st) < data_limit"
  shows "word_dup st = push_data x st"
    and "vm_error_active (word_dup st) = vm_error_active st"
    and "data_stack (word_dup st) = x # x # xs"
    and "stack_depth (data_stack (word_dup st)) = Suc (stack_depth (data_stack st))"
proof -
  have push: "vm_push_sem x st = push_data x st"
    using vm_push_sem_success[OF guard] by simp
  have ds: "stack_depth (data_stack (push_data x st)) = Suc (stack_depth (data_stack st))"
    using push vm_push_sem_success[OF guard] by simp
  show
    "word_dup st = push_data x st"
    "vm_error_active (word_dup st) = vm_error_active st"
    "data_stack (word_dup st) = x # x # xs"
    "stack_depth (data_stack (word_dup st)) = Suc (stack_depth (data_stack st))"
    using stk push vm_push_sem_success[OF guard] ds unfolding word_dup_def push_data_def stack_push_def by simp_all
qed

lemma word_qdup_zero:
  assumes "data_stack st = 0 # xs"
  shows "word_qdup st = st"
  using assms unfolding word_qdup_def by simp

lemma word_qdup_nonzero_success:
  assumes stk: "data_stack st = x # xs" and nz: "x \<noteq> 0"
      and guard: "stack_depth (data_stack st) < data_limit"
  shows "word_qdup st = push_data x st"
    and "vm_error_active (word_qdup st) = vm_error_active st"
    and "data_stack (word_qdup st) = x # x # xs"
  using stk nz guard word_dup_success[OF stk guard] unfolding word_qdup_def by simp_all

lemma word_swap_success:
  assumes "data_stack st = x # y # xs"
  shows "word_swap st = st\<lparr> data_stack := y # x # xs \<rparr>"
    and "vm_error_active (word_swap st) = vm_error_active st"
    and "stack_depth (data_stack (word_swap st)) = stack_depth (data_stack st)"
  using assms unfolding word_swap_def vm_error_active_def stack_depth_def by simp_all

lemma word_over_success:
  assumes stk: "data_stack st = x # y # xs"
      and guard: "stack_depth (data_stack st) < data_limit"
  shows "word_over st = push_data y st"
    and "vm_error_active (word_over st) = vm_error_active st"
    and "data_stack (word_over st) = y # x # y # xs"
  using stk vm_push_sem_success[OF guard] unfolding word_over_def push_data_def stack_push_def by simp_all

lemma word_rot_success:
  assumes "data_stack st = x # y # z # xs"
  shows "word_rot st = st\<lparr> data_stack := z # x # y # xs \<rparr>"
    and "vm_error_active (word_rot st) = vm_error_active st"
    and "stack_depth (data_stack (word_rot st)) = stack_depth (data_stack st)"
  using assms unfolding word_rot_def vm_error_active_def stack_depth_def by simp_all

lemma word_minus_rot_success:
  assumes "data_stack st = x # y # z # xs"
  shows "word_minus_rot st = st\<lparr> data_stack := y # z # x # xs \<rparr>"
    and "vm_error_active (word_minus_rot st) = vm_error_active st"
    and "stack_depth (data_stack (word_minus_rot st)) = stack_depth (data_stack st)"
  using assms unfolding word_minus_rot_def vm_error_active_def stack_depth_def by simp_all

lemma word_depth_success:
  assumes guard: "stack_depth (data_stack st) < data_limit"
  shows "word_depth st = push_data (int_to_cell (int (stack_depth (data_stack st)))) st"
    and "vm_error_active (word_depth st) = vm_error_active st"
    and "stack_depth (data_stack (word_depth st)) = Suc (stack_depth (data_stack st))"
proof -
  from vm_push_sem_success[OF guard]
  have eq: "vm_push_sem (int_to_cell (int (stack_depth (data_stack st)))) st =
            push_data (int_to_cell (int (stack_depth (data_stack st)))) st"
    and err: "vm_error_active (vm_push_sem (int_to_cell (int (stack_depth (data_stack st)))) st) = vm_error_active st"
    and depth': "stack_depth (data_stack (vm_push_sem (int_to_cell (int (stack_depth (data_stack st)))) st)) =
                  Suc (stack_depth (data_stack st))" by simp_all
  thus
    "word_depth st = push_data (int_to_cell (int (stack_depth (data_stack st)))) st"
    "vm_error_active (word_depth st) = vm_error_active st"
    "stack_depth (data_stack (word_depth st)) = Suc (stack_depth (data_stack st))"
    unfolding word_depth_def by simp_all
qed

lemma word_depth_overflow:
  assumes "\<not> stack_depth (data_stack st) < data_limit"
  shows "word_depth st = vm_set_error True st"
    and "vm_error_active (word_depth st)"
    and "data_stack (word_depth st) = data_stack st"
  using assms unfolding word_depth_def by simp_all

lemma word_pick_underflow:
  assumes "data_stack st = []"
  shows "word_pick st = vm_set_error True st"
    and "vm_error_active (word_pick st)"
    and "data_stack (word_pick st) = data_stack st"
  using assms unfolding word_pick_def by simp_all

lemma word_pick_negative_index:
  assumes "data_stack st = idx # xs" "cell_to_int idx < 0"
  shows "word_pick st = vm_set_error True st"
    and "vm_error_active (word_pick st)"
    and "data_stack (word_pick st) = data_stack st"
  using assms unfolding word_pick_def by simp_all

lemma word_pick_out_of_range:
  assumes "data_stack st = idx # xs" "0 \<le> cell_to_int idx"
      "nat (cell_to_int idx) \<ge> length (idx # xs)"
  shows "word_pick st = vm_set_error True st"
    and "vm_error_active (word_pick st)"
    and "data_stack (word_pick st) = data_stack st"
  using assms unfolding word_pick_def by simp_all

lemma word_pick_success:
  assumes stk: "data_stack st = idx # xs"
      and idx_val: "cell_to_int idx = int n"
      and range: "n < length (idx # xs)"
  shows "word_pick st = st\<lparr> data_stack := (idx # xs) ! n # xs \<rparr>"
    and "vm_error_active (word_pick st) = vm_error_active st"
    and "stack_depth (data_stack (word_pick st)) = stack_depth (data_stack st)"
proof -
  have n_nonneg: "0 \<le> cell_to_int idx"
    using idx_val by simp
  have nz: "nat (cell_to_int idx) = n"
    using idx_val by simp
  from range have len_pos: "length (idx # xs) > 0"
    by simp
  have def: "word_pick st = st\<lparr> data_stack := (idx # xs) ! n # xs \<rparr>"
    using stk idx_val range unfolding word_pick_def by simp
  moreover have "vm_error_active (word_pick st) = vm_error_active st"
    using def unfolding vm_error_active_def by simp
  moreover have "stack_depth (data_stack (word_pick st)) = stack_depth (data_stack st)"
    using def unfolding stack_depth_def by simp
  ultimately show ?thesis by simp
qed

lemma roll_list_length:
  assumes "n < length xs"
  shows "length (roll_list n xs) = length xs"
  using assms unfolding roll_list_def by simp

lemma roll_list_head:
  assumes "n < length xs"
  shows "hd (roll_list n xs) = xs ! n"
  using assms unfolding roll_list_def by (cases xs) simp_all

lemma word_roll_underflow:
  assumes "data_stack st = []"
  shows "word_roll st = vm_set_error True st"
    and "vm_error_active (word_roll st)"
    and "data_stack (word_roll st) = data_stack st"
  using assms unfolding word_roll_def by simp_all

lemma word_roll_negative_index:
  assumes "data_stack st = idx # xs" "cell_to_int idx < 0"
  shows "word_roll st = vm_set_error True st"
    and "vm_error_active (word_roll st)"
    and "data_stack (word_roll st) = data_stack st"
  using assms unfolding word_roll_def by simp_all

lemma word_roll_out_of_range:
  assumes "data_stack st = idx # xs" "0 \<le> cell_to_int idx"
      "nat (cell_to_int idx) \<ge> length xs"
  shows "word_roll st = vm_set_error True st"
    and "vm_error_active (word_roll st)"
    and "data_stack (word_roll st) = data_stack st"
  using assms unfolding word_roll_def roll_list_def by simp_all

lemma word_roll_success:
  assumes stk: "data_stack st = idx # xs"
      and idx_val: "cell_to_int idx = int n"
      and range: "n < length xs"
  shows "word_roll st = st\<lparr> data_stack := roll_list n xs \<rparr>"
    and "vm_error_active (word_roll st) = vm_error_active st"
    and "stack_depth (data_stack (word_roll st)) = stack_depth (data_stack st) - 1"
proof -
  have guard: "0 \<le> cell_to_int idx" "nat (cell_to_int idx) = n"
    using idx_val by simp_all
  have def: "word_roll st = st\<lparr> data_stack := roll_list n xs \<rparr>"
    using stk idx_val range unfolding word_roll_def by simp
  moreover have "vm_error_active (word_roll st) = vm_error_active st"
    using def unfolding vm_error_active_def by simp
  moreover have "stack_depth (data_stack (word_roll st)) = stack_depth (roll_list n xs)"
    using def by simp
  moreover have "stack_depth (roll_list n xs) = stack_depth xs"
    using roll_list_length[OF range] unfolding stack_depth_def by simp
  moreover have "stack_depth xs = stack_depth (data_stack st) - 1"
    using stk unfolding stack_depth_def by simp
  ultimately show ?thesis by simp
qed

end

subsection \<open>Runtime correspondence\<close>

context vm_stack_runtime begin

definition vm_drop_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_drop_impl st = (if dsp < 0 then vm_set_error True st else word_drop st)"

definition vm_dup_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_dup_impl st = (if dsp < 0 then vm_set_error True st
     else if dsp + 1 \<ge> int data_limit then vm_set_error True st
     else word_dup st)"

definition vm_qdup_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_qdup_impl st = (if dsp < 0 then vm_set_error True st
     else if dsp + 1 \<ge> int data_limit \<and> stack_top (data_stack st) \<noteq> Some 0 then vm_set_error True st
     else word_qdup st)"

definition vm_swap_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_swap_impl st = (if dsp < 1 then vm_set_error True st else word_swap st)"

definition vm_over_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_over_impl st = (if dsp < 1 then vm_set_error True st
     else if dsp + 1 \<ge> int data_limit then vm_set_error True st
     else word_over st)"

definition vm_rot_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_rot_impl st = (if dsp < 2 then vm_set_error True st else word_rot st)"

definition vm_minus_rot_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_minus_rot_impl st = (if dsp < 2 then vm_set_error True st else word_minus_rot st)"

definition vm_depth_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_depth_impl st = (if dsp + 1 \<ge> int data_limit then vm_set_error True st else word_depth st)"

definition vm_pick_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_pick_impl st = (if dsp < 0 then vm_set_error True st
     else case data_stack st of [] \<Rightarrow> vm_set_error True st | idx # xs \<Rightarrow>
       let i = cell_to_int idx in
       if i < 0 then vm_set_error True st
       else let n = nat i; stack = idx # xs in
         if int (length stack) \<le> i then vm_set_error True st else word_pick st)"

definition vm_roll_impl :: "('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state" where
  "vm_roll_impl st = (if dsp < 0 then vm_set_error True st
     else case data_stack st of [] \<Rightarrow> vm_set_error True st | idx # xs \<Rightarrow>
       let i = cell_to_int idx in
       if i < 0 then vm_set_error True st
       else let n = nat i in
         if n \<ge> length xs then vm_set_error True st else word_roll st)"

lemma vm_drop_impl_eq:
  "vm_drop_impl state = word_drop state"
proof (cases "dsp < 0")
  case True
  then have "stack_depth (data_stack state) = 0"
    using dsp_underflow_iff by simp
  thus ?thesis
    unfolding vm_drop_impl_def word_drop_def vm_pop_sem_def pop_data_def stack_pop_def
    using True by (cases "data_stack state") auto
next
  case False
  then obtain x xs where "data_stack state = x # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") auto
  thus ?thesis
    unfolding vm_drop_impl_def word_drop_def by simp
qed

lemma vm_dup_impl_eq:
  "vm_dup_impl state = word_dup state"
proof (cases "dsp < 0")
  case True
  then have "stack_depth (data_stack state) = 0"
    using dsp_underflow_iff by simp
  thus ?thesis
    unfolding vm_dup_impl_def word_dup_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x xs where stk: "data_stack state = x # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") auto
  show ?thesis
  proof (cases "dsp + 1 \<ge> int data_limit")
    case True
    have "vm_push_sem x state = vm_set_error True state"
      using vm_push_sem_runtime_overflow[OF True] by simp
    thus ?thesis
      using stk unfolding vm_dup_impl_def word_dup_def by simp
  next
    case False2
    hence guard: "stack_depth (data_stack state) < data_limit"
      using data_guard_ptr_iff by simp
    thus ?thesis
      using stk unfolding vm_dup_impl_def word_dup_def by simp
  qed
qed

lemma vm_qdup_impl_eq:
  "vm_qdup_impl state = word_qdup state"
proof (cases "dsp < 0")
  case True
  then have "stack_depth (data_stack state) = 0"
    using dsp_underflow_iff by simp
  thus ?thesis
    unfolding vm_qdup_impl_def word_qdup_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x xs where stk: "data_stack state = x # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") auto
  show ?thesis
  proof (cases "x = 0")
    case True
    thus ?thesis
      using stk unfolding vm_qdup_impl_def word_qdup_def by simp
  next
    case False2
    show ?thesis
    proof (cases "dsp + 1 \<ge> int data_limit")
      case True
      have "vm_push_sem x state = vm_set_error True state"
        using vm_push_sem_runtime_overflow[OF True] by simp
      thus ?thesis
        using stk False2 unfolding vm_qdup_impl_def word_qdup_def by simp
    next
      case False3
      hence guard: "stack_depth (data_stack state) < data_limit"
        using data_guard_ptr_iff by simp
      thus ?thesis
        using stk False2 unfolding vm_qdup_impl_def word_qdup_def by simp
    qed
  qed
qed

lemma vm_swap_impl_eq:
  "vm_swap_impl state = word_swap state"
proof (cases "dsp < 1")
  case True
  then have "stack_depth (data_stack state) \<le> 1"
    using data_ptr_rel by auto
  thus ?thesis
    unfolding vm_swap_impl_def word_swap_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x y xs where "data_stack state = x # y # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") (auto, case_tac lista)
  thus ?thesis
    unfolding vm_swap_impl_def word_swap_def by simp
qed

lemma vm_over_impl_eq:
  "vm_over_impl state = word_over state"
proof (cases "dsp < 1")
  case True
  then have "stack_depth (data_stack state) \<le> 1"
    using data_ptr_rel by auto
  thus ?thesis
    unfolding vm_over_impl_def word_over_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x y xs where stk: "data_stack state = x # y # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") (auto, case_tac lista)
  show ?thesis
  proof (cases "dsp + 1 \<ge> int data_limit")
    case True
    have "vm_push_sem y state = vm_set_error True state"
      using vm_push_sem_runtime_overflow[OF True] by simp
    thus ?thesis
      using stk unfolding vm_over_impl_def word_over_def by simp
  next
    case False2
    hence guard: "stack_depth (data_stack state) < data_limit"
      using data_guard_ptr_iff by simp
    thus ?thesis
      using stk unfolding vm_over_impl_def word_over_def by simp
  qed
qed

lemma vm_rot_impl_eq:
  "vm_rot_impl state = word_rot state"
proof (cases "dsp < 2")
  case True
  then have "stack_depth (data_stack state) \<le> 2"
    using data_ptr_rel by auto
  thus ?thesis
    unfolding vm_rot_impl_def word_rot_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x y z xs where "data_stack state = x # y # z # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") (auto, case_tac lista, case_tac listaa)
  thus ?thesis
    unfolding vm_rot_impl_def word_rot_def by simp
qed

lemma vm_minus_rot_impl_eq:
  "vm_minus_rot_impl state = word_minus_rot state"
proof (cases "dsp < 2")
  case True
  then have "stack_depth (data_stack state) \<le> 2"
    using data_ptr_rel by auto
  thus ?thesis
    unfolding vm_minus_rot_impl_def word_minus_rot_def stack_depth_def by (cases "data_stack state") auto
next
  case False
  then obtain x y z xs where "data_stack state = x # y # z # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") (auto, case_tac lista, case_tac listaa)
  thus ?thesis
    unfolding vm_minus_rot_impl_def word_minus_rot_def by simp
qed

lemma vm_depth_impl_eq:
  "vm_depth_impl state = word_depth state"
proof (cases "dsp + 1 \<ge> int data_limit")
  case True
  then have "\<not> stack_depth (data_stack state) < data_limit"
    using data_guard_ptr_iff by auto
  thus ?thesis
    unfolding vm_depth_impl_def word_depth_def by simp
next
  case False
  then have guard: "stack_depth (data_stack state) < data_limit"
    using data_guard_ptr_iff by simp
  thus ?thesis
    unfolding vm_depth_impl_def word_depth_def by simp
qed

lemma vm_pick_impl_eq:
  "vm_pick_impl state = word_pick state"
proof (cases "dsp < 0")
  case True
  then have "stack_depth (data_stack state) = 0"
    using dsp_underflow_iff by simp
  hence "data_stack state = []"
    unfolding stack_depth_def by (cases "data_stack state") auto
  thus ?thesis
    unfolding vm_pick_impl_def word_pick_def by simp
next
  case False
  then obtain idx xs where stk: "data_stack state = idx # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") auto
  show ?thesis
  proof (cases "cell_to_int idx < 0")
    case True
    thus ?thesis
      using stk unfolding vm_pick_impl_def word_pick_def by simp
  next
    case False'
    set i = "cell_to_int idx"
    show ?thesis
    proof (cases "int (length (idx # xs)) \<le> i")
      case True
      then obtain n where "nat i \<ge> length (idx # xs)"
        by (metis False' nat_le_iff)
      thus ?thesis
        using stk False' unfolding vm_pick_impl_def word_pick_def by simp
    next
      case False''
      thus ?thesis
        unfolding vm_pick_impl_def using stk False' by simp
    qed
  qed
qed

lemma vm_roll_impl_eq:
  "vm_roll_impl state = word_roll state"
proof (cases "dsp < 0")
  case True
  then have "stack_depth (data_stack state) = 0"
    using dsp_underflow_iff by simp
  hence "data_stack state = []"
    unfolding stack_depth_def by (cases "data_stack state") auto
  thus ?thesis
    unfolding vm_roll_impl_def word_roll_def by simp
next
  case False
  then obtain idx xs where stk: "data_stack state = idx # xs"
    using dsp_nonneg_iff unfolding stack_depth_def by (cases "data_stack state") auto
  show ?thesis
  proof (cases "cell_to_int idx < 0")
    case True
    thus ?thesis
      using stk unfolding vm_roll_impl_def word_roll_def by simp
  next
    case False'
    set i = "cell_to_int idx"
    show ?thesis
    proof (cases "nat i \<ge> length xs")
      case True
      thus ?thesis
        using stk False' unfolding vm_roll_impl_def word_roll_def by simp
    next
      case False''
      thus ?thesis
        using stk False' unfolding vm_roll_impl_def word_roll_def by simp
    qed
  qed
qed

end

end

end
