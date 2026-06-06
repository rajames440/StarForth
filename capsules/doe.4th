Block 2049
( doe.4th - L8-DOE ( seed reps -- )                 )
( Blind full-factorial L8 DoE for LithosAnanke       )
( 16 L8 configs x 16 workload capsules x N-REPS reps )
( Run matrix: N-CFG*REPS rows; workload blindly drawn )
( Response: heartbeat [HADES][DOE] ticks during EXEC  )
( Same seed = same experiment; different seed = vary  )
49152 CONSTANT ENT-HI    ( 0.75 * 65536 )
 9830 CONSTANT CV-HI     ( 0.15 * 65536 )
32768 CONSTANT TMP-HI    ( 0.50 * 65536 )
32768 CONSTANT STB-HI    ( 0.50 * 65536 )
   16 CONSTANT N-CFG
   16 CONSTANT N-WL
VARIABLE DOE-SEED
VARIABLE DOE-REPS
VARIABLE DOE-RUNS
VARIABLE CURR-CFG
VARIABLE CURR-WL
VARIABLE CURR-REP
VARIABLE RUN-ID

Block 2050
( serial output primitives )
: N. ( n -- )
  DUP 0 < IF 45 EMIT ABS THEN
  0 SWAP <# #S #> TYPE ;
: COMMA  44 EMIT ;
: CRLF   13 EMIT 10 EMIT ;

Block 2051
( factor extraction: cfg 4 bits b3=ent b2=cv b1=tmp b0=stb )
: CFG-ENT ( cfg -- q ) 8 AND IF ENT-HI ELSE 0 THEN ;
: CFG-CV  ( cfg -- q ) 4 AND IF CV-HI  ELSE 0 THEN ;
: CFG-TMP ( cfg -- q ) 2 AND IF TMP-HI ELSE 0 THEN ;
: CFG-STB ( cfg -- q ) 1 AND IF STB-HI ELSE 0 THEN ;
: APPLY-CFG ( cfg -- )
  CURR-CFG !
  CURR-CFG @ CFG-ENT
  CURR-CFG @ CFG-CV
  CURR-CFG @ CFG-TMP
  CURR-CFG @ CFG-STB
  L8-UPDATE L8-APPLY ;

Block 2052
( workload names 0-7: blind study, callers see index only )
( slot 1=init-7 sub init-1 100K*WAIT; slot 5=init-8 sub init-5 1M*inner )
: WL-LO ( n -- c-addr u )
  CASE
    0 OF S" init-0.4th" ENDOF
    1 OF S" init-7.4th" ENDOF
    2 OF S" init-2.4th" ENDOF
    3 OF S" init-3.4th" ENDOF
    4 OF S" init-4.4th" ENDOF
    5 OF S" init-8.4th" ENDOF
    6 OF S" init-6.4th" ENDOF
    7 OF S" init-7.4th" ENDOF
    DROP S" init-0.4th"
  ENDCASE ;

Block 2057
( workload names 8-15 and WL-NAME dispatch )
: WL-HI ( n -- c-addr u )
  CASE
    0 OF S" init-8.4th"             ENDOF
    1 OF S" init-9.4th"             ENDOF
    2 OF S" init-l8-diverse.4th"    ENDOF
    3 OF S" init-l8-omni.4th"       ENDOF
    4 OF S" init-l8-stable.4th"     ENDOF
    5 OF S" init-l8-temporal.4th"   ENDOF
    6 OF S" init-l8-transition.4th" ENDOF
    7 OF S" init-l8-volatile.4th"   ENDOF
    DROP S" init-0.4th"
  ENDCASE ;
: WL-NAME ( n -- c-addr u )
  DUP 8 < IF WL-LO ELSE 8 - WL-HI THEN ;
: EXEC-WL ( n -- ) WL-NAME EXEC ;

Block 2053
( run and workload matrices -- max N-CFG * 200 reps = 3200 cells )
CREATE RUN-MATRIX N-CFG 200 * 8 * ALLOT
CREATE WL-MATRIX  N-CFG 200 * 8 * ALLOT
: RM@ ( idx -- val ) 8 * RUN-MATRIX + @ ;
: RM! ( val idx -- ) 8 * RUN-MATRIX + ! ;
: WL@ ( idx -- val ) 8 * WL-MATRIX  + @ ;
: WL! ( val idx -- ) 8 * WL-MATRIX  + ! ;
: SWAP-RM ( i j -- )
  OVER RM@ >R
  DUP  RM@ ROT RM!
  R>   SWAP RM! ;
: SWAP-WL ( i j -- )
  OVER WL@ >R
  DUP  WL@ ROT WL!
  R>   SWAP WL! ;

Block 2054
( matrix initialisation and Fisher-Yates shuffle )
: INIT-RM ( -- )
  DOE-RUNS @ 0 DO I I RM! LOOP ;
: INIT-WL ( -- )
  DOE-RUNS @ 0 DO I N-WL MOD I WL! LOOP ;
: SHUFFLE-RM ( -- )
  DOE-RUNS @ 1 - 0 DO
    I DOE-RUNS @ 1 - RANDOM
    I SWAP-RM
  LOOP ;
: SHUFFLE-WL ( -- )
  DOE-RUNS @ 1 - 0 DO
    I DOE-RUNS @ 1 - RANDOM
    I SWAP-WL
  LOOP ;
: DECODE-CFG ( val -- cfg ) DOE-REPS @ / ;
: DECODE-REP ( val -- rep ) DOE-REPS @ MOD ;

Block 2055
( single run: set config, emit DOE-RUN marker, exec workload )
( marker format: DOE-RUN,run_id,cfg,wl_id,rep               )
( heartbeat [HADES][DOE] rows emitted during EXEC-WL are     )
( the response variable -- no separate measurement needed    )
: RUN-MARKER ( -- )
  ." DOE-RUN," RUN-ID @ N. COMMA
  CURR-CFG @ N. COMMA
  CURR-WL @ N. COMMA
  CURR-REP @ N. CRLF ;
: ONE-RUN ( pos -- )
  DUP RM@ DUP DECODE-CFG CURR-CFG ! DECODE-REP CURR-REP !
  DUP WL@ CURR-WL !
  DROP
  CURR-CFG @ APPLY-CFG
  PHYSICS-RESET-STATS
  RUN-MARKER
  CURR-WL @ EXEC-WL
  RUN-ID @ 1 + RUN-ID ! ;

Block 2056
( L8-DOE ( seed reps -- ) : seed-parameterized DoE entry point )
: L8-DOE ( seed reps -- )
  DUP 0= IF 2DROP ." L8-DOE: reps=0, aborted" CRLF EXIT THEN
  DUP 200 > IF 2DROP ." L8-DOE: reps>200, aborted" CRLF EXIT THEN
  DOE-REPS !
  DUP DOE-SEED ! SEED
  N-CFG DOE-REPS @ * DOE-RUNS !
  0 RUN-ID !
  ." L8-DOE: SEED=" DOE-SEED @ N. ."  REPS=" DOE-REPS @ N. CRLF
  ."   Generating matrix..." CRLF
  INIT-RM INIT-WL
  ."   Shuffling..." CRLF
  SHUFFLE-RM SHUFFLE-WL
  ."   Executing..." CRLF
  DOE-RUNS @ 0 DO
    I ONE-RUN
    I 1 + N-CFG MOD 0 = IF
      ." REP " I 1 + N-CFG / N. CRLF
    THEN
  LOOP
  ."   Done. " DOE-RUNS @ N. ."  runs complete" CRLF ;
