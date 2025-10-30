theory Physics_Observation
  imports Physics_StateMachine
begin

section \<open>Observation windows and word metadata\<close>

text \<open>
  This stub theory pins down the structures used by the planned
  observation-window proofs. Detailed bounds, decay equations, and
  schedule interactions will land in later iterations once the runtime
  contracts are settled.
\<close>

record word_physics =
  temperature_q8 :: nat
  last_active_ns :: nat
  mass_bytes :: nat
  avg_latency_ns :: nat
  state_flags :: nat
  acl_hint :: nat
  pubsub_mask :: nat

record observation_frame =
  frame_time_ns :: nat
  entropy_delta :: nat
  cooldown_factor :: nat
  latency_sample_ns :: nat
  latency_weight :: nat
  mass_sample_bytes :: nat
  state_flags_update :: nat

definition clip :: "nat \<Rightarrow> nat \<Rightarrow> nat" where
  "clip limit x = min limit x"

text \<open>
  Future work will reintroduce the cooling, latency, and mass update
  combinators together with proof obligations that tie back to the VM
  telemetry.
\<close>

end
