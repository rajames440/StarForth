Block 2060
( ACL.4th - Word-Level Access Control System for StarForth              )
( C fields per DictEntry: acl_ttl acl_allow acl_mode acl_pinned         )
( C primitives: ACL-MODE@ ACL-MODE! ACL-PINNED? ACL-PIN                 )
(               ACL-TTL@  ACL-TTL!  ACL-ALLOW@  ACL-ALLOW!              )
(               ACL-HEAT@ ACL-WORD-ID ACL-INHERIT ACL-INIT-PRIMITIVES   )
( Policy words (this file): ACL-STRICT ACL-TTL-MODE ACL-TTL-COMPUTE     )
(                            ACL-RECHECK ACL-ENTRY ACL-BOOT             )
0 CONSTANT ACL-STRICT-MODE   ( acl_mode value: re-check every execution )
1 CONSTANT ACL-TTL-MODE-VAL  ( acl_mode value: adaptive TTL countdown   )
16 CONSTANT ACL-BASE-TTL     ( minimum TTL after first recheck          )
65535 CONSTANT ACL-MAX-TTL   ( upper cap on TTL                         )

Block 2061
( ACL-ENTRY ( xt -- xt )                                                )
( In StarForth, xt IS the DictEntry pointer — ACL state lives in the    )
( DictEntry struct itself. ACL-ENTRY is the identity: addr = xt.        )
( Provided for API symmetry with the design doc.                        )
: ACL-ENTRY ( xt -- xt ) ;

Block 2062
( Mode selector words — pin-guarded                                     )
: ACL-STRICT ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-STRICT-MODE SWAP ACL-MODE! ;

: ACL-TTL-MODE ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  ACL-TTL-MODE-VAL SWAP ACL-MODE! ;

Block 2063
( ACL-TTL-COMPUTE ( xt -- ttl )                                         )
( Adaptive TTL derived from execution heat. Hotter words earn a longer  )
( TTL so the recheck cost is amortised across more executions.          )
( Formula: heat / 4 + ACL-BASE-TTL, capped at ACL-MAX-TTL.             )
: ACL-TTL-COMPUTE ( xt -- ttl )
  ACL-HEAT@            ( -- heat )
  4 /                  ( -- heat/4 )
  ACL-BASE-TTL +       ( -- raw-ttl )
  ACL-MAX-TTL MIN ;    ( -- ttl, capped )

Block 2064
( ACL-RECHECK ( xt -- )                                                 )
( Cold-path policy evaluation called by C acl_recheck() when TTL=0.    )
( Updates acl_allow and resets acl_ttl. Pinned words are immutable.    )
( STRICT mode: always allow, keep TTL at 0 so next exec rechecks.      )
( TTL mode:    compute new TTL from heat; set allow=1.                  )
: ACL-RECHECK ( xt -- )
  DUP ACL-PINNED? IF DROP EXIT THEN
  DUP ACL-MODE@ ACL-STRICT-MODE = IF
    1 OVER ACL-ALLOW!
    0 OVER ACL-TTL!
    DROP EXIT
  THEN
  ( TTL mode )
  DUP ACL-TTL-COMPUTE OVER ACL-TTL!
  1 SWAP ACL-ALLOW! ;

Block 2065
( ACL-BOOT ( -- )                                                       )
( Called from init.4th after loading ACL.4th. Stamps default ACL on    )
( all existing dictionary words, then pins the privileged kernel words  )
( and the ACL words themselves so they cannot be denied or re-pinned.   )
: ACL-BOOT ( -- )
  ACL-INIT-PRIMITIVES
  ' BIRTH  ACL-STRICT  ' BIRTH  ACL-PIN
  ' EXEC   ACL-STRICT  ' EXEC   ACL-PIN
  ' BYE    ACL-STRICT  ' BYE    ACL-PIN
  ' ACL-RECHECK      ACL-PIN
  ' ACL-INIT-PRIMITIVES ACL-PIN
  ' ACL-BOOT         ACL-PIN ;
