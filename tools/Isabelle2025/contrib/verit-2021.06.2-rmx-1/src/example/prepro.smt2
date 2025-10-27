(set-logic QF_UF)

(declare-sort S 0)
(declare-const A S)
(declare-fun f (S) Bool)
(assert (or false (f A) (f A)))
(assert (not (f A)))
(check-sat)
