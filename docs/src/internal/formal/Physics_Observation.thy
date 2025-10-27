(*!
  File: Physics_Observation.thy
  Author: StarForth Physics Working Group

  Invariants relating the observation window to host snapshots. This theory
  is intended to evolve alongside the HOLA shared-memory specification.
!*)

theory Physics_Observation
  imports Physics_StateMachine
begin

record host_snapshot =
  backend              :: nat
  scheduler_quantum_ns :: nat
  runnable_threads     :: nat
  cpu_count            :: nat

record observation_frame =
  state    :: physics_state
  snapshot :: host_snapshot

(* Observation windows must not shrink below one tick, and a populated heap
   implies we observed at least one sample. *)

definition window_ready :: "observation_frame ⇒ bool" where
  "window_ready f ≡ window_width (state f) ≥ 1 ∧ (accumulator (state f) > 0 ⟶ runnable_threads (snapshot f) ≥ 1)"

lemma window_ready_step_sample:
  assumes "window_ready f"
  shows "window_ready (f⦇state := physics_step (state f) (Sample e)⦈)"
  using assms
  by (simp add: window_ready_def)

lemma window_ready_step_decay:
  assumes "window_ready f"
  shows "window_ready (f⦇state := physics_step (state f) Decay⦈)"
  using assms
  by (simp add: window_ready_def)

end
