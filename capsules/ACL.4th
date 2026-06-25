Block 4000
( ACL.4th - Word-Level Access Control for StarForth )
( DictEntry fields: acl_ttl acl_allow acl_mode acl_pinned )
( C prims: ACL-MODE@ ACL-MODE! ACL-PINNED? ACL-PIN )
( ACL-TTL@ ACL-TTL! ACL-ALLOW@ ACL-ALLOW! )
( ACL-HEAT@ ACL-WORD-ID ACL-INHERIT ACL-INIT-PRIMITIVES )
( Policy words (this file): ACL-STRICT ACL-TTL-MODE )
( ACL-TTL-COMPUTE ACL-RECHECK ACL-ENTRY ACL-BOOT )
( Self-activating: init.4th only needs S" ACL.4th" EXEC )
( Comment out that line in init.4th for no-security mode. )
1 CONSTANT ACL-STRICT-MODE
0 CONSTANT ACL-TTL-MODE-VAL
256 CONSTANT ACL-BASE-TTL
65535 CONSTANT ACL-MAX-TTL

Block 4001
( ACL-ENTRY ( xt -- xt )                              )
( xt IS the DictEntry pointer in StarForth. ACL-ENTRY )
( is the identity word provided for API symmetry.     )
: ACL-ENTRY ( xt -- xt ) ;

Block 4002
( Mode selector words -- pin-guarded )
: ACL-STRICT ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-STRICT-MODE SWAP ACL-MODE! ;

: ACL-TTL-MODE ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-TTL-MODE-VAL SWAP ACL-MODE! ;

Block 4003
( ACL-TTL-COMPUTE ( xt -- ttl )                       )
( Adaptive TTL from execution heat. Hotter words earn )
( a longer TTL so recheck cost is amortised.          )
( Formula: heat/4 + ACL-BASE-TTL (256), cap ACL-MAX-TTL. )
: ACL-TTL-COMPUTE ( xt -- ttl )
  ACL-HEAT@
  4 /
  ACL-BASE-TTL +
  ACL-MAX-TTL MIN ;

Block 4004
( ACL-RECHECK ( xt -- )                               )
( Cold-path policy called by C acl_recheck() at TTL=0.)
( STRICT: always allow, TTL stays 0 (recheck always). )
( TTL:    compute TTL from heat; set allow=1.         )
: ACL-RECHECK ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  DUP ACL-MODE@ ACL-STRICT-MODE = IF
    1 OVER ACL-ALLOW!
    0 OVER ACL-TTL!
    DROP EXIT
  THEN
  DUP ACL-TTL-COMPUTE OVER ACL-TTL!
  1 SWAP ACL-ALLOW! ;

Block 4005
( ACL-BOOT ( -- )                                     )
( Stamps default ACL on all existing dict words, then )
( pins privileged words and ACL words themselves.     )
( BIRTH/CAPSULE-BIRTH are omitted: kernel-only, not in )
( hosted VM. Pinned in C (kernel_main.c) after capsule )
( load instead, so this file stays host-portable.      )
: ACL-BOOT ( -- )
  ACL-INIT-PRIMITIVES
  ['] EXEC   ACL-STRICT  ['] EXEC   ACL-PIN
  ['] BYE    ACL-STRICT  ['] BYE    ACL-PIN
  ['] ACL-RECHECK        ACL-PIN
  ['] ACL-INIT-PRIMITIVES ACL-PIN ;
' ACL-BOOT ACL-PIN

Block 4006
( CA ROOT - Ed25519 public key of system CA.          )
( Capsule hash IS the root-of-trust fingerprint; any  )
( change to CA changes hash and birth-protocol rejects)
( the tampered image.                                 )
( FUTURE: Replace placeholders at build time via      )
( tools/mkcapsule. Two 16-bit halves for portability. )
( HUMAN-REVIEW: Verify CA key matches build manifest. )
0 CONSTANT ACL-CA-KEY-LO
0 CONSTANT ACL-CA-KEY-HI

Block 4007
( Self-activation placeholder - actual call is in Block 4015 )
( ACL-BOOT-RW and helpers are defined in Blocks 4010-4014.   )
( init.4th only needs: S" ACL.4th" EXEC                      )

Block 4010
( ACL Rolling Window of Truth - mirrors Loop #2+#3+#6 )
( Ring buffer depth=8; tick stamps per recheck. )
( Slope inference drives TTL cooling; bounded. )
( C prims: ACL-RWT-TICK@ ACL-RWT-PUSH )
(          ACL-RWT-COUNT@ ACL-RWT-INTERVAL@ )
(          ACL-RWT-SLOPE@ ACL-RWT-SLOPE! )
( Policy: ACL-RWT-SLOPE-COMPUTE ACL-TTL-COMPUTE-RW )
( ACL-RECHECK-RW ACL-RW-MODE ACL-BOOT-RW )

Block 4011
( ACL-RWT-SLOPE-COMPUTE ( xt -- slope_q8 ) )
( Q8 slope from ring buffer interval series. )
( slope=(newest-oldest)<<8/(count-2) )
( +: intervals growing -> TTL grows. )
( -: intervals shrinking -> TTL shrinks. )
( Returns 0 if count<3 (need 2 intervals). )
: ACL-RWT-SLOPE-COMPUTE ( xt -- slope_q8 )
  DUP ACL-RWT-COUNT@ 3 < IF DROP 0 EXIT THEN
  DUP 0 SWAP ACL-RWT-INTERVAL@    ( xt newest )
  OVER ACL-RWT-COUNT@ 2 - 1-      ( xt newest n_oldest )
  ROT ACL-RWT-INTERVAL@            ( newest oldest )
  SWAP OVER -                      ( oldest delta )
  256 *                            ( oldest delta<<8 )
  SWAP 1 MAX /                     ( slope_q8 ) ;

Block 4012
( ACL-TTL-COMPUTE-RW ( xt -- ttl )                  )
( count<2: MAX-TTL (open). count=2: interval as TTL. )
( count>=3: mean*((256+slope)>>8). Clamp to range.   )
: ACL-TTL-COMPUTE-RW ( xt -- ttl )
  DUP ACL-RWT-COUNT@ 2 < IF DROP ACL-MAX-TTL EXIT THEN
  DUP ACL-RWT-COUNT@ 2 = IF
    0 SWAP ACL-RWT-INTERVAL@
    ACL-BASE-TTL MAX ACL-MAX-TTL MIN EXIT THEN
  DUP ACL-RWT-SLOPE@ 256 +
  SWAP
  DUP ACL-RWT-COUNT@ 1- 0
  DO I OVER ACL-RWT-INTERVAL@ + LOOP
  SWAP ACL-RWT-COUNT@ 1- /
  * 256 /
  ACL-BASE-TTL MAX ACL-MAX-TTL MIN ;

Block 4013
( ACL-RECHECK-RW: pin-guarded rolling-window recheck )
: ACL-RECHECK-RW ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  DUP ACL-MODE@ ACL-STRICT-MODE = IF
    1 OVER ACL-ALLOW!
    0 SWAP ACL-TTL!
    EXIT
  THEN
  ACL-RWT-TICK@ OVER ACL-RWT-PUSH
  DUP ACL-RWT-COUNT@ 3 >= IF
    DUP ACL-RWT-SLOPE-COMPUTE OVER ACL-RWT-SLOPE!
  THEN
  DUP ACL-TTL-COMPUTE-RW OVER ACL-TTL!
  1 SWAP ACL-ALLOW! ;

Block 4014
( ACL-RW-MODE ( xt -- ) pin-guarded. )
( Switch word to rolling-window recheck policy. )
( Sets mode=TTL. ACL-BOOT-RW replaces ACL-BOOT. )
: ACL-RW-MODE ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-TTL-MODE-VAL SWAP ACL-MODE! ;

: ACL-BOOT-RW ( -- )
  ACL-INIT-PRIMITIVES
  ['] EXEC   ACL-STRICT  ['] EXEC   ACL-PIN
  ['] BYE    ACL-STRICT  ['] BYE    ACL-PIN
  ['] ACL-RECHECK-RW      ACL-PIN
  ['] ACL-INIT-PRIMITIVES ACL-PIN ;
' ACL-BOOT-RW ACL-PIN

Block 4015
( Self-activation - runs after all ACL words are defined )
ACL-BOOT-RW
S" zuse.4th" EXEC
