(set-logic UF)

(declare-sort S 0)
(declare-const A Bool)
(declare-const B Bool)
(assert (or (not A) B))
(assert (forall ((x S)) (xor A B)))
(check-sat)
