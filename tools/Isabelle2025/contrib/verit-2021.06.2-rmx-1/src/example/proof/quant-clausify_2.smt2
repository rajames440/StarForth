(set-option :produce-proofs true)

(set-logic AUFLIA)

(assert (not (< 0 (ite (forall ((?v0 Int)) (or (< ?v0 0) (< 0 ?v0))) (- 1) 3))))

(check-sat)
(get-proof)

