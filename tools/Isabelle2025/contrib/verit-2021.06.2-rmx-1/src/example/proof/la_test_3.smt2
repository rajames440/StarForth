(echo "Example to test la_generic proof output. Run without prepro.")
(set-logic QF_LRA)

(declare-const t0 Real)
(declare-const t1 Real)
(declare-const t2 Real)
(declare-const t3 Real)
(declare-const t4 Real)
(declare-const t5 Real)
(declare-const  x Real)


(assert (and (<= t0 0.0)
             (<= 3.0 t4)
             (<= (* 0.5 t4) t0)
						 ))
(check-sat)
(exit)
