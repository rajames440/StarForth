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
( BIRTH is omitted: kernel-only, not in hosted VM.    )
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
( Self-activation - runs at capsule load time )
( init.4th only needs: S" ACL.4th" EXEC      )
ACL-BOOT
S" zuse.4th" EXEC
