(*  Title:      HOL/Library/Subseq_Order.thy
    Author:     Peter Lammich, Uni Muenster <peter.lammich@uni-muenster.de>
    Author:     Florian Haftmann, TU Muenchen
    Author:     Tobias Nipkow, TU Muenchen
*)

section \<open>Subsequence Ordering\<close>

theory Subseq_Order
imports Sublist
begin

text \<open>
  This theory defines subsequence ordering on lists. A list \<open>ys\<close> is a subsequence of a
  list \<open>xs\<close>, iff one obtains \<open>ys\<close> by erasing some elements from \<open>xs\<close>.
\<close>

subsection \<open>Definitions and basic lemmas\<close>

instantiation list :: (type) ord
begin

definition less_eq_list
  where \<open>xs \<le> ys \<longleftrightarrow> subseq xs ys\<close> for xs ys :: \<open>'a list\<close>

definition less_list
  where \<open>xs < ys \<longleftrightarrow> xs \<le> ys \<and> \<not> ys \<le> xs\<close> for xs ys :: \<open>'a list\<close>

instance ..

end

instance list :: (type) order
proof
  fix xs ys zs :: "'a list"
  show "xs < ys \<longleftrightarrow> xs \<le> ys \<and> \<not> ys \<le> xs"
    unfolding less_list_def ..
  show "xs \<le> xs"
    by (simp add: less_eq_list_def)
  show "xs = ys" if "xs \<le> ys" and "ys \<le> xs"
    using that unfolding less_eq_list_def
    by (rule subseq_order.antisym)
  show "xs \<le> zs" if "xs \<le> ys" and "ys \<le> zs"
    using that unfolding less_eq_list_def
    by (rule subseq_order.order_trans)
qed

lemmas less_eq_list_induct [consumes 1, case_names empty drop take] =
  list_emb.induct [of "(=)", folded less_eq_list_def]

lemma less_eq_list_empty [code]:
  \<open>[] \<le> xs \<longleftrightarrow> True\<close>
  by (simp add: less_eq_list_def)

lemma less_eq_list_below_empty [code]:
  \<open>x # xs \<le> [] \<longleftrightarrow> False\<close>
  by (simp add: less_eq_list_def)

lemma le_list_Cons2_iff [simp, code]:
  \<open>x # xs \<le> y # ys \<longleftrightarrow> (if x = y then xs \<le> ys else x # xs \<le> ys)\<close>
  by (simp add: less_eq_list_def)

lemma less_list_empty [simp]:
  \<open>[] < xs \<longleftrightarrow> xs \<noteq> []\<close>
  by (metis less_eq_list_def list_emb_Nil order_less_le)

lemma less_list_empty_Cons [code]:
  \<open>[] < x # xs \<longleftrightarrow> True\<close>
  by simp_all

lemma less_list_below_empty [simp, code]:
  \<open>xs < [] \<longleftrightarrow> False\<close>
  by (metis list_emb_Nil less_eq_list_def less_list_def)

lemma less_list_Cons2_iff [code]:
  \<open>x # xs < y # ys \<longleftrightarrow> (if x = y then xs < ys else x # xs \<le> ys)\<close>
  by (simp add: less_le)

lemmas less_eq_list_drop = list_emb.list_emb_Cons [of "(=)", folded less_eq_list_def]
lemmas le_list_map = subseq_map [folded less_eq_list_def]
lemmas le_list_filter = subseq_filter [folded less_eq_list_def]
lemmas le_list_length = list_emb_length [of "(=)", folded less_eq_list_def]

lemma less_list_length: "xs < ys \<Longrightarrow> length xs < length ys"
  by (metis list_emb_length subseq_same_length le_neq_implies_less less_list_def less_eq_list_def)

lemma less_list_drop: "xs < ys \<Longrightarrow> xs < x # ys"
  by (unfold less_le less_eq_list_def) (auto)

lemma less_list_take_iff: "x # xs < x # ys \<longleftrightarrow> xs < ys"
  by (metis subseq_Cons2_iff less_list_def less_eq_list_def)

lemma less_list_drop_many: "xs < ys \<Longrightarrow> xs < zs @ ys"
  by (metis subseq_append_le_same_iff subseq_drop_many order_less_le
      self_append_conv2 less_eq_list_def)

lemma less_list_take_many_iff: "zs @ xs < zs @ ys \<longleftrightarrow> xs < ys"
  by (metis less_list_def less_eq_list_def subseq_append')

lemma less_list_rev_take: "xs @ zs < ys @ zs \<longleftrightarrow> xs < ys"
  by (unfold less_le less_eq_list_def) auto

end
