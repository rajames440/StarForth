(*  Title:      HOL/MicroJava/J/JBasis.thy
    Author:     David von Oheimb, TU Muenchen
*)

chapter \<open>Java Source Language \label{cha:j}\<close>

section \<open>Some Auxiliary Definitions\<close>

theory JBasis
imports
  Main
  "HOL-Library.Transitive_Closure_Table"
  "HOL-Eisbach.Eisbach"
begin

lemmas [simp] = Let_def

subsection "unique"
 
definition unique :: "('a \<times> 'b) list => bool" where
  "unique == distinct \<circ> map fst"


lemma fst_in_set_lemma: "(x, y) \<in> set xys \<Longrightarrow> x \<in> fst ` set xys"
  by (induct xys) auto

lemma unique_Nil [simp]: "unique []"
  by (simp add: unique_def)

lemma unique_Cons [simp]: "unique ((x,y)#l) = (unique l & (\<forall>y. (x,y) \<notin> set l))"
  by (auto simp: unique_def dest: fst_in_set_lemma)

lemma unique_append: "unique l' \<Longrightarrow> unique l \<Longrightarrow>
    (\<forall>(x,y) \<in> set l. \<forall>(x',y') \<in> set l'. x' \<noteq> x) \<Longrightarrow> unique (l @ l')"
  by (induct l) (auto dest: fst_in_set_lemma)

lemma unique_map_inj: "unique l ==> inj f ==> unique (map (%(k,x). (f k, g k x)) l)"
  by (induct l) (auto dest: fst_in_set_lemma simp add: inj_eq)


subsection "More about Maps"

lemma map_of_SomeI: "unique l \<Longrightarrow> (k, x) \<in> set l \<Longrightarrow> map_of l k = Some x"
  by (induct l) auto

lemma Ball_set_table: "(\<forall>(x,y)\<in>set l. P x y) ==> (\<forall>x. \<forall>y. map_of l x = Some y --> P x y)"
  by (induct l) auto

lemma table_of_remap_SomeD:
  "map_of (map (\<lambda>((k,k'),x). (k,(k',x))) t) k = Some (k',x) ==>
    map_of t (k, k') = Some x"
  by (atomize (full), induct t) auto

end
