(echo "Example of proof output for step simplifications.")
(echo "Run with --disable-simp")
(set-option :produce-proofs true)

(set-logic AUFLIA)

(declare-fun p$ () Bool)
(assert (! (not (ite p$ p$ (not p$))) :named a0))

(check-sat)
(get-proof)
