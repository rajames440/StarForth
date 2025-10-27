theory Observation_Window
  imports StarForth_Spec
begin

locale Observation_Window =
  fixes step :: "SF_State ⇒ SF_Obs ⇒ SF_State"
assumes publish_zeroes: "accumulator (step s Publish) = 0"
assumes width_invariant: "window_width (step s o) = window_width s"
assumes cursor_bounds:
  "window_width s > 0 ⟹ window_cursor (step s o) < window_width s"
assumes monotone_cases:
  "accumulator (step s (Sample e)) ≥ accumulator s"
  "accumulator (step s Decay) ≤ accumulator s"
  "accumulator (step s Publish) ≤ accumulator s"
begin

lemma acc_nonneg_if_init_nonneg:
  assumes "accumulator s ≥ 0"
  shows "accumulator (step s o) ≥ 0"
  using assms monotone_cases publish_zeroes
  by (cases o; simp)

lemma width_constant: "window_width (step s o) = window_width s"
  using width_invariant by simp

lemma cursor_stays_in_range:
  assumes "window_width s > 0"
  shows "window_cursor (step s o) < window_width s"
  using assms cursor_bounds by blast

end

end
