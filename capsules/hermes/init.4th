Block 4100
( Hermes v1 — constants )
1 CONSTANT SPAWN-EVENT
2 CONSTANT PAUSE-EVENT
3 CONSTANT RESUME-EVENT
4 CONSTANT KILL-EVENT
0 CONSTANT CH-NEGOTIATING
1 CONSTANT CH-OPEN
2 CONSTANT CH-CLOSING
9 CONSTANT MSG-CELLS
6 CONSTANT CH-CELLS
2 CONSTANT MBR-CELLS
32 CONSTANT MSG-MAX
16 CONSTANT CH-MAX
64 CONSTANT MBR-MAX
65208 CONSTANT Q-DECAY
255 CONSTANT MSG-DELIVERED
Block 4101
( Hermes v1 — arenas and free-list roots )
CREATE MSG-ARENA MSG-MAX MSG-CELLS * CELLS ALLOT
CREATE CH-ARENA  CH-MAX  CH-CELLS  * CELLS ALLOT
CREATE MBR-ARENA MBR-MAX MBR-CELLS * CELLS ALLOT
VARIABLE MSG-FREE-HEAD
VARIABLE CH-FREE-HEAD
VARIABLE MBR-FREE-HEAD
VARIABLE MSG-SEQ
VARIABLE CH-ACTIVE
Block 4102
( Hermes v1 — MSG-INIT-FREE CH-INIT-FREE )
: MSG-INIT-FREE ( -- )
  MSG-MAX 1- 0 DO
    I MSG-CELLS * CELLS MSG-ARENA +
    I 1+ MSG-CELLS * CELLS MSG-ARENA + SWAP !
  LOOP
  0 MSG-MAX 1- MSG-CELLS * CELLS MSG-ARENA + !
  MSG-ARENA MSG-FREE-HEAD ! ;
: CH-INIT-FREE ( -- )
  CH-MAX 1- 0 DO
    I CH-CELLS * CELLS CH-ARENA +
    I 1+ CH-CELLS * CELLS CH-ARENA + SWAP !
  LOOP
  0 CH-MAX 1- CH-CELLS * CELLS CH-ARENA + !
  CH-ARENA CH-FREE-HEAD ! ;
Block 4103
( Hermes v1 — MBR-INIT-FREE MSG alloc/free )
: MBR-INIT-FREE ( -- )
  MBR-MAX 1- 0 DO
    I MBR-CELLS * CELLS MBR-ARENA +
    I 1+ MBR-CELLS * CELLS MBR-ARENA + SWAP !
  LOOP
  0 MBR-MAX 1- MBR-CELLS * CELLS MBR-ARENA + !
  MBR-ARENA MBR-FREE-HEAD ! ;
: MSG-ALLOC ( -- addr|0 )
  MSG-FREE-HEAD @ DUP 0= IF EXIT THEN
  DUP @ MSG-FREE-HEAD !
  DUP MSG-CELLS CELLS 0 FILL ;
: MSG-FREE-NODE ( addr -- )
  MSG-FREE-HEAD @ OVER ! MSG-FREE-HEAD ! ;
Block 4104
( Hermes v1 — CH MBR alloc/free )
: CH-ALLOC ( -- addr|0 )
  CH-FREE-HEAD @ DUP 0= IF EXIT THEN
  DUP @ CH-FREE-HEAD !
  DUP CH-CELLS CELLS 0 FILL ;
: CH-FREE-NODE ( addr -- )
  CH-FREE-HEAD @ OVER ! CH-FREE-HEAD ! ;
: MBR-ALLOC ( -- addr|0 )
  MBR-FREE-HEAD @ DUP 0= IF EXIT THEN
  DUP @ MBR-FREE-HEAD !
  DUP MBR-CELLS CELLS 0 FILL ;
: MBR-FREE-NODE ( addr -- )
  MBR-FREE-HEAD @ OVER ! MBR-FREE-HEAD ! ;
Block 4105
( Hermes v1 — message field accessors )
: MSG-TYPE@  ( m -- n ) @ ;
: MSG-TYPE!  ( n m -- ) ! ;
: MSG-FROM@  ( m -- n ) 1 CELLS + @ ;
: MSG-FROM!  ( n m -- ) 1 CELLS + ! ;
: MSG-TO@    ( m -- n ) 2 CELLS + @ ;
: MSG-TO!    ( n m -- ) 2 CELLS + ! ;
: MSG-PADDR@ ( m -- a ) 3 CELLS + @ ;
: MSG-PADDR! ( a m -- ) 3 CELLS + ! ;
: MSG-PLEN@  ( m -- u ) 4 CELLS + @ ;
: MSG-PLEN!  ( u m -- ) 4 CELLS + ! ;
: MSG-HEAT@  ( m -- q ) 5 CELLS + @ ;
: MSG-HEAT!  ( q m -- ) 5 CELLS + ! ;
: MSG-SEQ@   ( m -- n ) 6 CELLS + @ ;
: MSG-SEQ!   ( n m -- ) 6 CELLS + ! ;
Block 4122
( Hermes v1 — message accessors: CH@/CH! )
: MSG-CH@         ( m -- c ) 7 CELLS + @ ;
: MSG-CH!         ( c m -- ) 7 CELLS + ! ;
: MSG-ORIG-TYPE@  ( m -- n ) 8 CELLS + @ ;
: MSG-ORIG-TYPE!  ( n m -- ) 8 CELLS + ! ;
253 CONSTANT MSG-NACKED
Block 4106
( Hermes v1 — channel + member accessors )
: CH-ID@    ( c -- n ) @ ;
: CH-ID!    ( n c -- ) ! ;
: CH-OWNER@ ( c -- n ) 1 CELLS + @ ;
: CH-OWNER! ( n c -- ) 1 CELLS + ! ;
: CH-STATE@ ( c -- n ) 2 CELLS + @ ;
: CH-STATE! ( n c -- ) 2 CELLS + ! ;
: CH-HEAT@  ( c -- q ) 3 CELLS + @ ;
: CH-HEAT!  ( q c -- ) 3 CELLS + ! ;
: CH-MBRS@  ( c -- a ) 4 CELLS + @ ;
: CH-MBRS!  ( a c -- ) 4 CELLS + ! ;
: CH-NEXT@  ( c -- a ) 5 CELLS + @ ;
: CH-NEXT!  ( a c -- ) 5 CELLS + ! ;
: MBR-NEXT@ ( m -- a ) @ ;
: MBR-VM@   ( m -- n ) 1 CELLS + @ ;
Block 4107
( Hermes v1 — deliver )
VARIABLE MSG-LAST-MSG
: IDX>NAME ( idx -- addr u )
  DUP 0 = IF DROP S" Hera"    EXIT THEN
  DUP 1 = IF DROP S" Hermes"  EXIT THEN
  DROP S" Artemis" ;
: MSG-DELIVER ( m -- )
  DUP MSG-LAST-MSG !
  DUP MSG-PADDR@ OVER MSG-PLEN@
  ROT MSG-TO@ IDX>NAME VM-EXEC ;
Block 4123
( Hermes v1 — MSG-SEND )
: MSG-SEND ( type from to paddr plen ch -- )
  MSG-ALLOC DUP 0= IF 2DROP 2DROP 2DROP DROP EXIT THEN
  >R
  MSG-SEQ @ 1+ DUP MSG-SEQ ! R@ MSG-SEQ!
  R@ MSG-CH!
  R@ MSG-PLEN! R@ MSG-PADDR!
  R@ MSG-TO!   R@ MSG-FROM! DUP R@ MSG-TYPE! R@ MSG-ORIG-TYPE!
  Q.1 R@ MSG-HEAT! R> DROP ;
Block 4108
( Hermes v1 — message cooling )
VARIABLE MSG-SCAN
: MSG-COOL-ONE ( m -- )
  DUP MSG-HEAT@ Q-DECAY Q.* SWAP MSG-HEAT! ;
: MSG-COOL-ALL ( -- )
  MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-HEAT@ 0 > IF
      MSG-SCAN @ MSG-COOL-ONE
    THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
Block 4124
( Hermes v1 — MSG-TOTAL-HEAT )
: MSG-TOTAL-HEAT ( -- q48 )
  0 MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-TYPE@ 0 <> IF
      MSG-SCAN @ MSG-HEAT@ +
    THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
: MSG-REDELIVER-NACKED ( -- )
  MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-TYPE@ MSG-NACKED = IF
      MSG-SCAN @ MSG-ORIG-TYPE@ MSG-SCAN @ MSG-TYPE! THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
Block 4128
( Hermes v1 — MSG-DELIVER-ALL )
: MSG-DELIVER-ALL ( -- )
  MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-TYPE@ 0 <>
    MSG-SCAN @ MSG-TYPE@ MSG-DELIVERED <> AND
    MSG-SCAN @ MSG-TYPE@ MSG-NACKED <> AND IF
      MSG-SCAN @ MSG-DELIVER
      MSG-SCAN @ MSG-TYPE@ 0 <> IF
        MSG-DELIVERED MSG-SCAN @ MSG-TYPE!
      THEN
    THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
Block 4109
( Hermes v1 — message reaping )
( K reap only fires at heat=0: freed K contribution is 0. )
( Force-reap not yet implemented. If added: explicit )
( K redistribution will be required here. )
: MSG-REAP ( -- )
  MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-HEAT@ 0 = IF
      MSG-SCAN @ MSG-TYPE@ 0 <> IF
        0 MSG-SCAN @ MSG-TYPE!
        MSG-SCAN @ MSG-FREE-NODE
      THEN
    THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
Block 4114
( Hermes v1 — channel cooling + heat aggregate )
VARIABLE CH-SCAN
: CH-COOL-ALL ( -- )
  CH-ACTIVE @ CH-SCAN !
  BEGIN CH-SCAN @ 0 <> WHILE
    CH-SCAN @ CH-HEAT@ Q-DECAY Q.* CH-SCAN @ CH-HEAT!
    CH-SCAN @ CH-NEXT@ CH-SCAN !
  REPEAT ;
: CH-TOTAL-HEAT ( -- q48 )
  0 CH-ACTIVE @ CH-SCAN !
  BEGIN CH-SCAN @ 0 <> WHILE
    CH-SCAN @ CH-HEAT@ +
    CH-SCAN @ CH-NEXT@ CH-SCAN !
  REPEAT ;
Block 4115
( Hermes v1 — channel reaping )
: CH-REAP-SAFE ( -- )
  CH-ACTIVE @ CH-SCAN !
  0 CH-ACTIVE !
  BEGIN CH-SCAN @ 0 <> WHILE
    CH-SCAN @ CH-NEXT@
    CH-SCAN @ CH-HEAT@ 0 =
    CH-SCAN @ COMMON-CH @ <> AND IF
      CH-SCAN @ CH-FREE-NODE
    ELSE
      CH-ACTIVE @ CH-SCAN @ CH-NEXT!
      CH-SCAN @ CH-ACTIVE !
    THEN
    CH-SCAN !
  REPEAT ;
Block 4116
( Hermes v1 — COMMON channel + HERMES-TICK )
( COMMON floor = Q.1/3: Hermes's fair share, VM-COUNT=3 )
VARIABLE COMMON-CH
: COMMON-INIT ( -- )
  CH-ALLOC DUP COMMON-CH !
  0 OVER CH-ID!   0 OVER CH-OWNER!
  CH-OPEN OVER CH-STATE!
  Q.1 3 / OVER CH-HEAT!
  0 OVER CH-MBRS!
  CH-ACTIVE @ OVER CH-NEXT!
  CH-ACTIVE ! ;
: HERMES-TICK ( -- )
  MSG-DELIVER-ALL MSG-REDELIVER-NACKED
  MSG-COOL-ALL MSG-REAP
  CH-COOL-ALL  CH-REAP-SAFE
  Q.1 3 / COMMON-CH @ CH-HEAT! ;
Block 4125
( Hermes v1 — HERMES-K + WELCOME )
: HERMES-K ( -- q48 )
  MSG-TOTAL-HEAT CH-TOTAL-HEAT + ;
: WELCOME ( -- ) LOG-INFO" Hermes: loaded" ;
WELCOME
Block 4117
( Hermes v1 — event compat interface )
: EVENT-EMIT ( type -- )
  DUP SPAWN-EVENT = IF DROP HERA-NOTIFY-SPAWN EXIT THEN
  DUP KILL-EVENT  = IF DROP HERA-NOTIFY-KILL  EXIT THEN
  DROP ;
: EVENT-WAIT ( -- type )
  MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-TYPE@ 0 <> IF
      MSG-SCAN @ MSG-TYPE@ UNLOOP EXIT THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP 0 ;
: EVENT-DRAIN ( -- )
  ( no-op: MSG-REAP owns cleanup; drain breaks async ) ;
Block 4118
( Hermes v1 — Hera notification )
: HERA-NOTIFY-SPAWN ( -- )
  S" K-SPAWN-HOOK" S" Hera" VM-EXEC ;
: HERA-NOTIFY-KILL ( -- )
  S" K-KILL-HOOK" S" Hera" VM-EXEC ;
Block 4119
( Hermes v1 — channel negotiation: mint/request )
: CH-MINT-ID ( owner -- id )
  MSG-SEQ @ 1+ DUP MSG-SEQ !
  SWAP 32 LSHIFT OR ;
: CH-REQUEST ( type from to paddr plen -- )
  COMMON-CH @ CH-STATE@ CH-OPEN = IF
    COMMON-CH @ MSG-SEND
  ELSE 2DROP 2DROP DROP THEN ;
Block 4126
( Hermes v1 — channel ops: accept confirm close )
: CH-ACCEPT ( -- ch|0 )
  CH-ALLOC DUP 0= IF EXIT THEN
  1 CH-MINT-ID OVER CH-ID!
  1 OVER CH-OWNER!
  CH-NEGOTIATING OVER CH-STATE!
  Q.1 OVER CH-HEAT!
  0 OVER CH-MBRS!
  CH-ACTIVE @ OVER CH-NEXT!
  DUP CH-ACTIVE ! ;
: CH-CONFIRM ( ch -- )
  DUP CH-STATE@ CH-NEGOTIATING = IF CH-OPEN SWAP CH-STATE!
  ELSE DROP THEN ;
: CH-CLOSE ( ch -- )
  DUP COMMON-CH @ = IF DROP EXIT THEN
  CH-CLOSING SWAP CH-STATE! ;
Block 4120
( Hermes v1 — CD-INIT )
: CD-INIT ( -- )
  MSG-ARENA MSG-MAX MSG-CELLS * CELLS 0 FILL
  CH-ARENA  CH-MAX  CH-CELLS  * CELLS 0 FILL
  MBR-ARENA MBR-MAX MBR-CELLS * CELLS 0 FILL
  MSG-INIT-FREE
  CH-INIT-FREE
  MBR-INIT-FREE
  0 MSG-SEQ !
  0 MSG-LAST-MSG !
  0 CH-ACTIVE !
  COMMON-INIT
  S" lib.4th" EXEC
  S" common:msg.4th" USE
  LOG-INFO" Hermes: ready" ;
Block 4121
( Hermes v1 — ACK/NACK server )
: MSG-ACK-LAST ( -- )
  MSG-LAST-MSG @ DUP 0= IF DROP EXIT THEN
  0 OVER MSG-TYPE! MSG-FREE-NODE ;
: MSG-NACK-LAST ( -- )
  MSG-LAST-MSG @ DUP 0= IF DROP EXIT THEN
  DUP MSG-HEAT@ 2 / OVER MSG-HEAT!
  MSG-NACKED SWAP MSG-TYPE! ;
Block 4127
( Hermes v1 — HERMES-STATUS )
: MSG-USED ( -- n )
  0 MSG-ARENA MSG-SCAN !
  MSG-MAX 0 DO
    MSG-SCAN @ MSG-TYPE@ 0 <> IF 1+ THEN
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
: CH-USED ( -- n )
  0 CH-ACTIVE @ CH-SCAN !
  BEGIN CH-SCAN @ 0 <> WHILE
    1+
    CH-SCAN @ CH-NEXT@ CH-SCAN !
  REPEAT ;
: HERMES-STATUS ( -- )
  MSG-USED . ." msgs " CH-USED . ." channels" CR ;
Block 4130
( Hermes v1 — member management )
: MBR-NEXT! ( a m -- ) ! ;
: MBR-VM!   ( n m -- ) 1 CELLS + ! ;
: CH-ADD-MBR ( vm ch -- )
  MBR-ALLOC DUP 0= IF DROP 2DROP EXIT THEN
  ROT OVER MBR-VM!
  OVER CH-MBRS@ OVER MBR-NEXT!
  SWAP CH-MBRS! ;
: HERMES-MSG-TEST ( -- flag )
  SPAWN-EVENT 1 0 0 0 COMMON-CH @ MSG-SEND
  MSG-USED 0 > ;
