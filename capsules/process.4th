Block 4300
( process.4th — lifecycle phase operators )
( Event codes must match hermes/init.4th )
1 CONSTANT SPAWN-EVENT
2 CONSTANT PAUSE-EVENT
3 CONSTANT RESUME-EVENT
4 CONSTANT KILL-EVENT
( SPAWN: birth VM, rebalance K, notify Hermes )
: SPAWN ( c-addr u -- )
  BIRTH
  LOG-INFO" process: spawned"
  K-SPAWN-HOOK
  S" 1 EVENT-EMIT" S" Hermes" VM-EXEC ;
( PAUSE: notify Hermes, send STOP to the named VM )
: PAUSE ( c-addr u -- )
  S" 2 EVENT-EMIT" S" Hermes" VM-EXEC
  S" STOP" 2SWAP VM-EXEC ;
Block 4301
( Lifecycle operators: resume, kill, phase )
( RESUME: notify Hermes, START the named VM )
: RESUME ( c-addr u -- )
  S" 3 EVENT-EMIT" S" Hermes" VM-EXEC
  LOG-INFO" process: resumed"
  START ;
( KILL-VM: notify Hermes -> K-KILL-HOOK, then kill VM )
: KILL-VM ( c-addr u -- )
  S" 4 EVENT-EMIT" S" Hermes" VM-EXEC
  LOG-INFO" process: killed"
  KILL ;
( CD-PHASE@: thermodynamic phase: 0=COLD 1=WARM 2=HOT )
: CD-PHASE@ ( idx -- n )
  DUP VM-HEAT@ 0= IF DROP 0 EXIT THEN
  DUP VM-HEAT@ SWAP CELLS K-TARGET + @ >
  IF 2 ELSE 1 THEN ;
