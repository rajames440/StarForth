(*!
  File: Physics_StateMachine.thy
  Author: StarForth Physics Working Group

  Initial Isabelle model for the physics execution state machine.
  This formalisation focuses on the discrete observation window used
  by Phase 1 to evolve DictPhysics metadata.
!*)

theory Physics_StateMachine
  imports Main
begin

record dict_physics =
  temperature_q8 :: nat
  last_active_ns :: nat
  mass_bytes :: nat
  avg_latency_ns :: nat
  state_flags :: nat

record physics_state =
  window_width   :: nat
  window_cursor  :: nat
  accumulator    :: nat
  active_entry   :: dict_physics

datatype physics_observation =
    Sample (obs_entropy :: nat)
  | Publish
  | Decay

definition advance_cursor :: "physics_state ⇒ physics_state" where
  "advance_cursor s = s⦇window_cursor := (window_cursor s + 1) mod (max 1 (window_width s))⦈"

fun update_temperature :: "physics_state ⇒ nat ⇒ physics_state" where
  "update_temperature s entropy =
     (let temp = temperature_q8 (active_entry s);
          blended = (3 * temp + min entropy 65535) div 4;
          entry' = (active_entry s)⦇temperature_q8 := blended⦈
      in s⦇active_entry := entry', accumulator := accumulator s + entropy⦈)"

fun physics_step :: "physics_state ⇒ physics_observation ⇒ physics_state" where
  "physics_step s (Sample entropy) = advance_cursor (update_temperature s entropy)" |
  "physics_step s Publish = (let entry' = (active_entry s)⦇last_active_ns := last_active_ns (active_entry s) + accumulator s⦈
                              in advance_cursor (s⦇active_entry := entry', accumulator := 0⦈))" |
  "physics_step s Decay = advance_cursor (s⦇accumulator := accumulator s div 2⦈)"

(* Basic invariants -------------------------------------------------------- *)

definition bounded_temperature :: "physics_state ⇒ bool" where
  "bounded_temperature s ≡ temperature_q8 (active_entry s) ≤ 65535"

definition bounded_window :: "physics_state ⇒ bool" where
  "bounded_window s ≡ window_width s > 0 ⟶ window_cursor s < window_width s"

lemma physics_step_preserves_temperature:
  assumes "bounded_temperature s"
  shows "bounded_temperature (physics_step s obs)"
  using assms
  by (cases obs; simp add: bounded_temperature_def advance_cursor_def split: if_splits)

lemma physics_step_preserves_window:
  assumes "window_width s > 0"
  shows "window_cursor (physics_step s obs) < window_width s"
  using assms
  by (cases obs; simp add: advance_cursor_def split: if_splits)

end
