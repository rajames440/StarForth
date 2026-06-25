Block 4400
( fleet-k.4th — per-VM rolling window + Fleet K conservation invariant )
8 CONSTANT K-WINDOW-DEPTH
CREATE K-WINDOWS VM-COUNT K-WINDOW-DEPTH * CELLS ALLOT
CREATE K-HEADS   VM-COUNT CELLS ALLOT
CREATE K-LOCAL   VM-COUNT CELLS ALLOT
CREATE K-TARGET  VM-COUNT CELLS ALLOT
: K-INIT ( -- )
  K-WINDOWS VM-COUNT K-WINDOW-DEPTH * CELLS 0 FILL
  K-HEADS   VM-COUNT CELLS 0 FILL
  K-LOCAL   VM-COUNT CELLS 0 FILL
  VM-COUNT 0 DO Q.1 I CELLS K-TARGET + ! LOOP ;
K-INIT
Block 4401
( K-SLOT: slot address for VM idx at its current ring head )
: K-SLOT ( idx -- slot-addr )
  DUP K-WINDOW-DEPTH * CELLS K-WINDOWS +
  SWAP CELLS K-HEADS + @ CELLS + ;
( K-PUSH: write q48 sample into VM idx ring and advance head )
: K-PUSH ( q48 idx -- )
  DUP K-SLOT ROT SWAP !
  CELLS K-HEADS +
  DUP @ 1+ K-WINDOW-DEPTH MOD SWAP ! ;
( K-LOCAL@: rolling average of 8 samples for VM idx )
: K-LOCAL@ ( idx -- q48 )
  K-WINDOW-DEPTH * CELLS K-WINDOWS +
  0 K-WINDOW-DEPTH 0 DO
    OVER I CELLS + @ +
  LOOP NIP K-WINDOW-DEPTH / ;
Block 4402
( K-FLEET: normalized sum of K-LOCAL across all VMs )
: K-FLEET ( -- q48 )
  0 VM-COUNT 0 DO I K-LOCAL@ + LOOP
  VM-COUNT / ;
: K-STATUS ( -- )
  ." K-LOCAL:" CR
  ." Hera:    " VM-HERA    K-LOCAL@ Q.PRINT CR
  ." Hermes:  " VM-HERMES  K-LOCAL@ Q.PRINT CR
  ." Artemis: " VM-ARTEMIS K-LOCAL@ Q.PRINT CR
  ." K-FLEET: " K-FLEET Q.PRINT CR ;
: PHASE-OF ( idx -- n )
  K-LOCAL@
  DUP 0= IF DROP 0 EXIT THEN
  Q.1 Q.> IF 2 ELSE 1 THEN ;
: K-BUMP ( idx -- ) Q.1 SWAP K-PUSH ;
Block 4403
( CD-TICK with K-BUMP hooked in — shadows doe-campaign.4th definition )
: CD-TICK ( -- )
  VM-DECAY-ALL
  VM-HOTTEST DUP VM-HOT ! DUP VM-BUMP VM-LAST !
  VM-HOT @ CD-WORK
  VM-HOT @ K-BUMP ;
