(echo "Example to test la_generic proof output with equalities. Run without prepro.")
(set-logic QF_LRA)

(declare-const a Real)
(declare-const b Real)
(declare-fun f (Real) Real)
(declare-fun g (Real) Real)


(assert (and (= a b)
             (> (+ (f a) (* 2.0 (g a))) 
                (+ (f b) (* 2.0 (g b)))
						 )))
(check-sat)
(exit)
