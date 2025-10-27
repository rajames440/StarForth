(*<*)theory Mutual imports Main begin(*>*)

subsection\<open>Mutually Inductive Definitions\<close>

text\<open>
Just as there are datatypes defined by mutual recursion, there are sets defined
by mutual induction. As a trivial example we consider the even and odd
natural numbers:
\<close>

inductive_set
  Even :: "nat set" and
  Odd  :: "nat set"
where
  zero:  "0 \<in> Even"
| EvenI: "n \<in> Odd \<Longrightarrow> Suc n \<in> Even"
| OddI:  "n \<in> Even \<Longrightarrow> Suc n \<in> Odd"

text\<open>\noindent
The mutually inductive definition of multiple sets is no different from
that of a single set, except for induction: just as for mutually recursive
datatypes, induction needs to involve all the simultaneously defined sets. In
the above case, the induction rule is called @{thm[source]Even_Odd.induct}
(simply concatenate the names of the sets involved) and has the conclusion
@{text[display]"(?x \<in> Even \<longrightarrow> ?P ?x) \<and> (?y \<in> Odd \<longrightarrow> ?Q ?y)"}

If we want to prove that all even numbers are divisible by two, we have to
generalize the statement as follows:
\<close>

lemma "(m \<in> Even \<longrightarrow> 2 dvd m) \<and> (n \<in> Odd \<longrightarrow> 2 dvd (Suc n))"

txt\<open>\noindent
The proof is by rule induction. Because of the form of the induction theorem,
it is applied by \<open>rule\<close> rather than \<open>erule\<close> as for ordinary
inductive definitions:
\<close>

apply(rule Even_Odd.induct)

txt\<open>
@{subgoals[display,indent=0]}
The first two subgoals are proved by simplification and the final one can be
proved in the same manner as in \S\ref{sec:rule-induction}
where the same subgoal was encountered before.
We do not show the proof script.
\<close>
(*<*)
  apply simp
 apply simp
apply(simp add: dvd_def)
apply(clarify)
apply(rule_tac x = "Suc k" in exI)
apply simp
done
(*>*)

subsection\<open>Inductively Defined Predicates\label{sec:ind-predicates}\<close>

text\<open>\index{inductive predicates|(}
Instead of a set of even numbers one can also define a predicate on \<^typ>\<open>nat\<close>:
\<close>

inductive evn :: "nat \<Rightarrow> bool" where
zero: "evn 0" |
step: "evn n \<Longrightarrow> evn(Suc(Suc n))"

text\<open>\noindent Everything works as before, except that
you write \commdx{inductive} instead of \isacommand{inductive\_set} and
\<^prop>\<open>evn n\<close> instead of \<^prop>\<open>n \<in> Even\<close>.
When defining an n-ary relation as a predicate, it is recommended to curry
the predicate: its type should be \mbox{\<open>\<tau>\<^sub>1 \<Rightarrow> \<dots> \<Rightarrow> \<tau>\<^sub>n \<Rightarrow> bool\<close>}
rather than
\<open>\<tau>\<^sub>1 \<times> \<dots> \<times> \<tau>\<^sub>n \<Rightarrow> bool\<close>. The curried version facilitates inductions.

When should you choose sets and when predicates? If you intend to combine your notion with set theoretic notation, define it as an inductive set. If not, define it as an inductive predicate, thus avoiding the \<open>\<in>\<close> notation. But note that predicates of more than one argument cannot be combined with the usual set theoretic operators: \<^term>\<open>P \<union> Q\<close> is not well-typed if \<open>P, Q :: \<tau>\<^sub>1 \<Rightarrow> \<tau>\<^sub>2 \<Rightarrow> bool\<close>, you have to write \<^term>\<open>%x y. P x y & Q x y\<close> instead.
\index{inductive predicates|)}
\<close>

(*<*)end(*>*)
