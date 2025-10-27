theory Typeclass_Hierarchy
imports Setup
begin

section \<open>Introduction\<close>

text \<open>
  The {Isabelle/HOL} type-class hierarchy entered the stage
  in a quite ancient era -- first related \<^emph>\<open>NEWS\<close> entries date
  back to release {Isabelle99-1}.  Since then, there have been
  ongoing modifications and additions, leading to ({Isabelle2016})
  more than 180 type-classes.  This sheer complexity makes access
  and understanding of that type-class hierarchy difficult and
  involved, let alone maintenance.

  The purpose of this primer is to shed some light on this,
  not only on the mere ingredients, but also on the design
  principles which have evolved and proven useful over time.
\<close>

section \<open>Foundations\<close>

subsection \<open>Locales and type classes\<close>

text \<open>
  Some familiarity with the Isabelle module system is assumed:
  defining locales and type-classes, interpreting locales and
  instantiating type-classes, adding relationships between
  locales (@{command sublocale}) and type-classes
  (@{command subclass}).  Handy introductions are the
  respective tutorials \<^cite>\<open>"isabelle-locale"\<close>
  \<^cite>\<open>"isabelle-classes"\<close>.
\<close>

subsection \<open>Strengths and restrictions of type classes\<close>

text \<open>
  The primary motivation for using type classes in {Isabelle/HOL}
  always have been numerical types, which form an inclusion chain:
  
  \begin{center}
    \<^typ>\<open>nat\<close> \<open>\<sqsubset>\<close> \<^typ>\<open>int\<close> \<open>\<sqsubset>\<close> \<^typ>\<open>rat\<close>
      \<open>\<sqsubset>\<close> \<^typ>\<open>real\<close> \<open>\<sqsubset>\<close> \<^typ>\<open>complex\<close>
  \end{center}

  \noindent The inclusion \<open>\<sqsubset>\<close> means that any value of the numerical
  type to the left hand side mathematically can be transferred
  to the numerical type on the right hand side.

  How to accomplish this given the quite restrictive type system
  of {Isabelle/HOL}?  Paulson \<^cite>\<open>"paulson-numerical"\<close> explains
  that each numerical type has some characteristic properties
  which define an characteristic algebraic structure;  \<open>\<sqsubset>\<close>
  then corresponds to specialization of the corresponding
  characteristic algebraic structures.  These algebraic structures
  are expressed using algebraic type classes and embeddings
  of numerical types into them:

  \begin{center}\begin{tabular}{lccc}
    \<^term>\<open>of_nat\<close> \<open>::\<close>  & \<^typ>\<open>nat\<close>  & \<open>\<Rightarrow>\<close> & @{typ [source] "'a::semiring_1"} \\
                                 & \<open>\<sqinter>\<close>   &            & \<open>\<up>\<close> \\
    \<^term>\<open>of_int\<close> \<open>::\<close>  & \<^typ>\<open>int\<close>  & \<open>\<Rightarrow>\<close> & @{typ [source] "'a::ring_1"} \\
                                 & \<open>\<sqinter>\<close>   &            & \<open>\<up>\<close> \\
    \<^term>\<open>of_rat\<close> \<open>::\<close>  & \<^typ>\<open>rat\<close>  & \<open>\<Rightarrow>\<close> & @{typ [source] "'a::field_char_0"} \\
                                 & \<open>\<sqinter>\<close>   &            & \<open>\<up>\<close> \\
    \<^term>\<open>of_real\<close> \<open>::\<close> & \<^typ>\<open>real\<close> & \<open>\<Rightarrow>\<close> & @{typ [source] "'a::real_algebra_1"} \\
                                 & \<open>\<sqinter>\<close> \\
                                 & \<^typ>\<open>complex\<close>
  \end{tabular}\end{center}

  \noindent \<open>d \<leftarrow> c\<close> means that \<open>c\<close> is subclass of \<open>d\<close>.
  Hence each characteristic embedding \<open>of_num\<close> can transform
  any value of type \<open>num\<close> to any numerical type further
  up in the inclusion chain.

  This canonical example exhibits key strengths of type classes:

    \<^item> Sharing of operations and facts among different
      types, hence also sharing of notation and names: there
      is only one plus operation using infix syntax \<open>+\<close>,
      only one zero written \<open>0\<close>, and neutrality
      (@{thm (frugal_sorts) add_0_left [all, no_vars]} and
      @{thm (frugal_sorts) add_0_right [all, no_vars]})
      is referred to
      uniformly by names @{fact add_0_left} and @{fact add_0_right}.

    \<^item> Proof tool setups are shared implicitly:
      @{fact add_0} and @{fact add_0_right} are simplification
      rules by default.

    \<^item> Hence existing proofs about particular numerical
      types are often easy to generalize to algebraic structures,
      given that they do not depend on specific properties of
      those numerical types.

  Considerable restrictions include:

    \<^item> Type class operations are restricted to one
      type parameter; this is insufficient e.g. for expressing
      a unified power operation adequately (see \secref{sec:power}).

    \<^item> Parameters are fixed over the whole type class
      hierarchy and cannot be refined in specific situations:
      think of integral domains with a predicate \<^term>\<open>is_unit\<close>;
      for natural numbers, this degenerates to the much simpler
      @{term [source] "HOL.equal (1::nat)"} but facts
      refer to \<^term>\<open>is_unit\<close> nonetheless.

    \<^item> Type classes are not apt for meta-theory.  There
      is no practically usable way to express that the units
      of an integral domain form a multiplicative group using
      type classes.  But see session \<open>HOL-Algebra\<close>
      which provides locales with an explicit carrier.
\<close>


subsection \<open>Navigating the hierarchy\<close>

text \<open>
  An indispensable tool to inspect the class hierarchy is
  @{command class_deps} which displays the graph of classes,
  optionally showing the logical content for each class also.
  Optional parameters restrict the graph to a particular segment
  which is useful to get a graspable view.  See
  the Isar reference manual \<^cite>\<open>"isabelle-isar-ref"\<close> for details.
\<close>


section \<open>The hierarchy\<close>

subsection \<open>Syntactic type classes \label{sec:syntactic-type-class}\<close>

text \<open>
  At the top of the hierarchy there are a couple of syntactic type
  classes, ie. classes with operations but with no axioms,
  most notably:

    \<^item> @{command class} \<^class>\<open>plus\<close> with @{term [source] "(a::'a::plus) + b"}

    \<^item> @{command class} \<^class>\<open>zero\<close> with @{term [source] "0::'a::zero"}

    \<^item> @{command class} \<^class>\<open>times\<close> with @{term [source] "(a::'a::times) * b"}

    \<^item> @{command class} \<^class>\<open>one\<close> with @{term [source] "1::'a::one"}

  \noindent Before the introduction of the @{command class} statement in
  Isabelle \<^cite>\<open>"Haftmann-Wenzel:2006:classes"\<close> it was impossible
  to define operations with associated axioms in the same class,
  hence there were always pairs of syntactic and logical type
  classes.  This restriction is lifted nowadays, but there are
  still reasons to maintain syntactic type classes:

    \<^item> Syntactic type classes allow generic notation to be used
      regardless of a particular logical interpretation; e.g.
      although multiplication \<open>*\<close> is usually associative,
      there are examples where it is not (e.g. octonions), and
      leaving \<open>*\<close> without axioms allows to re-use this
      syntax by means of type class instantiation also for such
      exotic examples.

    \<^item> Type classes might share operations but not necessarily
      axioms on them, e.g. \<^term>\<open>gcd\<close> (see \secref{sec:gcd}).
      Hence their common base is a syntactic type class.

  \noindent However syntactic type classes should only be used with striking
  cause.  Otherwise there is risk for confusion if the notation
  suggests properties which do not hold without particular
  constraints.  This can be illustrated using numerals
  (see \secref{sec:numerals}):  @{lemma "2 + 2 = 4" by simp} is
  provable without further ado, and this also meets the typical
  expectation towards a numeral notation;  in more ancient releases
  numerals were purely syntactic and \<^prop>\<open>2 + 2 = 4\<close> was
  not provable without particular type constraints.
\<close>


subsection \<open>Additive and multiplicative semigroups and monoids\<close>

text \<open>
  In common literature, notation for semigroups and monoids
  is either multiplicative \<open>(*, 1)\<close> or additive
  \<open>(+, 0)\<close> with underlying properties isomorphic.
  In {Isabelle/HOL}, this is accomplished using the following
  abstract setup:

    \<^item> A \<^locale>\<open>semigroup\<close> introduces an abstract binary
      associative operation.

    \<^item> A \<^locale>\<open>monoid\<close> is an extension of \<^locale>\<open>semigroup\<close>
      with a neutral element.

    \<^item> Both \<^locale>\<open>semigroup\<close> and \<^locale>\<open>monoid\<close> provide
      dedicated syntax for their operations \<open>(\<^bold>*, \<^bold>1)\<close>.
      This syntax is not visible on the global theory level
      but only for abstract reasoning inside the respective
      locale.

    \<^item> Concrete global syntax is added building on existing
      syntactic type classes \secref{sec:syntactic-type-class}
      using the following classes:

        \<^item> @{command class} \<^class>\<open>semigroup_mult\<close> = \<^class>\<open>times\<close>

        \<^item> @{command class} \<^class>\<open>monoid_mult\<close> = \<^class>\<open>one\<close> + \<^class>\<open>semigroup_mult\<close>

        \<^item> @{command class} \<^class>\<open>semigroup_add\<close> = \<^class>\<open>plus\<close>

        \<^item> @{command class} \<^class>\<open>monoid_add\<close> = \<^class>\<open>zero\<close> + \<^class>\<open>semigroup_add\<close>

      Locales \<^locale>\<open>semigroup\<close> and \<^locale>\<open>monoid\<close> are
      interpreted (using @{command sublocale}) into their
      corresponding type classes, with prefixes \<open>add\<close>
      and \<open>mult\<close>; hence facts derived in \<^locale>\<open>semigroup\<close>
      and \<^locale>\<open>monoid\<close> are propagated simultaneously to
      \<^emph>\<open>both\<close> using a consistent naming policy, ie.

        \<^item> @{fact semigroup.assoc}: @{thm (frugal_sorts) semigroup.assoc [all, no_vars]}

        \<^item> @{fact mult.assoc}: @{thm (frugal_sorts) mult.assoc [all, no_vars]}

        \<^item> @{fact add.assoc}: @{thm (frugal_sorts) add.assoc [all, no_vars]}

        \<^item> @{fact monoid.right_neutral}: @{thm (frugal_sorts) monoid.right_neutral [all, no_vars]}

        \<^item> @{fact mult.right_neutral}: @{thm (frugal_sorts) mult.right_neutral [all, no_vars]}

        \<^item> @{fact add.right_neutral}: @{thm (frugal_sorts) add.right_neutral [all, no_vars]}

    \<^item> Note that the syntax in \<^locale>\<open>semigroup\<close> and \<^locale>\<open>monoid\<close>
      is bold; this avoids clashes when writing properties
      inside one of these locales in presence of that global
      concrete type class syntax.

  \noindent That hierarchy extends in a straightforward manner
  to abelian semigroups and commutative monoids\footnote{The
  designation \<^emph>\<open>abelian\<close> is quite standard concerning
  (semi)groups, but not for monoids}:

    \<^item> Locales \<^locale>\<open>abel_semigroup\<close> and \<^locale>\<open>comm_monoid\<close>
      add commutativity as property.

    \<^item> Concrete syntax emerges through

        \<^item> @{command class} \<^class>\<open>ab_semigroup_add\<close> = \<^class>\<open>semigroup_add\<close>

        \<^item> @{command class} \<^class>\<open>ab_semigroup_mult\<close> = \<^class>\<open>semigroup_mult\<close>

        \<^item> @{command class} \<^class>\<open>comm_monoid_add\<close> = \<^class>\<open>zero\<close> + \<^class>\<open>ab_semigroup_add\<close>

        \<^item> @{command class} \<^class>\<open>comm_monoid_mult\<close> = \<^class>\<open>one\<close> + \<^class>\<open>ab_semigroup_mult\<close>
  
      and corresponding interpretation of the locales above, yielding

        \<^item> @{fact abel_semigroup.commute}: @{thm (frugal_sorts) abel_semigroup.commute [all, no_vars]}

        \<^item> @{fact mult.commute}: @{thm (frugal_sorts) mult.commute [all, no_vars]}

        \<^item> @{fact add.commute}: @{thm (frugal_sorts) add.commute [all, no_vars]}
\<close>

paragraph \<open>Named collection of theorems\<close>

text \<open>
  Locale interpretation interacts smoothly with named collections of
  theorems as introduced by command @{command named_theorems}.  In our
  example, rules concerning associativity and commutativity are no
  simplification rules by default since they desired orientation may
  vary depending on the situation.  However, there is a collection
  @{fact ac_simps} where facts @{fact abel_semigroup.assoc},
  @{fact abel_semigroup.commute} and @{fact abel_semigroup.left_commute}
  are declared as members.  Due to interpretation, also
  @{fact mult.assoc}, @{fact mult.commute} and @{fact mult.left_commute}
  are also members of @{fact ac_simps}, as any corresponding facts
  stemming from interpretation of \<^locale>\<open>abel_semigroup\<close>.
  Hence adding @{fact ac_simps} to the simplification rules for
  a single method call uses all associativity and commutativity
  rules known by means of interpretation.
\<close>


subsection \<open>Additive and multiplicative groups\<close>

text \<open>
  The hierarchy for inverse group operations takes into account
  that there are weaker algebraic structures with only a partially
  inverse operation.  E. g. the natural numbers have bounded
  subtraction \<^term>\<open>m - (n::nat)\<close> which is only an inverse
  operation if \<^term>\<open>m \<ge> (n::nat)\<close>;  unary minus \<open>-\<close>
  is pointless on the natural numbers.

  Hence for both additive and multiplicative notation there
  are syntactic classes for inverse operations, both unary
  and binary:

    \<^item> @{command class} \<^class>\<open>minus\<close> with @{term [source] "(a::'a::minus) - b"}

    \<^item> @{command class} \<^class>\<open>uminus\<close> with @{term [source] "- a::'a::uminus"}

    \<^item> @{command class} \<^class>\<open>divide\<close> with @{term [source] "(a::'a::divide) div b"}

    \<^item> @{command class} \<^class>\<open>inverse\<close> = \<^class>\<open>divide\<close> with @{term [source] "inverse a::'a::inverse"}
        \\ and @{term [source] "(a::'a::inverse) / b"}

  \noindent Here \<^class>\<open>inverse\<close> specializes the ``partial'' syntax
  @{term [source] "a div b"} to the more specific
  @{term [source] "a / b"}. 

  Semantic properties are added by

    \<^item> @{command class} \<^class>\<open>cancel_ab_semigroup_add\<close> = \<^class>\<open>ab_semigroup_add\<close> + \<^class>\<open>minus\<close>

    \<^item> @{command class} \<^class>\<open>cancel_comm_monoid_add\<close> = \<^class>\<open>cancel_ab_semigroup_add\<close> + \<^class>\<open>comm_monoid_add\<close>

  \noindent which specify a minimal binary partially inverse operation as

    \<^item> @{fact add_diff_cancel_left'}: @{thm (frugal_sorts) add_diff_cancel_left' [all, no_vars]}

    \<^item> @{fact diff_diff_add}: @{thm (frugal_sorts) diff_diff_add [all, no_vars]}

  \noindent which in turn allow to derive facts like

    \<^item> @{fact add_left_imp_eq}: @{thm (frugal_sorts) add_left_imp_eq [all, no_vars]}

  \noindent The total inverse operation is established as follows:

    \<^item> Locale \<^locale>\<open>group\<close> extends the abstract hierarchy with
      the inverse operation.

    \<^item> The concrete additive inverse operation emerges through

      \<^item> @{command class} \<^class>\<open>group_add\<close> = \<^class>\<open>minus\<close> + \<^class>\<open>uminus\<close> + \<^class>\<open>monoid_add\<close> (in \<^theory>\<open>HOL.Groups\<close>) \\

      \<^item> @{command class} \<^class>\<open>ab_group_add\<close> = \<^class>\<open>minus\<close> + \<^class>\<open>uminus\<close> + \<^class>\<open>comm_monoid_add\<close> (in \<^theory>\<open>HOL.Groups\<close>)

      and corresponding interpretation of locale \<^locale>\<open>group\<close>, yielding e.g.

      \<^item> @{fact group.left_inverse}: @{thm (frugal_sorts) group.left_inverse [all, no_vars]}

      \<^item> @{fact add.left_inverse}: @{thm (frugal_sorts) add.left_inverse [all, no_vars]}

  \noindent There is no multiplicative counterpart.  Why?  In rings,
  the multiplicative group excludes the zero element, hence
  the inverse operation is not total.  See further \secref{sec:rings}.
\<close>

paragraph \<open>Mitigating against redundancy by default simplification rules\<close>

text \<open>
  Inverse operations imposes some redundancy on the type class
  hierarchy: in a group with a total inverse operation, the
  unary operation is simpler and more primitive than the binary
  one; but we cannot eliminate the binary one in favour of
  a mere syntactic abbreviation since the binary one is vital
  to express a partial inverse operation.

  This is mitigated by providing suitable default simplification
  rules: expression involving the unary inverse operation are
  simplified to binary inverse operation whenever appropriate.
  The rationale is that simplification is a central device in
  explorative proving, where proof obligation remaining after certain
  default proof steps including simplification are inspected
  to get an idea what is missing to finish a proof.  When
  preferable normal forms are encoded into
  default simplification rules, proof obligations after simplification
  are normalized and hence more proof-friendly.
\<close>

end
