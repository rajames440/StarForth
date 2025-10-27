section\<open>The binary product topology\<close>

theory Product_Topology
  imports Function_Topology
begin

section \<open>Product Topology\<close>

subsection\<open>Definition\<close>

definition prod_topology :: "'a topology \<Rightarrow> 'b topology \<Rightarrow> ('a \<times> 'b) topology" where
 "prod_topology X Y \<equiv> topology (arbitrary union_of (\<lambda>U. U \<in> {S \<times> T |S T. openin X S \<and> openin Y T}))"

lemma open_product_open:
  assumes "open A"
  shows "\<exists>\<U>. \<U> \<subseteq> {S \<times> T |S T. open S \<and> open T} \<and> \<Union> \<U> = A"
proof -
  obtain f g where *: "\<And>u. u \<in> A \<Longrightarrow> open (f u) \<and> open (g u) \<and> u \<in> (f u) \<times> (g u) \<and> (f u) \<times> (g u) \<subseteq> A"
    using open_prod_def [of A] assms by metis
  let ?\<U> = "(\<lambda>u. f u \<times> g u) ` A"
  show ?thesis
    by (rule_tac x="?\<U>" in exI) (auto simp: dest: *)
qed

lemma open_product_open_eq: "(arbitrary union_of (\<lambda>U. \<exists>S T. U = S \<times> T \<and> open S \<and> open T)) = open"
  by (force simp: union_of_def arbitrary_def intro: open_product_open open_Times)

lemma openin_prod_topology:
   "openin (prod_topology X Y) = arbitrary union_of (\<lambda>U. U \<in> {S \<times> T |S T. openin X S \<and> openin Y T})"
  unfolding prod_topology_def
proof (rule topology_inverse')
  show "istopology (arbitrary union_of (\<lambda>U. U \<in> {S \<times> T |S T. openin X S \<and> openin Y T}))"
    apply (rule istopology_base, simp)
    by (metis openin_Int Times_Int_Times)
qed

lemma topspace_prod_topology [simp]:
   "topspace (prod_topology X Y) = topspace X \<times> topspace Y"
proof -
  have "topspace(prod_topology X Y) = \<Union> (Collect (openin (prod_topology X Y)))" (is "_ = ?Z")
    unfolding topspace_def ..
  also have "\<dots> = topspace X \<times> topspace Y"
  proof
    show "?Z \<subseteq> topspace X \<times> topspace Y"
      apply (auto simp: openin_prod_topology union_of_def arbitrary_def)
      using openin_subset by force+
  next
    have *: "\<exists>A B. topspace X \<times> topspace Y = A \<times> B \<and> openin X A \<and> openin Y B"
      by blast
    show "topspace X \<times> topspace Y \<subseteq> ?Z"
      apply (rule Union_upper)
      using * by (simp add: openin_prod_topology arbitrary_union_of_inc)
  qed
  finally show ?thesis .
qed

lemma prod_topology_trivial_iff [simp]:
  "prod_topology X Y = trivial_topology \<longleftrightarrow> X = trivial_topology \<or> Y = trivial_topology"
  by (metis (full_types) Sigma_empty1 null_topspace_iff_trivial subset_empty times_subset_iff topspace_prod_topology)

lemma subtopology_Times:
  shows "subtopology (prod_topology X Y) (S \<times> T) = prod_topology (subtopology X S) (subtopology Y T)"
proof -
  have "((\<lambda>U. \<exists>S T. U = S \<times> T \<and> openin X S \<and> openin Y T) relative_to S \<times> T) =
        (\<lambda>U. \<exists>S' T'. U = S' \<times> T' \<and> (openin X relative_to S) S' \<and> (openin Y relative_to T) T')"
    by (auto simp: relative_to_def Times_Int_Times fun_eq_iff) metis
  then show ?thesis
    by (simp add: topology_eq openin_prod_topology arbitrary_union_of_relative_to flip: openin_relative_to)
qed

lemma prod_topology_subtopology:
    "prod_topology (subtopology X S) Y = subtopology (prod_topology X Y) (S \<times> topspace Y)"
    "prod_topology X (subtopology Y T) = subtopology (prod_topology X Y) (topspace X \<times> T)"
by (auto simp: subtopology_Times)

lemma prod_topology_discrete_topology:
     "discrete_topology (S \<times> T) = prod_topology (discrete_topology S) (discrete_topology T)"
  by (auto simp: discrete_topology_unique openin_prod_topology intro: arbitrary_union_of_inc)

lemma prod_topology_euclidean [simp]: "prod_topology euclidean euclidean = euclidean"
  by (simp add: prod_topology_def open_product_open_eq)

lemma prod_topology_subtopology_eu [simp]:
  "prod_topology (subtopology euclidean S) (subtopology euclidean T) = subtopology euclidean (S \<times> T)"
  by (simp add: prod_topology_subtopology subtopology_subtopology Times_Int_Times)

lemma openin_prod_topology_alt:
     "openin (prod_topology X Y) S \<longleftrightarrow>
      (\<forall>x y. (x,y) \<in> S \<longrightarrow> (\<exists>U V. openin X U \<and> openin Y V \<and> x \<in> U \<and> y \<in> V \<and> U \<times> V \<subseteq> S))"
  apply (auto simp: openin_prod_topology arbitrary_union_of_alt, fastforce)
  by (metis mem_Sigma_iff)

lemma open_map_fst: "open_map (prod_topology X Y) X fst"
  unfolding open_map_def openin_prod_topology_alt
  by (force simp: openin_subopen [of X "fst ` _"] intro: subset_fst_imageI)

lemma open_map_snd: "open_map (prod_topology X Y) Y snd"
  unfolding open_map_def openin_prod_topology_alt
  by (force simp: openin_subopen [of Y "snd ` _"] intro: subset_snd_imageI)

lemma openin_prod_Times_iff:
     "openin (prod_topology X Y) (S \<times> T) \<longleftrightarrow> S = {} \<or> T = {} \<or> openin X S \<and> openin Y T"
proof (cases "S = {} \<or> T = {}")
  case False
  then show ?thesis
    apply (simp add: openin_prod_topology_alt openin_subopen [of X S] openin_subopen [of Y T] times_subset_iff, safe)
      apply (meson|force)+
    done
qed force


lemma closure_of_Times:
   "(prod_topology X Y) closure_of (S \<times> T) = (X closure_of S) \<times> (Y closure_of T)"  (is "?lhs = ?rhs")
proof
  show "?lhs \<subseteq> ?rhs"
    by (clarsimp simp: closure_of_def openin_prod_topology_alt) blast
  show "?rhs \<subseteq> ?lhs"
    by (clarsimp simp: closure_of_def openin_prod_topology_alt) (meson SigmaI subsetD)
qed

lemma closedin_prod_Times_iff:
   "closedin (prod_topology X Y) (S \<times> T) \<longleftrightarrow> S = {} \<or> T = {} \<or> closedin X S \<and> closedin Y T"
  by (auto simp: closure_of_Times times_eq_iff simp flip: closure_of_eq)

lemma interior_of_Times: "(prod_topology X Y) interior_of (S \<times> T) = (X interior_of S) \<times> (Y interior_of T)"
proof (rule interior_of_unique)
  show "(X interior_of S) \<times> Y interior_of T \<subseteq> S \<times> T"
    by (simp add: Sigma_mono interior_of_subset)
  show "openin (prod_topology X Y) ((X interior_of S) \<times> Y interior_of T)"
    by (simp add: openin_prod_Times_iff)
next
  show "T' \<subseteq> (X interior_of S) \<times> Y interior_of T" if "T' \<subseteq> S \<times> T" "openin (prod_topology X Y) T'" for T'
  proof (clarsimp; intro conjI)
    fix a :: "'a" and b :: "'b"
    assume "(a, b) \<in> T'"
    with that obtain U V where UV: "openin X U" "openin Y V" "a \<in> U" "b \<in> V" "U \<times> V \<subseteq> T'"
      by (metis openin_prod_topology_alt)
    then show "a \<in> X interior_of S"
      using interior_of_maximal_eq that(1) by fastforce
    show "b \<in> Y interior_of T"
      using UV interior_of_maximal_eq that(1)
      by (metis SigmaI mem_Sigma_iff subset_eq)
  qed
qed

text \<open>Missing the opposite direction. Does it hold? A converse is proved for proper maps, a stronger condition\<close>
lemma closed_map_prod:
  assumes "closed_map (prod_topology X Y) (prod_topology X' Y') (\<lambda>(x,y). (f x, g y))"
  shows "(prod_topology X Y) = trivial_topology \<or> closed_map X X' f \<and> closed_map Y Y' g"
proof (cases "(prod_topology X Y) = trivial_topology")
  case False
  then have ne: "topspace X \<noteq> {}" "topspace Y \<noteq> {}"
    by (auto simp flip: null_topspace_iff_trivial)
  have "closed_map X X' f"
    unfolding closed_map_def
  proof (intro strip)
    fix C
    assume "closedin X C"
    show "closedin X' (f ` C)"
    proof (cases "C={}")
      case False
      with assms have "closedin (prod_topology X' Y') ((\<lambda>(x,y). (f x, g y)) ` (C \<times> topspace Y))"
        by (simp add: \<open>closedin X C\<close> closed_map_def closedin_prod_Times_iff)
      with False ne show ?thesis
        by (simp add: image_paired_Times closedin_Times closedin_prod_Times_iff)
    qed auto
  qed
  moreover
  have "closed_map Y Y' g"
    unfolding closed_map_def
  proof (intro strip)
    fix C
    assume "closedin Y C"
    show "closedin Y' (g ` C)"
    proof (cases "C={}")
      case False
      with assms have "closedin (prod_topology X' Y') ((\<lambda>(x,y). (f x, g y)) ` (topspace X \<times> C))"
        by (simp add: \<open>closedin Y C\<close> closed_map_def closedin_prod_Times_iff)
      with False ne show ?thesis
        by (simp add: image_paired_Times closedin_Times closedin_prod_Times_iff)
    qed auto
  qed
  ultimately show ?thesis
    by (auto simp: False)
qed auto

subsection \<open>Continuity\<close>

lemma continuous_map_pairwise:
   "continuous_map Z (prod_topology X Y) f \<longleftrightarrow> continuous_map Z X (fst \<circ> f) \<and> continuous_map Z Y (snd \<circ> f)"
   (is "?lhs = ?rhs")
proof -
  let ?g = "fst \<circ> f" and ?h = "snd \<circ> f"
  have f: "f x = (?g x, ?h x)" for x
    by auto
  show ?thesis
  proof (cases "?g \<in> topspace Z \<rightarrow> topspace X \<and> ?h \<in> topspace Z \<rightarrow> topspace Y")
    case True
    show ?thesis
    proof safe
      assume "continuous_map Z (prod_topology X Y) f"
      then have "openin Z {x \<in> topspace Z. fst (f x) \<in> U}" if "openin X U" for U
        unfolding continuous_map_def using True that
        apply clarify
        apply (drule_tac x="U \<times> topspace Y" in spec)
        by (auto simp: openin_prod_Times_iff mem_Times_iff Pi_iff cong: conj_cong)
      with True show "continuous_map Z X (fst \<circ> f)"
        by (auto simp: continuous_map_def)
    next
      assume "continuous_map Z (prod_topology X Y) f"
      then have "openin Z {x \<in> topspace Z. snd (f x) \<in> V}" if "openin Y V" for V
        unfolding continuous_map_def using True that
        apply clarify
        apply (drule_tac x="topspace X \<times> V" in spec)
        by (simp add: openin_prod_Times_iff mem_Times_iff Pi_iff cong: conj_cong)
      with True show "continuous_map Z Y (snd \<circ> f)"
        by (auto simp: continuous_map_def)
    next
      assume Z: "continuous_map Z X (fst \<circ> f)" "continuous_map Z Y (snd \<circ> f)"
      have *: "openin Z {x \<in> topspace Z. f x \<in> W}"
        if "\<And>w. w \<in> W \<Longrightarrow> \<exists>U V. openin X U \<and> openin Y V \<and> w \<in> U \<times> V \<and> U \<times> V \<subseteq> W" for W
      proof (subst openin_subopen, clarify)
        fix x :: "'a"
        assume "x \<in> topspace Z" and "f x \<in> W"
        with that [OF \<open>f x \<in> W\<close>]
        obtain U V where UV: "openin X U" "openin Y V" "f x \<in> U \<times> V" "U \<times> V \<subseteq> W"
          by auto
        with Z  UV show "\<exists>T. openin Z T \<and> x \<in> T \<and> T \<subseteq> {x \<in> topspace Z. f x \<in> W}"
          apply (rule_tac x="{x \<in> topspace Z. ?g x \<in> U} \<inter> {x \<in> topspace Z. ?h x \<in> V}" in exI)
          apply (auto simp: \<open>x \<in> topspace Z\<close> continuous_map_def)
          done
      qed
      show "continuous_map Z (prod_topology X Y) f"
        using True by (force simp: continuous_map_def openin_prod_topology_alt mem_Times_iff *)
    qed
  qed (force simp: continuous_map_def)
qed

lemma continuous_map_paired:
   "continuous_map Z (prod_topology X Y) (\<lambda>x. (f x,g x)) \<longleftrightarrow> continuous_map Z X f \<and> continuous_map Z Y g"
  by (simp add: continuous_map_pairwise o_def)

lemma continuous_map_pairedI [continuous_intros]:
   "\<lbrakk>continuous_map Z X f; continuous_map Z Y g\<rbrakk> \<Longrightarrow> continuous_map Z (prod_topology X Y) (\<lambda>x. (f x,g x))"
  by (simp add: continuous_map_pairwise o_def)

lemma continuous_map_fst [continuous_intros]: "continuous_map (prod_topology X Y) X fst"
  using continuous_map_pairwise [of "prod_topology X Y" X Y id]
  by (simp add: continuous_map_pairwise)

lemma continuous_map_snd [continuous_intros]: "continuous_map (prod_topology X Y) Y snd"
  using continuous_map_pairwise [of "prod_topology X Y" X Y id]
  by (simp add: continuous_map_pairwise)

lemma continuous_map_fst_of [continuous_intros]:
   "continuous_map Z (prod_topology X Y) f \<Longrightarrow> continuous_map Z X (fst \<circ> f)"
  by (simp add: continuous_map_pairwise)

lemma continuous_map_snd_of [continuous_intros]:
   "continuous_map Z (prod_topology X Y) f \<Longrightarrow> continuous_map Z Y (snd \<circ> f)"
  by (simp add: continuous_map_pairwise)
    
lemma continuous_map_prod_fst: 
  "i \<in> I \<Longrightarrow> continuous_map (prod_topology (product_topology (\<lambda>i. Y) I) X) Y (\<lambda>x. fst x i)"
  using continuous_map_componentwise_UNIV continuous_map_fst by fastforce

lemma continuous_map_prod_snd: 
  "i \<in> I \<Longrightarrow> continuous_map (prod_topology X (product_topology (\<lambda>i. Y) I)) Y (\<lambda>x. snd x i)"
  using continuous_map_componentwise_UNIV continuous_map_snd by fastforce

lemma continuous_map_if_iff [simp]: "continuous_map X Y (\<lambda>x. if P then f x else g x) \<longleftrightarrow> continuous_map X Y (if P then f else g)"
  by simp

lemma continuous_map_if [continuous_intros]: "\<lbrakk>P \<Longrightarrow> continuous_map X Y f; ~P \<Longrightarrow> continuous_map X Y g\<rbrakk>
      \<Longrightarrow> continuous_map X Y (\<lambda>x. if P then f x else g x)"
  by simp

lemma prod_topology_trivial1 [simp]: "prod_topology trivial_topology Y = trivial_topology"
  using continuous_map_fst continuous_map_on_empty2 by blast

lemma prod_topology_trivial2 [simp]: "prod_topology X trivial_topology = trivial_topology"
  using continuous_map_snd continuous_map_on_empty2 by blast

lemma continuous_map_subtopology_fst [continuous_intros]: "continuous_map (subtopology (prod_topology X Y) Z) X fst"
      using continuous_map_from_subtopology continuous_map_fst by force

lemma continuous_map_subtopology_snd [continuous_intros]: "continuous_map (subtopology (prod_topology X Y) Z) Y snd"
      using continuous_map_from_subtopology continuous_map_snd by force

lemma quotient_map_fst [simp]:
   "quotient_map(prod_topology X Y) X fst \<longleftrightarrow> (Y = trivial_topology \<longrightarrow> X = trivial_topology)"
  apply (simp add: continuous_open_quotient_map open_map_fst continuous_map_fst)
  by (metis null_topspace_iff_trivial)

lemma quotient_map_snd [simp]:
   "quotient_map(prod_topology X Y) Y snd \<longleftrightarrow> (X = trivial_topology \<longrightarrow> Y = trivial_topology)"
  apply (simp add: continuous_open_quotient_map open_map_snd continuous_map_snd)
  by (metis null_topspace_iff_trivial)

lemma retraction_map_fst:
   "retraction_map (prod_topology X Y) X fst \<longleftrightarrow> (Y = trivial_topology \<longrightarrow> X = trivial_topology)"
proof (cases "Y = trivial_topology")
  case True
  then show ?thesis
    using continuous_map_image_subset_topspace
    by (auto simp: retraction_map_def retraction_maps_def continuous_map_pairwise)
next
  case False
  have "\<exists>g. continuous_map X (prod_topology X Y) g \<and> (\<forall>x\<in>topspace X. fst (g x) = x)"
    if y: "y \<in> topspace Y" for y
    by (rule_tac x="\<lambda>x. (x,y)" in exI) (auto simp: y continuous_map_paired)
  with False have "retraction_map (prod_topology X Y) X fst"
    by (fastforce simp: retraction_map_def retraction_maps_def continuous_map_fst)
  with False show ?thesis
    by simp
qed

lemma retraction_map_snd:
   "retraction_map (prod_topology X Y) Y snd \<longleftrightarrow> (X = trivial_topology \<longrightarrow> Y = trivial_topology)"
proof (cases "X = trivial_topology")
  case True
  then show ?thesis
    using continuous_map_image_subset_topspace
    by (fastforce simp: retraction_map_def retraction_maps_def continuous_map_fst)
next
  case False
  have "\<exists>g. continuous_map Y (prod_topology X Y) g \<and> (\<forall>y\<in>topspace Y. snd (g y) = y)"
    if x: "x \<in> topspace X" for x
    by (rule_tac x="\<lambda>y. (x,y)" in exI) (auto simp: x continuous_map_paired)
  with False have "retraction_map (prod_topology X Y) Y snd"
    by (fastforce simp: retraction_map_def retraction_maps_def continuous_map_snd)
  with False show ?thesis
    by simp
qed


lemma continuous_map_of_fst:
   "continuous_map (prod_topology X Y) Z (f \<circ> fst) \<longleftrightarrow> Y = trivial_topology \<or> continuous_map X Z f"
proof (cases "Y = trivial_topology")
  case True
  then show ?thesis
    by (simp add: continuous_map_on_empty)
next
  case False
  then show ?thesis
    by (simp add: continuous_compose_quotient_map_eq)
qed

lemma continuous_map_of_snd:
   "continuous_map (prod_topology X Y) Z (f \<circ> snd) \<longleftrightarrow> X = trivial_topology \<or> continuous_map Y Z f"
proof (cases "X = trivial_topology")
  case True
  then show ?thesis
    by (simp add: continuous_map_on_empty)
next
  case False
  then show ?thesis
    by (simp add: continuous_compose_quotient_map_eq)
qed

lemma continuous_map_prod_top:
   "continuous_map (prod_topology X Y) (prod_topology X' Y') (\<lambda>(x,y). (f x, g y)) \<longleftrightarrow>
    (prod_topology X Y) = trivial_topology \<or> continuous_map X X' f \<and> continuous_map Y Y' g"
proof (cases "(prod_topology X Y) = trivial_topology")
  case False
  then show ?thesis
    by (auto simp: continuous_map_paired case_prod_unfold 
               continuous_map_of_fst [unfolded o_def] continuous_map_of_snd [unfolded o_def])
qed auto

lemma in_prod_topology_closure_of:
  assumes  "z \<in> (prod_topology X Y) closure_of S"
  shows "fst z \<in> X closure_of (fst ` S)" "snd z \<in> Y closure_of (snd ` S)"
  using assms continuous_map_eq_image_closure_subset continuous_map_fst apply fastforce
  using assms continuous_map_eq_image_closure_subset continuous_map_snd apply fastforce
  done


proposition compact_space_prod_topology:
   "compact_space(prod_topology X Y) \<longleftrightarrow> (prod_topology X Y) = trivial_topology \<or> compact_space X \<and> compact_space Y"
proof (cases "(prod_topology X Y) = trivial_topology")
  case True
  then show ?thesis
    by fastforce
next
  case False
  then have non_mt: "topspace X \<noteq> {}" "topspace Y \<noteq> {}"
    by auto
  have "compact_space X" "compact_space Y" if "compact_space(prod_topology X Y)"
  proof -
    have "compactin X (fst ` (topspace X \<times> topspace Y))"
      by (metis compact_space_def continuous_map_fst image_compactin that topspace_prod_topology)
    moreover
    have "compactin Y (snd ` (topspace X \<times> topspace Y))"
      by (metis compact_space_def continuous_map_snd image_compactin that topspace_prod_topology)
    ultimately show "compact_space X" "compact_space Y"
      using non_mt by (auto simp: compact_space_def)
  qed
  moreover
  define \<X> where "\<X> \<equiv> (\<lambda>V. topspace X \<times> V) ` Collect (openin Y)"
  define \<Y> where "\<Y> \<equiv> (\<lambda>U. U \<times> topspace Y) ` Collect (openin X)"
  have "compact_space(prod_topology X Y)" if "compact_space X" "compact_space Y"
  proof (rule Alexander_subbase_alt)
    show toptop: "topspace X \<times> topspace Y \<subseteq> \<Union>(\<X> \<union> \<Y>)"
      unfolding \<X>_def \<Y>_def by auto
    fix \<C> :: "('a \<times> 'b) set set"
    assume \<C>: "\<C> \<subseteq> \<X> \<union> \<Y>" "topspace X \<times> topspace Y \<subseteq> \<Union>\<C>"
    then obtain \<X>' \<Y>' where XY: "\<X>' \<subseteq> \<X>" "\<Y>' \<subseteq> \<Y>" and \<C>eq: "\<C> = \<X>' \<union> \<Y>'"
      using subset_UnE by metis
    then have sub: "topspace X \<times> topspace Y \<subseteq> \<Union>(\<X>' \<union> \<Y>')"
      using \<C> by simp
    obtain \<U> \<V> where \<U>: "\<And>U. U \<in> \<U> \<Longrightarrow> openin X U" "\<Y>' = (\<lambda>U. U \<times> topspace Y) ` \<U>"
      and \<V>: "\<And>V. V \<in> \<V> \<Longrightarrow> openin Y V" "\<X>' = (\<lambda>V. topspace X \<times> V) ` \<V>"
      using XY by (clarsimp simp add: \<X>_def \<Y>_def subset_image_iff) (force simp: subset_iff)
    have "\<exists>\<D>. finite \<D> \<and> \<D> \<subseteq> \<X>' \<union> \<Y>' \<and> topspace X \<times> topspace Y \<subseteq> \<Union> \<D>"
    proof -
      have "topspace X \<subseteq> \<Union>\<U> \<or> topspace Y \<subseteq> \<Union>\<V>"
        using \<U> \<V> \<C> \<C>eq by auto
      then have *: "\<exists>\<D>. finite \<D> \<and>
               (\<forall>x \<in> \<D>. x \<in> (\<lambda>V. topspace X \<times> V) ` \<V> \<or> x \<in> (\<lambda>U. U \<times> topspace Y) ` \<U>) \<and>
               (topspace X \<times> topspace Y \<subseteq> \<Union>\<D>)"
      proof
        assume "topspace X \<subseteq> \<Union>\<U>"
        with \<open>compact_space X\<close> \<U> obtain \<F> where "finite \<F>" "\<F> \<subseteq> \<U>" "topspace X \<subseteq> \<Union>\<F>"
          by (meson compact_space_alt)
        with that show ?thesis
          by (rule_tac x="(\<lambda>D. D \<times> topspace Y) ` \<F>" in exI) auto
      next
        assume "topspace Y \<subseteq> \<Union>\<V>"
        with \<open>compact_space Y\<close> \<V> obtain \<F> where "finite \<F>" "\<F> \<subseteq> \<V>" "topspace Y \<subseteq> \<Union>\<F>"
          by (meson compact_space_alt)
        with that show ?thesis
          by (rule_tac x="(\<lambda>C. topspace X \<times> C) ` \<F>" in exI) auto
      qed
      then show ?thesis
        using that \<U> \<V> by blast
    qed
    then show "\<exists>\<D>. finite \<D> \<and> \<D> \<subseteq> \<C> \<and> topspace X \<times> topspace Y \<subseteq> \<Union> \<D>"
      using \<C> \<C>eq by blast
  next
    have "(finite intersection_of (\<lambda>x. x \<in> \<X> \<or> x \<in> \<Y>) relative_to topspace X \<times> topspace Y)
           = (\<lambda>U. \<exists>S T. U = S \<times> T \<and> openin X S \<and> openin Y T)"
      (is "?lhs = ?rhs")
    proof -
      have "?rhs U" if "?lhs U" for U
      proof -
        have "topspace X \<times> topspace Y \<inter> \<Inter> T \<in> {A \<times> B |A B. A \<in> Collect (openin X) \<and> B \<in> Collect (openin Y)}"
          if "finite T" "T \<subseteq> \<X> \<union> \<Y>" for T
          using that
        proof induction
          case (insert B \<B>)
          then show ?case
            unfolding \<X>_def \<Y>_def
            apply (simp add: Int_ac subset_eq image_def)
            apply (metis (no_types) openin_Int openin_topspace Times_Int_Times)
            done
        qed auto
        then show ?thesis
          using that
          by (auto simp: subset_eq  elim!: relative_toE intersection_ofE)
      qed
      moreover
      have "?lhs Z" if Z: "?rhs Z" for Z
      proof -
        obtain U V where "Z = U \<times> V" "openin X U" "openin Y V"
          using Z by blast
        then have UV: "U \<times> V = (topspace X \<times> topspace Y) \<inter> (U \<times> V)"
          by (simp add: Sigma_mono inf_absorb2 openin_subset)
        moreover
        have "?lhs ((topspace X \<times> topspace Y) \<inter> (U \<times> V))"
        proof (rule relative_to_inc)
          show "(finite intersection_of (\<lambda>x. x \<in> \<X> \<or> x \<in> \<Y>)) (U \<times> V)"
            apply (simp add: intersection_of_def \<X>_def \<Y>_def)
            apply (rule_tac x="{(U \<times> topspace Y),(topspace X \<times> V)}" in exI)
            using \<open>openin X U\<close> \<open>openin Y V\<close> openin_subset UV apply (fastforce simp:)
            done
        qed
        ultimately show ?thesis
          using \<open>Z = U \<times> V\<close> by auto
      qed
      ultimately show ?thesis
        by meson
    qed
    then show "topology (arbitrary union_of (finite intersection_of (\<lambda>x. x \<in> \<X> \<union> \<Y>)
           relative_to (topspace X \<times> topspace Y))) =
        prod_topology X Y"
      by (simp add: prod_topology_def)
  qed
  ultimately show ?thesis
    using False by blast
qed

lemma compactin_Times:
   "compactin (prod_topology X Y) (S \<times> T) \<longleftrightarrow> S = {} \<or> T = {} \<or> compactin X S \<and> compactin Y T"
  by (auto simp: compactin_subspace subtopology_Times compact_space_prod_topology subtopology_trivial_iff)


subsection\<open>Homeomorphic maps\<close>

lemma homeomorphic_maps_prod:
   "homeomorphic_maps (prod_topology X Y) (prod_topology X' Y') (\<lambda>(x,y). (f x, g y)) (\<lambda>(x,y). (f' x, g' y)) \<longleftrightarrow>
        (prod_topology X Y) = trivial_topology \<and> (prod_topology X' Y') = trivial_topology 
      \<or> homeomorphic_maps X X' f f' \<and> homeomorphic_maps Y Y' g g'"  (is "?lhs = ?rhs")
proof
  show "?lhs \<Longrightarrow> ?rhs"
    by (fastforce simp: homeomorphic_maps_def continuous_map_prod_top ball_conj_distrib)
next
  show "?rhs \<Longrightarrow> ?lhs"
  by (auto simp: homeomorphic_maps_def continuous_map_prod_top)
qed


lemma homeomorphic_maps_swap:
   "homeomorphic_maps (prod_topology X Y) (prod_topology Y X) (\<lambda>(x,y). (y,x)) (\<lambda>(y,x). (x,y))"
  by (auto simp: homeomorphic_maps_def case_prod_unfold continuous_map_fst continuous_map_pairedI continuous_map_snd)

lemma homeomorphic_map_swap:
   "homeomorphic_map (prod_topology X Y) (prod_topology Y X) (\<lambda>(x,y). (y,x))"
  using homeomorphic_map_maps homeomorphic_maps_swap by metis

lemma homeomorphic_space_prod_topology_swap:
   "(prod_topology X Y) homeomorphic_space (prod_topology Y X)"
  using homeomorphic_map_swap homeomorphic_space by blast

lemma embedding_map_graph:
   "embedding_map X (prod_topology X Y) (\<lambda>x. (x, f x)) \<longleftrightarrow> continuous_map X Y f"
    (is "?lhs = ?rhs")
proof
  assume L: ?lhs
  have "snd \<circ> (\<lambda>x. (x, f x)) = f"
    by force
  moreover have "continuous_map X Y (snd \<circ> (\<lambda>x. (x, f x)))"
    using L unfolding embedding_map_def
    by (meson continuous_map_in_subtopology continuous_map_snd_of homeomorphic_imp_continuous_map)
  ultimately show ?rhs
    by simp
next
  assume R: ?rhs
  then show ?lhs
    unfolding homeomorphic_map_maps embedding_map_def homeomorphic_maps_def
    by (rule_tac x=fst in exI)
       (auto simp: continuous_map_in_subtopology continuous_map_paired continuous_map_from_subtopology
                   continuous_map_fst)
qed

lemma homeomorphic_space_prod_topology:
   "\<lbrakk>X homeomorphic_space X''; Y homeomorphic_space Y'\<rbrakk>
        \<Longrightarrow> prod_topology X Y homeomorphic_space prod_topology X'' Y'"
using homeomorphic_maps_prod unfolding homeomorphic_space_def by blast

lemma prod_topology_homeomorphic_space_left:
   "Y = discrete_topology {b} \<Longrightarrow> prod_topology X Y homeomorphic_space X"
  unfolding homeomorphic_space_def
  apply (rule_tac x=fst in exI)
  apply (simp add: homeomorphic_map_def inj_on_def discrete_topology_unique flip: homeomorphic_map_maps)
  done

lemma prod_topology_homeomorphic_space_right:
   "X = discrete_topology {a} \<Longrightarrow> prod_topology X Y homeomorphic_space Y"
  unfolding homeomorphic_space_def
  by (meson homeomorphic_space_def homeomorphic_space_prod_topology_swap homeomorphic_space_trans prod_topology_homeomorphic_space_left)


lemma homeomorphic_space_prod_topology_sing1:
     "b \<in> topspace Y \<Longrightarrow> X homeomorphic_space (prod_topology X (subtopology Y {b}))"
  by (metis empty_subsetI homeomorphic_space_sym insert_subset prod_topology_homeomorphic_space_left subtopology_eq_discrete_topology_sing topspace_subtopology_subset)

lemma homeomorphic_space_prod_topology_sing2:
     "a \<in> topspace X \<Longrightarrow> Y homeomorphic_space (prod_topology (subtopology X {a}) Y)"
  by (metis empty_subsetI homeomorphic_space_sym insert_subset prod_topology_homeomorphic_space_right subtopology_eq_discrete_topology_sing topspace_subtopology_subset)

lemma topological_property_of_prod_component:
  assumes major: "P(prod_topology X Y)"
    and X: "\<And>x. \<lbrakk>x \<in> topspace X; P(prod_topology X Y)\<rbrakk> \<Longrightarrow> P(subtopology (prod_topology X Y) ({x} \<times> topspace Y))"
    and Y: "\<And>y. \<lbrakk>y \<in> topspace Y; P(prod_topology X Y)\<rbrakk> \<Longrightarrow> P(subtopology (prod_topology X Y) (topspace X \<times> {y}))"
    and PQ:  "\<And>X X'. X homeomorphic_space X' \<Longrightarrow> (P X \<longleftrightarrow> Q X')"
    and PR: "\<And>X X'. X homeomorphic_space X' \<Longrightarrow> (P X \<longleftrightarrow> R X')"
  shows "(prod_topology X Y) = trivial_topology \<or> Q X \<and> R Y"
proof -
  have "Q X \<and> R Y" if "topspace(prod_topology X Y) \<noteq> {}"
  proof -
    from that obtain a b where a: "a \<in> topspace X" and b: "b \<in> topspace Y"
      by force
    show ?thesis
      using X [OF a major] and Y [OF b major] homeomorphic_space_prod_topology_sing1 [OF b, of X] homeomorphic_space_prod_topology_sing2 [OF a, of Y]
      by (simp add: subtopology_Times) (meson PQ PR homeomorphic_space_prod_topology_sing2 homeomorphic_space_sym)
  qed
  then show ?thesis by force
qed

lemma limitin_pairwise:
   "limitin (prod_topology X Y) f l F \<longleftrightarrow> limitin X (fst \<circ> f) (fst l) F \<and> limitin Y (snd \<circ> f) (snd l) F"
    (is "?lhs = ?rhs")
proof
  assume ?lhs
  then obtain a b where ev: "\<And>U. \<lbrakk>(a,b) \<in> U; openin (prod_topology X Y) U\<rbrakk> \<Longrightarrow> \<forall>\<^sub>F x in F. f x \<in> U"
                        and a: "a \<in> topspace X" and b: "b \<in> topspace Y" and l: "l = (a,b)"
    by (auto simp: limitin_def)
  moreover have "\<forall>\<^sub>F x in F. fst (f x) \<in> U" if "openin X U" "a \<in> U" for U
  proof -
    have "\<forall>\<^sub>F c in F. f c \<in> U \<times> topspace Y"
      using b that ev [of "U \<times> topspace Y"] by (auto simp: openin_prod_topology_alt)
    then show ?thesis
      by (rule eventually_mono) (metis (mono_tags, lifting) SigmaE2 prod.collapse)
  qed
  moreover have "\<forall>\<^sub>F x in F. snd (f x) \<in> U" if "openin Y U" "b \<in> U" for U
  proof -
    have "\<forall>\<^sub>F c in F. f c \<in> topspace X \<times> U"
      using a that ev [of "topspace X \<times> U"] by (auto simp: openin_prod_topology_alt)
    then show ?thesis
      by (rule eventually_mono) (metis (mono_tags, lifting) SigmaE2 prod.collapse)
  qed
  ultimately show ?rhs
    by (simp add: limitin_def)
next
  have "limitin (prod_topology X Y) f (a,b) F"
    if "limitin X (fst \<circ> f) a F" "limitin Y (snd \<circ> f) b F" for a b
    using that
  proof (clarsimp simp: limitin_def)
    fix Z :: "('a \<times> 'b) set"
    assume a: "a \<in> topspace X" "\<forall>U. openin X U \<and> a \<in> U \<longrightarrow> (\<forall>\<^sub>F x in F. fst (f x) \<in> U)"
      and b: "b \<in> topspace Y" "\<forall>U. openin Y U \<and> b \<in> U \<longrightarrow> (\<forall>\<^sub>F x in F. snd (f x) \<in> U)"
      and Z: "openin (prod_topology X Y) Z" "(a, b) \<in> Z"
    then obtain U V where "openin X U" "openin Y V" "a \<in> U" "b \<in> V" "U \<times> V \<subseteq> Z"
      using Z by (force simp: openin_prod_topology_alt)
    then have "\<forall>\<^sub>F x in F. fst (f x) \<in> U" "\<forall>\<^sub>F x in F. snd (f x) \<in> V"
      by (simp_all add: a b)
    then show "\<forall>\<^sub>F x in F. f x \<in> Z"
      by (rule eventually_elim2) (use \<open>U \<times> V \<subseteq> Z\<close> subsetD in auto)
  qed
  then show "?rhs \<Longrightarrow> ?lhs"
    by (metis prod.collapse)
qed

proposition connected_space_prod_topology:
   "connected_space(prod_topology X Y) \<longleftrightarrow>
    (prod_topology X Y) = trivial_topology \<or> connected_space X \<and> connected_space Y" (is "?lhs=?rhs")
proof (cases "(prod_topology X Y) = trivial_topology")
  case True
  then show ?thesis
    by auto
next
  case False
  then have nonempty: "topspace X \<noteq> {}" "topspace Y \<noteq> {}"
    by force+
  show ?thesis 
  proof
    assume ?lhs
    then show ?rhs
      by (metis connected_space_quotient_map_image nonempty quotient_map_fst quotient_map_snd 
          subtopology_eq_discrete_topology_empty)
  next
    assume ?rhs
    then have conX: "connected_space X" and conY: "connected_space Y"
      using False by blast+
    have False
      if "openin (prod_topology X Y) U" and "openin (prod_topology X Y) V"
        and UV: "topspace X \<times> topspace Y \<subseteq> U \<union> V" "U \<inter> V = {}" 
        and "U \<noteq> {}" and "V \<noteq> {}"
      for U V
    proof -
      have Usub: "U \<subseteq> topspace X \<times> topspace Y" and Vsub: "V \<subseteq> topspace X \<times> topspace Y"
        using that by (metis openin_subset topspace_prod_topology)+
      obtain a b where ab: "(a,b) \<in> U" and a: "a \<in> topspace X" and b: "b \<in> topspace Y"
        using \<open>U \<noteq> {}\<close> Usub by auto
      have "\<not> topspace X \<times> topspace Y \<subseteq> U"
        using Usub Vsub \<open>U \<inter> V = {}\<close> \<open>V \<noteq> {}\<close> by auto
      then obtain x y where x: "x \<in> topspace X" and y: "y \<in> topspace Y" and "(x,y) \<notin> U"
        by blast
      have oX: "openin X {x \<in> topspace X. (x,y) \<in> U}" "openin X {x \<in> topspace X. (x,y) \<in> V}"
       and oY: "openin Y {y \<in> topspace Y. (a,y) \<in> U}" "openin Y {y \<in> topspace Y. (a,y) \<in> V}"
        by (force intro: openin_continuous_map_preimage [where Y = "prod_topology X Y"] 
            simp: that continuous_map_pairwise o_def x y a)+
      have 1: "topspace Y \<subseteq> {y \<in> topspace Y. (a,y) \<in> U} \<union> {y \<in> topspace Y. (a,y) \<in> V}"
        using a that(3) by auto
      have 2: "{y \<in> topspace Y. (a,y) \<in> U} \<inter> {y \<in> topspace Y. (a,y) \<in> V} = {}"
        using that(4) by auto
      have 3: "{y \<in> topspace Y. (a,y) \<in> U} \<noteq> {}"
        using ab b by auto
      have 4: "{y \<in> topspace Y. (a,y) \<in> V} \<noteq> {}"
      proof -
        show ?thesis
          using connected_spaceD [OF conX oX] UV \<open>(x,y) \<notin> U\<close> a x y
                disjoint_iff_not_equal by blast
      qed
      show ?thesis
        using connected_spaceD [OF conY oY 1 2 3 4] by auto
    qed
    then show ?lhs
      unfolding connected_space_def topspace_prod_topology by blast 
  qed
qed

lemma connectedin_Times:
   "connectedin (prod_topology X Y) (S \<times> T) \<longleftrightarrow>
        S = {} \<or> T = {} \<or> connectedin X S \<and> connectedin Y T"
  by (auto simp: connectedin_def subtopology_Times connected_space_prod_topology subtopology_trivial_iff)

end

