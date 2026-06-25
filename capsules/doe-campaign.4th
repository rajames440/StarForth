Block 4060
( doe-campaign.4th: Compudynamics DoE campaign orchestrator )
( Requires: compudynamics.4th. Child VMs must define LOAD-DOE. )
: SETUP-HERMES ( -- )
  S" Hermes" BIRTH
  S" LOAD-DOE" S" Hermes" VM-EXEC ;
: SETUP-ARTEMIS ( -- )
  S" Artemis" BIRTH
  S" LOAD-DOE" S" Artemis" VM-EXEC ;
: SETUP-VMS ( -- )
  SETUP-HERMES SETUP-ARTEMIS ;
Block 4061
( Phase 1: each VM runs DOE-WORK once for baseline. )
: PHASE1-DOE ( -- )
  ." Phase 1: Hermes" CR
  S" DOE-WORK" S" Hermes" VM-EXEC
  ." Phase 1: Artemis" CR
  S" DOE-WORK" S" Artemis" VM-EXEC ;
( Phase 2: CD-WORK dispatches DOE-WORK to the hottest VM. )
: CD-WORK ( idx -- )
  CASE
    VM-HERMES  OF S" DOE-WORK" S" Hermes"  VM-EXEC ENDOF
    VM-ARTEMIS OF S" DOE-WORK" S" Artemis" VM-EXEC ENDOF
    VM-HERA    OF S" DOE-WORK" S" Hera"    VM-EXEC ENDOF
  ENDCASE ;
Block 4062
( Phase 2 campaign execution )
: CD-TICK ( -- )
  VM-DECAY-ALL
  VM-HOTTEST DUP VM-HOT ! DUP VM-BUMP VM-LAST !
  VM-HOT @ CD-WORK
  VM-HOT @ K-BUMP ;
: CD-DOE ( n -- ) 0 DO CD-TICK LOOP ;
: CAMPAIGN-STATUS ( -- )
  ." === VM Heat after campaign ===" CR
  VM-STATUS ;
Block 4063
( Full DoE campaign: baselines then 30 CD-governed ticks )
: CAMPAIGN ( -- )
  ." === Compudynamics DoE Campaign ===" CR
  VM-INIT
  SETUP-VMS
  ." Phase 1: individual baselines" CR
  PHASE1-DOE
  VM-INIT
  ." Phase 2: 30 CD-governed ticks" CR
  30 CD-DOE
  CAMPAIGN-STATUS ;
Block 4064
( Smoke test: 1 rep seed 1959 + 16 CD ticks )
: SMOKE-CAMPAIGN ( -- )
  ." === Smoke: 1 rep seed 1959 ===" CR
  VM-INIT
  SETUP-VMS
  ." Hermes 1 rep..." CR
  S" 1959 1 EXEC-DOE" S" Hermes" VM-EXEC
  ." Artemis 1 rep..." CR
  S" 1959 1 EXEC-DOE" S" Artemis" VM-EXEC
  VM-INIT
  Q.1 VM-HERMES VM-HEAT!
  Q.1 VM-ARTEMIS VM-HEAT!
  ." 16 CD ticks..." CR
  16 CD-DOE
  CAMPAIGN-STATUS ;
Block 4065
( THREE-VM-CAMPAIGN: all three VMs as Compudynamics peers. )
: THREE-VM-CAMPAIGN ( -- )
  ." === Three-VM Campaign ===" CR
  S" doe.4th" EXEC
  SETUP-VMS
  VM-INIT
  Q.1                       VM-HERA    VM-HEAT!
  Q.1 Q.1 Q.+               VM-HERMES  VM-HEAT!
  Q.1 Q.1 Q.+ Q.1 Q.+       VM-ARTEMIS VM-HEAT!
  ." 48 CD ticks..." CR
  48 CD-DOE
  CAMPAIGN-STATUS ;
