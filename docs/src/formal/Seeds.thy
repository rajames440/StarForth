theory Seeds
  imports StarForth_Spec
begin

locale Seeds =
  fixes apply_seed :: "SF_State ⇒ string ⇒ SF_State"
  fixes seeded     :: "string set"
assumes idempotent: "w ∈ seeded ⟹ apply_seed (apply_seed s w) w = apply_seed s w"
assumes nonseed_noop: "w ∉ seeded ⟹ apply_seed s w = s"
begin

lemma apply_seed_twice_eq_once:
  assumes "w ∈ seeded"
  shows "apply_seed (apply_seed s w) w = apply_seed s w"
  using assms idempotent by simp

lemma unseeded_no_effect:
  assumes "w ∉ seeded"
  shows "apply_seed s w = s"
  using assms nonseed_noop by simp

end

end
