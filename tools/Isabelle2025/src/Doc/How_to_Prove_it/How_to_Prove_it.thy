(*<*)
theory How_to_Prove_it
imports Complex_Main
begin
(*>*)
text\<open>
\chapter{\<^theory>\<open>Main\<close>}

\section{Natural numbers}

%Tobias Nipkow
\paragraph{Induction rules}~\\
In addition to structural induction there is the induction rule
@{thm[source] less_induct}:
\begin{quote}
@{thm less_induct}
\end{quote}
This is often called ``complete induction''. It is applied like this:
\begin{quote}
(\<open>induction n rule: less_induct\<close>)
\end{quote}
In fact, it is not restricted to \<^typ>\<open>nat\<close> but works for any wellfounded
order \<open><\<close>.

There are many more special induction rules. You can find all of them
via the Find button (in Isabelle/jedit) with the following search criteria:
\begin{quote}
\texttt{name: Nat name: induct}
\end{quote}


\paragraph{How to convert numerals into \<^const>\<open>Suc\<close> terms}~\\
Solution: simplify with the lemma @{thm[source] numeral_eq_Suc}.

\noindent
Example:
\<close>

lemma fixes x :: int shows "x ^ 3 = x * x * x"
by (simp add: numeral_eq_Suc)

text\<open>This is a typical situation: function ``\<open>^\<close>'' is defined
by pattern matching on \<^const>\<open>Suc\<close> but is applied to a numeral.

Note: simplification with @{thm[source] numeral_eq_Suc} will convert all numerals.
One can be more specific with the lemmas @{thm [source] numeral_2_eq_2}
(@{thm numeral_2_eq_2}) and @{thm[source] numeral_3_eq_3} (@{thm numeral_3_eq_3}).


\section{Lists}

%Tobias Nipkow
\paragraph{Induction rules}~\\
In addition to structural induction there are a few more induction rules
that come in handy at times:
\begin{itemize}
\item
Structural induction where the new element is appended to the end
of the list (@{thm[source] rev_induct}):

@{thm rev_induct}

\item Induction on the length of a list (@{thm [source] length_induct}):

@{thm length_induct}

\item Simultaneous induction on two lists of the same length (@{thm [source] list_induct2}):

@{thm[display,margin=60] list_induct2}

\end{itemize}

%Tobias Nipkow
\section{Algebraic simplification}

On the numeric types \<^typ>\<open>nat\<close>, \<^typ>\<open>int\<close> and \<^typ>\<open>real\<close>,
proof method \<open>simp\<close> and friends can deal with a limited amount of linear
arithmetic (no multiplication except by numerals) and method \<open>arith\<close> can
handle full linear arithmetic (on \<^typ>\<open>nat\<close>, \<^typ>\<open>int\<close> including quantifiers).
But what to do when proper multiplication is involved?
At this point it can be helpful to simplify with the lemma list
@{thm [source] algebra_simps}. Examples:
\<close>

lemma fixes x :: int
  shows "(x + y) * (y - z) = (y - z) * x + y * (y-z)"
by(simp add: algebra_simps)

lemma fixes x :: "'a :: comm_ring"
  shows "(x + y) * (y - z) = (y - z) * x + y * (y-z)"
by(simp add: algebra_simps)

text\<open>
Rewriting with @{thm[source] algebra_simps} has the following effect:
terms are rewritten into a normal form by multiplying out,
rearranging sums and products into some canonical order.
In the above lemma the normal form will be something like
\<^term>\<open>x*y + y*y - x*z - y*z\<close>.
This works for concrete types like \<^typ>\<open>int\<close> as well as for classes like
\<^class>\<open>comm_ring\<close> (commutative rings). For some classes (e.g.\ \<^class>\<open>ring\<close>
and \<^class>\<open>comm_ring\<close>) this yields a decision procedure for equality.

Additional function and predicate symbols are not a problem either:
\<close>

lemma fixes f :: "int \<Rightarrow> int" shows "2 * f(x*y) - f(y*x) < f(y*x) + 1"
by(simp add: algebra_simps)

text\<open>Here @{thm[source]algebra_simps} merely has the effect of rewriting
\<^term>\<open>y*x\<close> to \<^term>\<open>x*y\<close> (or the other way around). This yields
a problem of the form \<^prop>\<open>2*t - t < t + (1::int)\<close> and we are back in the
realm of linear arithmetic.

Because @{thm[source]algebra_simps} multiplies out, terms can explode.
If one merely wants to bring sums or products into a canonical order
it suffices to rewrite with @{thm [source] ac_simps}:\<close>

lemma fixes f :: "int \<Rightarrow> int" shows "f(x*y*z) - f(z*x*y) = 0"
by(simp add: ac_simps)

text\<open>The lemmas @{thm[source]algebra_simps} take care of addition, subtraction
and multiplication (algebraic structures up to rings) but ignore division (fields).
The lemmas @{thm[source]field_simps} also deal with division:
\<close>

lemma fixes x :: real shows "x+z \<noteq> 0 \<Longrightarrow> 1 + y/(x+z) = (x+y+z)/(x+z)"
by(simp add: field_simps)

text\<open>Warning: @{thm[source]field_simps} can blow up your terms
beyond recognition.\<close>

(*<*)
end
(*>*)
