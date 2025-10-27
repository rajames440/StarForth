(echo "run with --disable-simp")
(set-option :produce-proofs true)
(set-logic UF)

(declare-sort S 0)
(declare-const A S)
(declare-fun f (S) Bool)
(declare-fun g (S) Bool)

(assert (forall ((x S)) (not (and (f x) (g x)))))
(assert (f A))
(assert (g A))

(check-sat)
(get-proof)
