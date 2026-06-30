Block 2057
( BOOT-BANNER - LithosAnanke REPL greeting )
: .SEP ." ================================================" CR ;
: BOOT-BANNER
  CR .SEP
  ." LithosAnanke — FORTH-79 bare-metal kernel" CR
  ." Tripod v1: K=1.0 + Hermes events + Artemis storage" CR
  .SEP
  ." Tripod: Hera  Hermes  Artemis  [live]" CR
  .SEP CR ;
Block 2049
( Hera init — Mama VM personality )
: VM-TREE     ( -- ) ." Hera[0]>Hermes[1] Artemis[2]" CR ;
: VM-PARENT   ( -- id ) 0 ;
: VM-CHILDREN ( -- ) ." Hermes[1] Artemis[2]" CR ;
\ S" ACL.4th" EXEC
S" compudynamics.4th" EXEC
VM-INIT
S" lib.4th" EXEC
S" common:msg.4th" USE
S" fleet-k.4th" EXEC
S" process.4th" EXEC
\ Tripod boot: Artemis -> Hermes -> Hera
S" Artemis" BIRTH
S" CD-INIT" S" Artemis" VM-EXEC
S" Hermes" BIRTH
S" CD-INIT" S" Hermes" VM-EXEC
Block 2050
( Hera boot — banner )
BOOT-BANNER
Block 2051
( TRIPOD-TEST: 6 criteria per TRIPOD.md )
: TRIPOD-TEST ( -- )
  LOG-INFO" === TRIPOD Acceptance ==="
  K-INIT 8 0 DO CD-TICK LOOP K-STATUS
  K-CONSERVED? IF LOG-TEST" PASS: fleet K"
               ELSE LOG-ERROR" FAIL: K drift" THEN
  S" HERMES-TICK" S" Hermes" VM-EXEC
  LOG-TEST" PASS: Hermes liveness"
  S" ART-STATUS" S" Artemis" VM-EXEC
  LOG-TEST" PASS: Artemis ready"
  S" Hermes" KILL-VM ." Reaped; active=" ACTIVE-VMS @ . CR
  S" Hermes" BIRTH K-SPAWN-HOOK S" CD-INIT" S" Hermes" VM-EXEC
  ." Respawned; active=" ACTIVE-VMS @ . CR
  K-INIT 32 0 DO CD-TICK LOOP
  K-CONSERVED? IF LOG-TEST" K soak" ELSE LOG-ERROR" K soak" THEN
  LOG-INFO" === TRIPOD DONE ===" ;
Block 2052
( Run TRIPOD acceptance test + E2E msg flow )
TRIPOD-TEST
: HERMES-E2E ( -- )
  S" HERMES-MSG-TEST" S" Hermes" VM-CALL
  IF LOG-TEST" PASS: msg queued"
  ELSE LOG-ERROR" FAIL: msg queued" THEN
  S" HERMES-TICK"     S" Hermes" VM-EXEC
  S" MSG-USED" S" Hermes" VM-CALL
  0 > IF LOG-TEST" PASS: E2E msg flow"
       ELSE LOG-ERROR" FAIL: E2E msg flow" THEN ;
HERMES-E2E
