Block 4110
( Artemis — block storage manager Phase 7 )
( Manages LBN 4501..4599 using FORTH block ABI: BLOCK BUFFER UPDATE FLUSH )
( In-VM free map; persists for Artemis lifetime. ART-FLUSH writes dirty blocks. )
: WELCOME ." Welcome to Artemis" CR ;
: CONNECT-HERA BYE ;
4501 CONSTANT ART-BASE
  99 CONSTANT ART-CAP
CREATE ART-FREE ART-CAP CELLS ALLOT
: ART-INIT ( -- ) ART-FREE ART-CAP CELLS 0 FILL ;
ART-INIT
Block 4111
( BLK-ALLOC: find first free slot, mark used, return LBN or -1 on full )
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
( BLK-FETCH: load block into buffer, return address )
: BLK-FETCH ( lbn -- addr ) BLOCK ;
( BLK-PERSIST: mark most-recently-accessed block dirty for next FLUSH )
: BLK-PERSIST ( lbn -- ) BLOCK DROP UPDATE ;
( ART-FLUSH: write all dirty blocks to the virtual disk )
: ART-FLUSH ( -- ) FLUSH ;
Block 4112
( ART-STATUS: report storage utilisation )
: ART-STATUS ( -- )
  ." Artemis: " ART-CAP . ." blocks managed (LBN "
  ART-BASE . ." .." ART-BASE ART-CAP + 1- . ." )" CR
  0 ART-CAP 0 DO I CELLS ART-FREE + @ 0<> IF 1+ THEN LOOP
  ." In use: " . CR ;
( ART-SELF-TEST: alloc/touch/persist/free one block — called from Hera )
: ART-SELF-TEST ( -- )
  ." Artemis storage self-test" CR
  BLK-ALLOC DUP -1 = IF ." FAIL: no free blocks" CR DROP EXIT THEN
  DUP . ." allocated" CR
  DUP BLK-FETCH DROP
  DUP BLK-PERSIST
  BLK-FREE
  ART-FLUSH
  ." PASS: BLK-ALLOC/FETCH/PERSIST/FREE/FLUSH OK" CR ;
: LOAD-DOE ( -- ) S" doe.4th" EXEC ;
WELCOME
