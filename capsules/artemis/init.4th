Block 4110
( Artemis — block storage manager Phase 7 )
( Manages LBN 4501..4599 via BLOCK BUFFER UPDATE FLUSH )
( In-VM free map; ART-FLUSH writes dirty blocks. )
: WELCOME ." Welcome to Artemis" CR ;
: CONNECT-HERA BYE ;
4501 CONSTANT ART-BASE
  99 CONSTANT ART-CAP
CREATE ART-FREE ART-CAP CELLS ALLOT
: ART-INIT ( -- ) ART-FREE ART-CAP CELLS 0 FILL ;
ART-INIT
Block 4111
( BLK-ALLOC: first free slot, mark used, return LBN or -1 )
: BLK-ALLOC ( -- lbn )
  ART-CAP 0 DO
    I CELLS ART-FREE + @ 0= IF
      1 I CELLS ART-FREE + !
      I ART-BASE + UNLOOP EXIT
    THEN
  LOOP -1 ;
( BLK-FREE: release block by LBN; bounds-checked )
: BLK-FREE ( lbn -- )
  ART-BASE -
  DUP 0 >= OVER ART-CAP < AND IF
    CELLS ART-FREE + 0 SWAP !
  ELSE DROP THEN ;
Block 4112
( Artemis block operations and status )
: BLK-FETCH ( lbn -- addr ) BLOCK ;
: BLK-PERSIST ( lbn -- ) BLOCK DROP UPDATE ;
: ART-FLUSH ( -- ) FLUSH ;
: ART-STATUS ( -- )
  ." Artemis: " ART-CAP . ." blocks managed (LBN "
  ART-BASE . ." .." ART-BASE ART-CAP + 1- . ." )" CR
  0 ART-CAP 0 DO I CELLS ART-FREE + @ 0<> IF 1+ THEN LOOP
  ." In use: " . CR ;
Block 4113
( ART-SELF-TEST: alloc/touch/persist/free one block )
: ART-SELF-TEST ( -- )
  ." Artemis storage self-test" CR
  BLK-ALLOC DUP -1 = IF ." FAIL: no free" CR DROP EXIT THEN
  DUP . ." allocated" CR
  DUP BLK-FETCH DROP
  DUP BLK-PERSIST
  BLK-FREE
  ART-FLUSH
  ." PASS: alloc/fetch/persist/free/flush OK" CR ;
: LOAD-DOE ( -- ) S" doe.4th" EXEC ;
WELCOME
