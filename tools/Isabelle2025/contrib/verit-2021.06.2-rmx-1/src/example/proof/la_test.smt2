(echo "Example to test la_generic proof output with equalities. Run without prepro.")
(set-logic QF_LRA)

(declare-const a Real)
(declare-const b Real)
(declare-fun f (Real) Real)

(assert (and (= a b) (> (f a) (f b))))
(check-sat)
(exit)
