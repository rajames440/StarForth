theory StarForth_Impl_Sketch
  imports Observation_Window Ring_Buffer Seeds Snapshot
begin

(* --- A. Observation window: a toy step function --- *)

definition step_impl :: "SF_State ⇒ SF_Obs ⇒ SF_State" where
  "step_impl s o = (case o of
     Publish    ⇒ s⦇accumulator := 0, backend_seq := Suc (backend_seq s)⦈
   | Decay      ⇒ s⦇accumulator := max 0 (accumulator s div 2)⦈
   | Sample e   ⇒ s⦇accumulator := accumulator s + int e⦈)"

(* cursor logic stub: keep cursor in [0,width) if width>0 *)
definition step_cursor :: "SF_State ⇒ SF_Obs ⇒ SF_State" where
  "step_cursor s o = (let s' = step_impl s o in
     if window_width s' = 0 then s'
     else s'⦇window_cursor := (window_cursor s' + 1) mod window_width s'⦈)"

definition step_all :: "SF_State ⇒ SF_Obs ⇒ SF_State" where
  "step_all s o = (step_cursor s o)"

interpretation OW: Observation_Window step_all
proof
  show "accumulator (step_all s Publish) = 0"
    by (simp add: step_all_def step_cursor_def step_impl_def)
  show "window_width (step_all s o) = window_width s"
    by (simp add: step_all_def step_cursor_def step_impl_def)
  show "window_width s > 0 ⟹ window_cursor (step_all s o) < window_width s"
    by (simp add: step_all_def step_cursor_def step_impl_def)
  show "accumulator (step_all s (Sample e)) ≥ accumulator s"
    by (simp add: step_all_def step_cursor_def step_impl_def)
  show "accumulator (step_all s Decay) ≤ accumulator s"
    by (simp add: step_all_def step_cursor_def step_impl_def)
  show "accumulator (step_all s Publish) ≤ accumulator s"
    by (simp add: step_all_def step_cursor_def step_impl_def)
qed

(* --- B. Ring buffer: list-backed model --- *)

fun enq_list :: "'a list ⇒ nat ⇒ 'a ⇒ ('a list × bool)" where
  "enq_list q cap x = (if length q < cap then (q @ [x], True) else (q, False))"

fun deq_list :: "'a list ⇒ ('a list × 'a option)" where
  "deq_list [] = ([], None)" |
  "deq_list (x#xs) = (xs, Some x)"

interpretation RB: Ring_Buffer cap enq_list deq_list
  for cap
proof
  show "cap > 0 ⟹ cap > 0" by simp
  show "length q < cap ⟷ snd (enq_list q cap x)" by simp
  show "length q < cap ⟹ fst (enq_list q cap x) = q @ [x]" by simp
  show "q ≠ [] ⟹ deq_list q = (tl q, Some (hd q))" by (cases q; simp)
  show "q = [] ⟹ deq_list q = ([], None)" by simp
qed

(* --- C. Seeds: idempotent on a whitelist --- *)

definition apply_seed_impl :: "SF_State ⇒ string ⇒ SF_State" where
  "apply_seed_impl s w = s⦇accumulator := (if w = ''TEMP'' then max (accumulator s) 1 else accumulator s)⦈"

definition seeded_set :: "string set" where
  "seeded_set = {''TEMP''}"

interpretation SD: Seeds apply_seed_impl seeded_set
proof
  show "w ∈ seeded_set ⟹ apply_seed_impl (apply_seed_impl s w) w = apply_seed_impl s w"
    by (auto simp: apply_seed_impl_def seeded_set_def)
  show "w ∉ seeded_set ⟹ apply_seed_impl s w = s"
    by (auto simp: apply_seed_impl_def seeded_set_def)
qed

(* --- D. Snapshot: publish increments sequence and may clear PSI flag --- *)

definition publish_impl :: "SF_State ⇒ SF_State" where
  "publish_impl s = s⦇backend_seq := Suc (backend_seq s)⦈"

interpretation SS: Snapshot publish_impl
proof
  show "backend_seq (publish_impl s) = Suc (backend_seq s)"
    by (simp add: publish_impl_def)
  show "¬ flags_PSI_ok (publish_impl s) ⟹ True" by simp
qed

end
