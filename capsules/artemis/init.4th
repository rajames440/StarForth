Block 4110
( Artemis flat pool disk manager )
( WARNING: no fs layer -- bugs can destroy disk/artemis.img )
( Disk: LBN 3072..26073 via virtio-blk = 23002 blocks )
( Hdr: 3072  Fmap: 3073-3075  Data: 3076..26073 )
3072 CONSTANT ART-HDR-LBN
3073 CONSTANT ART-FM-LBN
   3 CONSTANT ART-FM-BLKS
3076 CONSTANT ART-DATA-LBN
22998 CONSTANT ART-DATA-BLKS
VARIABLE ART-K
: ART-K+! ART-K @ 1+ ART-K ! ;
: ART-K-! ART-K @ 1- ART-K ! ;
Block 4111
( Arch-neutral little-endian byte I/O )
: LE32! ( u addr -- )
  OVER       255 AND OVER C! 1+
  OVER 256 / 255 AND OVER C! 1+
  OVER 65536 / 255 AND OVER C! 1+
  SWAP 16777216 / 255 AND SWAP C! ;
: LE32@ ( addr -- u )
  DUP      C@
  OVER  1+ C@  256 *     +
  OVER  2+ C@  65536 *   +
  SWAP  3+ C@  16777216 * + ;
: LE64! ( u addr -- )
  2DUP LE32!
  SWAP 4294967296 / SWAP 4+ LE32! ;
: LE64@ ( addr -- u )
  DUP  LE32@
  SWAP 4+ LE32@
  4294967296 * + ;
Block 4112
( Bit ops -- 2*/2/ loops; no LSHIFT/RSHIFT in vocabulary )
: SHL1N ( u n -- u' ) 0 ?DO 2* LOOP ;
: SHR1N ( u n -- u' ) 0 ?DO 2/ LOOP ;
: BIT-TEST ( byte bit# -- flag )
  SHR1N 1 AND ;
: BIT-SET ( byte bit# -- byte' )
  1 SWAP SHL1N OR ;
: BIT-CLR ( byte bit# -- byte' )
  1 SWAP SHL1N INVERT AND ;
Block 4113
( Free map: bit array across LBN 3073-3075 )
( n = 0-based data block index; n=0 -> LBN 3076 )
: FM-ADDR-BIT ( n -- bit# addr )
  DUP 7 AND
  SWAP 8 /
  DUP 1024 / ART-FM-LBN + BLOCK
  SWAP 1024 MOD + ;
: FM-TEST ( n -- flag )
  FM-ADDR-BIT C@ SWAP BIT-TEST ;
: FM-SET ( n -- )
  FM-ADDR-BIT DUP C@ ROT BIT-SET SWAP C! UPDATE ;
: FM-CLR ( n -- )
  FM-ADDR-BIT DUP C@ ROT BIT-CLR SWAP C! UPDATE ;
Block 4122
( Block allocator -- first-free linear scan )
( NOTE: Hermes owns 4114-4121; Artemis resumes at 4122 )
: FM-FIND-FREE ( -- n|-1 )
  ART-DATA-BLKS 0 DO
    I FM-TEST 0= IF I UNLOOP EXIT THEN
  LOOP -1 ;
: BLK-ALLOC ( -- lbn|-1 )
  FM-FIND-FREE DUP -1 = IF EXIT THEN
  DUP FM-SET ART-DATA-LBN + ART-K+! ;
: BLK-FREE ( lbn -- )
  ART-DATA-LBN -
  DUP 0 < IF DROP EXIT THEN
  DUP ART-DATA-BLKS >= IF DROP EXIT THEN
  DUP FM-TEST 0= IF DROP EXIT THEN
  FM-CLR ART-K-! ;
Block 4123
( Artemis magic and blank detection )
( Magic = "ARTEMIS\0" = 41 52 54 45 4D 49 53 00 )
: ART-MAGIC! ( addr -- )
  65 OVER C! 1+  82 OVER C! 1+
  84 OVER C! 1+  69 OVER C! 1+
  77 OVER C! 1+  73 OVER C! 1+
  83 OVER C! 1+   0 SWAP C! ;
: ART-MAGIC? ( addr -- flag )
  DUP      C@ 65 =
  OVER 1+  C@ 82 = AND
  OVER 2+  C@ 84 = AND
  OVER 3+  C@ 69 = AND
  OVER 4+  C@ 77 = AND
  OVER 5+  C@ 73 = AND
  OVER 6+  C@ 83 = AND
  SWAP 7+  C@  0 = AND ;
: ART-BLANK? ( addr -- flag )
  1024 0 DO
    DUP I + C@ 0<> IF
      DROP FALSE UNLOOP EXIT
    THEN
  LOOP DROP TRUE ;
Block 4124
( Boot detection and block I/O )
0 CONSTANT ART-BLANK-DISK
1 CONSTANT ART-LITHOS-DISK
2 CONSTANT ART-UNRECOG-DISK
: ART-BOOT-DETECT ( -- mode )
  ART-HDR-LBN BLOCK
  DUP ART-MAGIC? IF DROP ART-LITHOS-DISK EXIT THEN
  DUP ART-BLANK? IF DROP ART-BLANK-DISK  EXIT THEN
  DROP ART-UNRECOG-DISK ;
: BLK-FETCH ( lbn -- addr ) BLOCK ;
: BLK-PERSIST ( lbn -- ) BLOCK DROP UPDATE ;
: ART-FLUSH ( -- ) FLUSH ;
Block 4125
( Artemis header write -- called on first format )
( Layout: [0-7] magic  [8-11] ver  [12-15] fm-blks )
(         [16-23] data-start  [24-31] total  [32-39] free )
: ART-HDR-WRITE ( -- )
  ART-HDR-LBN BUFFER
  DUP 1024 0 FILL
  DUP ART-MAGIC!
  DUP  8 + 1            SWAP LE32!
  DUP 12 + ART-FM-BLKS  SWAP LE32!
  DUP 16 + ART-DATA-LBN  SWAP LE64!
  DUP 24 + ART-DATA-BLKS SWAP LE64!
  DUP 32 + ART-DATA-BLKS SWAP LE64!
  DROP UPDATE ;
Block 4126
( Boot sequence )
: ART-FORMAT ( -- )
  ." Artemis: blank disk -- formatting" CR
  ART-HDR-WRITE FLUSH ;
: ART-RESUME ( -- )
  ." Artemis: LithosAnanke disk -- resuming" CR ;
: ART-HALT-UNRECOG ( -- )
  ." ARTEMIS HALT: unrecognized disk content" CR
  ." Disk preserved. Manual intervention required." CR
  ABORT ;
: ART-INIT ( -- )
  0 ART-K !
  ART-BOOT-DETECT
  CASE
    ART-BLANK-DISK   OF ART-FORMAT       ENDOF
    ART-LITHOS-DISK  OF ART-RESUME       ENDOF
    ART-UNRECOG-DISK OF ART-HALT-UNRECOG ENDOF
  ENDCASE ;
Block 4127
( Status, self-test )
: ART-STATUS ( -- )
  ." Artemis: " ART-DATA-BLKS . ." data blocks" CR
  ." Data LBN: " ART-DATA-LBN . ." K=" ART-K @ . CR ;
: ART-SELF-TEST ( -- )
  ." Artemis self-test..." CR
  BLK-ALLOC DUP -1 = IF
    ." FAIL: no free blocks" CR DROP EXIT
  THEN
  DUP . ." allocated" CR
  DUP BLK-FETCH DROP
  DUP BLK-PERSIST
  ART-K @ . ." K after alloc" CR
  BLK-FREE
  ART-K @ . ." K after free" CR
  ART-FLUSH
  ." PASS" CR ;
: WELCOME ( -- ) ." Artemis ready" CR ART-STATUS ;
Block 4129
( Artemis entry -- runs at capsule load )
ART-INIT
ART-SELF-TEST
WELCOME
