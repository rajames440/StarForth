(set-logic QF_UF)
(set-info :status unsat)

(declare-fun c1 () Bool)
(declare-fun c2 () Bool)

(define-fun eq ((x Bool) (y Bool)) Bool (= x y))

(assert (eq c1 c2))
(assert (not (= c1 c2)))
(check-sat)
