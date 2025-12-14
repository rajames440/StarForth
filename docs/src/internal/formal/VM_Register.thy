theory VM_Register
  imports VM_Words VM_DataStack_Words
begin

section \<open>Dictionary registration model\<close>

text \<open>
  We model the portion of the VM dictionary relevant to the return-stack
  words.  The model keeps only the word name together with the abstract
  semantic function (state → state).
\<close>

type_synonym ('dict,'val,'io,'phys) word_dict =
  "(string \<times> (('dict,'val stack,'io,'phys) vm_core_state \<Rightarrow> ('dict,'val stack,'io,'phys) vm_core_state)) list"

fun lookup_word :: "string \<Rightarrow> ('d,'v,'i,'p) word_dict \<Rightarrow> (('d,'v stack,'i,'p) vm_core_state \<Rightarrow> ('d,'v stack,'i,'p) vm_core_state) option" where
  "lookup_word _ [] = None" |
  "lookup_word name ((n,f)#rest) = (if name = n then Some f else lookup_word name rest)"

definition register_word :: "string \<Rightarrow> (('d,'v stack,'i,'p) vm_core_state \<Rightarrow> ('d,'v stack,'i,'p) vm_core_state) \<Rightarrow> ('d,'v,'i,'p) word_dict \<Rightarrow> ('d,'v,'i,'p) word_dict" where
  "register_word name f dict = (name, f) # dict"

context vm_stack_runtime begin

definition register_return_stack_words :: "('dict,'val,'io,'phys) word_dict \<Rightarrow> ('dict,'val,'io,'phys) word_dict" where
  "register_return_stack_words dict =
      register_word \"R@\" word_peek_return (
      register_word \"R>\" word_from_return (
      register_word \">R\" word_to_return dict))"

definition register_data_stack_words :: "('dict,'val,'io,'phys) word_dict \<Rightarrow> ('dict,'val,'io,'phys) word_dict" where
  "register_data_stack_words dict =
      register_word \"ROLL\" word_roll (
      register_word \"PICK\" word_pick (
      register_word \"DEPTH\" word_depth (
      register_word \"-ROT\" word_minus_rot (
      register_word \"ROT\" word_rot (
      register_word \"OVER\" word_over (
      register_word \"SWAP\" word_swap (
      register_word \"?DUP\" word_qdup (
      register_word \"DUP\" word_dup (
      register_word \"DROP\" word_drop dict)))))))))"

lemma lookup_register_word_same:
  "lookup_word name (register_word name f dict) = Some f"
  unfolding register_word_def by simp

lemma lookup_register_word_other:
  assumes "name \<noteq> name'"
  shows "lookup_word name (register_word name' f dict) = lookup_word name dict"
  using assms unfolding register_word_def by simp

lemma register_return_stack_words_to_r:
  "lookup_word \">R\" (register_return_stack_words dict) = Some word_to_return"
  unfolding register_return_stack_words_def
  by (simp add: register_word_def)

lemma register_return_stack_words_r_from:
  "lookup_word \"R>\" (register_return_stack_words dict) = Some word_from_return"
  unfolding register_return_stack_words_def register_word_def
  by simp

lemma register_return_stack_words_r_peek:
  "lookup_word \"R@\" (register_return_stack_words dict) = Some word_peek_return"
  unfolding register_return_stack_words_def
  by (simp add: register_word_def)

lemma register_return_stack_words_preserve_other:
  assumes name \notin set [\">R\", \"R>\", \"R@\"]
  shows "lookup_word name (register_return_stack_words dict) = lookup_word name dict"
  using assms unfolding register_return_stack_words_def register_word_def by simp

lemma register_data_stack_words_drop:
  "lookup_word \"DROP\" (register_data_stack_words dict) = Some word_drop"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_dup:
  "lookup_word \"DUP\" (register_data_stack_words dict) = Some word_dup"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_qdup:
  "lookup_word \"?DUP\" (register_data_stack_words dict) = Some word_qdup"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_swap:
  "lookup_word \"SWAP\" (register_data_stack_words dict) = Some word_swap"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_over:
  "lookup_word \"OVER\" (register_data_stack_words dict) = Some word_over"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_rot:
  "lookup_word \"ROT\" (register_data_stack_words dict) = Some word_rot"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_minus_rot:
  "lookup_word \"-ROT\" (register_data_stack_words dict) = Some word_minus_rot"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_depth:
  "lookup_word \"DEPTH\" (register_data_stack_words dict) = Some word_depth"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_pick:
  "lookup_word \"PICK\" (register_data_stack_words dict) = Some word_pick"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_roll:
  "lookup_word \"ROLL\" (register_data_stack_words dict) = Some word_roll"
  unfolding register_data_stack_words_def by (simp add: register_word_def)

lemma register_data_stack_words_preserve_other:
  assumes name \notin set [\"DROP\", \"DUP\", \"?DUP\", \"SWAP\", \"OVER\", \"ROT\", \"-ROT\", \"DEPTH\", \"PICK\", \"ROLL\"]
  shows "lookup_word name (register_data_stack_words dict) = lookup_word name dict"
  using assms unfolding register_data_stack_words_def register_word_def by simp

end

end
