theory VM_Register
  imports Physics_Formal.VM_Words
begin

section \<open>Dictionary registration model\<close>

text \<open>
  We model the portion of the VM dictionary relevant to the return-stack
  words.  The model keeps only the word name together with the abstract
  semantic function (state → state).
\<close>

type_synonym ('dict,'val,'io) word_dict =
  "(string \<times> (('dict,'val stack,'io) vm_core_state \<Rightarrow> ('dict,'val stack,'io) vm_core_state)) list"

fun lookup_word :: "string \<Rightarrow> ('d,'v,'i) word_dict \<Rightarrow> (('d,'v stack,'i) vm_core_state \<Rightarrow> ('d,'v stack,'i) vm_core_state) option" where
  "lookup_word _ [] = None" |
  "lookup_word name ((n,f)#rest) = (if name = n then Some f else lookup_word name rest)"

definition register_word :: "string \<Rightarrow> (('d,'v stack,'i) vm_core_state \<Rightarrow> ('d,'v stack,'i) vm_core_state) \<Rightarrow> ('d,'v,'i) word_dict \<Rightarrow> ('d,'v,'i) word_dict" where
  "register_word name f dict = (name, f) # dict"

context vm_stack_runtime begin

definition register_return_stack_words :: "('dict,'val,'io) word_dict \<Rightarrow> ('dict,'val,'io) word_dict" where
  "register_return_stack_words dict =
      register_word \"R@\" word_peek_return (
      register_word \"R>\" word_from_return (
      register_word \">R\" word_to_return dict))"

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

end

end
