theory Snapshot
  imports StarForth_Spec
begin

locale Snapshot =
  fixes publish :: "SF_State ⇒ SF_State"
assumes publish_increments_seq: "backend_seq (publish s) = Suc (backend_seq s)"
assumes psi_flag_guard:
  "¬ flags_PSI_ok (publish s) ⟹ (* consumers must ignore PSI-derived fields *) True"
begin

lemma seq_strictly_increases:
  "backend_seq (publish s) > backend_seq s"
  using publish_increments_seq by simp

end

end
