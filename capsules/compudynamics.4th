Block 4200
( Compudynamics - VM-level physics governance )
( Same 7-loop framework as word physics, lifted to VM granularity. )
( Context switch at REPL-turn boundary. One shared heartbeat. )
0 CONSTANT VM-HERA
1 CONSTANT VM-HERMES
2 CONSTANT VM-ARTEMIS
3 CONSTANT VM-COUNT
CREATE VM-HEATS VM-COUNT CELLS ALLOT
CREATE VM-TRANS VM-COUNT VM-COUNT * CELLS ALLOT
VARIABLE VM-LAST
VARIABLE VM-HOT
VARIABLE VM-BEST-IDX
VARIABLE VM-BEST-HEAT
65208 CONSTANT Q-DECAY
Block 4201
( Compudynamics - heat and transition management )
: VM-HEAT@ ( idx -- q48 ) CELLS VM-HEATS + @ ;
: VM-HEAT! ( q48 idx -- ) CELLS VM-HEATS + ! ;
: VM-DECAY-ONE ( idx -- )
  DUP VM-HEAT@ Q-DECAY Q.* SWAP VM-HEAT! ;
: VM-DECAY-ALL ( -- )
  VM-COUNT 0 DO I VM-DECAY-ONE LOOP ;
: VM-BUMP ( idx -- )
  VM-LAST @ VM-COUNT * OVER + CELLS VM-TRANS + DUP @ 1+ SWAP !
  DUP VM-HEAT@ Q.1 Q.+ SWAP VM-HEAT! ;
Block 4202
( Compudynamics - selection, tick, control )
: VM-HOTTEST ( -- idx )
  Q.0 VM-BEST-HEAT ! 0 VM-BEST-IDX !
  VM-COUNT 0 DO
    I CELLS VM-HEATS + @
    DUP VM-BEST-HEAT @ Q.>
    IF VM-BEST-HEAT ! I VM-BEST-IDX !
    ELSE DROP THEN
  LOOP VM-BEST-IDX @ ;
: VM-STEP-IDX ( idx -- )
  CASE
    VM-HERA    OF S" Hera"    VM-STEP ENDOF
    VM-HERMES  OF S" Hermes"  VM-STEP ENDOF
    VM-ARTEMIS OF S" Artemis" VM-STEP ENDOF
  ENDCASE ;
: VM-TICK ( -- )
  VM-DECAY-ALL
  VM-HOTTEST DUP VM-HOT ! DUP VM-BUMP VM-LAST !
  VM-HOT @ VM-STEP-IDX ;
: VM-RUN ( n -- ) 0 DO VM-TICK LOOP ;
: VM-STATUS ( -- )
  ." Hera:    " VM-HERA    VM-HEAT@ Q.PRINT CR
  ." Hermes:  " VM-HERMES  VM-HEAT@ Q.PRINT CR
  ." Artemis: " VM-ARTEMIS VM-HEAT@ Q.PRINT CR
  ." Hot VM:  " VM-HOT @ . CR ;
: VM-INIT ( -- )
  VM-HEATS VM-COUNT CELLS 0 FILL
  VM-TRANS VM-COUNT VM-COUNT * CELLS 0 FILL
  VM-HERA VM-LAST ! VM-HERA VM-HOT ! ;
