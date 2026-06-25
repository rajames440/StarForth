Block 4300
( process.4th — Compudynamic lifecycle phase operators )
( SPAWN/KILL are phase perturbations, not scheduler decisions. )
( Process unit = StarForth VM. Future: backed by sk_proc_create HAL. )
: SPAWN  ( c-addr u -- )
  BIRTH
  S" 0 EVENT-EMIT" S" Hermes" VM-EXEC ;
: PAUSE  ( c-addr u -- )
  S" STOP" 2SWAP VM-EXEC ;
: RESUME ( c-addr u -- )
  START ;
: KILL-VM ( c-addr u -- )
  KILL
  S" 0 EVENT-EMIT" S" Hermes" VM-EXEC ;
: CD-PHASE@ ( idx -- phase )
  VM-HEAT@ Q.1 Q.> IF 1 ELSE 0 THEN ;
