theory Ring_Buffer
  imports Main
begin

locale Ring_Buffer =
  fixes cap :: nat
  fixes enq :: "'a list ⇒ 'a ⇒ ('a list × bool)"   (* bool = success *)
  fixes deq :: "'a list ⇒ ('a list × 'a option)"
assumes cap_pos: "cap > 0"
assumes enq_ok_iff_space: "length q < cap ⟷ snd (enq q x)"
assumes enq_success_appends:
  "length q < cap ⟹ fst (enq q x) = q @ [x]"
assumes deq_head: "q ≠ [] ⟹ deq q = (tl q, Some (hd q))"
assumes deq_empty: "q = [] ⟹ deq q = ([], None)"
begin

lemma deq_roundtrip_hd:
  assumes "q ≠ []"
  shows "snd (deq q) = Some (hd q)"
  using assms deq_head by simp

lemma enq_never_overwrites:
  assumes "length q < cap"
  shows "fst (enq q x) = q @ [x] ∧ snd (enq q x) = True"
  using assms enq_success_appends enq_ok_iff_space by auto

lemma enq_full_drops:
  assumes "length q ≥ cap"
  shows "snd (enq q x) = False"
  using assms enq_ok_iff_space by auto

end

end
