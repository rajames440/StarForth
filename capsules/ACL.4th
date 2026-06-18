Block 4000
( ACL.4th - Word-Level Access Control for StarForth )
( C fields per DictEntry: acl_ttl acl_allow acl_mode acl_pinned )
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
  ' EXEC   ACL-STRICT  ' EXEC   ACL-PIN
  ' BYE    ACL-STRICT  ' BYE    ACL-PIN
  ' ACL-RECHECK        ACL-PIN
  ' ACL-INIT-PRIMITIVES ACL-PIN
  ' ACL-BOOT           ACL-PIN ;

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
( Self-activation moved to Block 4015 — must run after ACL-BOOT-RW )

Block 4010
( ACL Rolling Window of Truth — topology mirrors Loop #2+#3+#6    )
( Ring buffer (depth 8) stores heartbeat tick stamps per recheck.  )
( Slope inference drives TTL cooling rate. Negative feedback keeps )
( the signal bounded. Starts OPEN (MAX TTL) and finds attractors. )
( New C prims: ACL-RWT-TICK@ ACL-RWT-PUSH ACL-RWT-COUNT@          )
(              ACL-RWT-INTERVAL@ ACL-RWT-SLOPE@ ACL-RWT-SLOPE!    )
( Policy words: ACL-RWT-SLOPE-COMPUTE ACL-TTL-COMPUTE-RW          )
(               ACL-RECHECK-RW ACL-RW-MODE ACL-BOOT-RW            )

Block 4011
( ACL-RWT-SLOPE-COMPUTE ( xt -- slope_q8 )                        )
( Compute Q8 slope from the interval series in the ring buffer.    )
( slope_q8 = ( newest_interval - oldest_interval ) << 8           )
(            / ( count - 2 )                                       )
( Positive: intervals growing  = word heating up  -> TTL grows.    )
( Negative: intervals shrinking = word cooling   -> TTL shrinks.   )
( Requires count >= 3 (two intervals). Returns 0 if insufficient.  )
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
( ACL-RECHECK-RW ( xt -- )                                        )
( Rolling-window recheck: push current tick, update slope, set TTL.)
( Pin-guarded. STRICT mode resets TTL to 0 always.                )
: ACL-RECHECK-RW ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  DUP ACL-MODE@ ACL-STRICT-MODE = IF
    1 OVER ACL-ALLOW!
    0 SWAP ACL-TTL!
    EXIT
  THEN
  ( Push current tick stamp into ring buffer )
  ACL-RWT-TICK@ OVER ACL-RWT-PUSH
  ( Update slope if we have enough history )
  DUP ACL-RWT-COUNT@ 2 > IF
    DUP ACL-RWT-SLOPE-COMPUTE OVER ACL-RWT-SLOPE!
  THEN
  ( Compute and apply new TTL )
  DUP ACL-TTL-COMPUTE-RW OVER ACL-TTL!
  1 SWAP ACL-ALLOW! ;

Block 4014
( ACL-RW-MODE ( xt -- )                                           )
( Switch a word to use the rolling-window recheck policy.         )
( Pin-guarded. Sets mode to TTL (RW uses same TTL slot as heat).  )
( ACL-BOOT-RW ( -- ) replaces ACL-BOOT for RW mode globally.     )
: ACL-RW-MODE ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-TTL-MODE-VAL SWAP ACL-MODE! ;

: ACL-BOOT-RW ( -- )
  ACL-INIT-PRIMITIVES
  ' EXEC   ACL-STRICT  ' EXEC   ACL-PIN
  ' BYE    ACL-STRICT  ' BYE    ACL-PIN
  ' ACL-RECHECK-RW      ACL-PIN
  ' ACL-INIT-PRIMITIVES ACL-PIN
  ' ACL-BOOT-RW         ACL-PIN ;

Block 4015
( Self-activation - runs at capsule load time )
( Block 4015 ensures ACL-BOOT-RW is defined before call )
( init.4th only needs: S" ACL.4th" EXEC                )
ACL-BOOT-RW
S" zuse.4th" EXEC
