theory StarForth_Spec
  imports Main
begin

record SF_State =
  accumulator    :: int
  window_width   :: nat
  window_cursor  :: nat
  backend_seq    :: nat
  flags_PSI_ok   :: bool
  dropped_events :: nat

datatype SF_Obs = Publish | Decay | Sample nat

end
