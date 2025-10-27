(set-option :produce-proofs true)
(set-logic UF)

(declare-sort S 0)
(declare-const A S)
(declare-fun f (S) Bool)

(assert (forall ((x S) (y S)) 
  (=> (= x y)
      (and (f x) (f y))
  )
))
(assert (not (f A)))

(check-sat)
(get-proof)
