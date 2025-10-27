(set-logic QF_UF)
(set-info :status unsat)

(declare-fun c1 ( ) Bool)

(define-fun cnst ( ) Bool (not c1))

(assert c1)
(assert cnst)
(check-sat)
