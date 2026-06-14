Block 2070
( zuse.4th — Bootstrap superuser for the StarForth ACL system           )
( Named for Konrad Zuse, pioneer of programmable computers.             )
(                                                                        )
( Zuse is the sole superuser: the only principal that may own the       )
( emergency REPL and mint credentials for other users. This capsule     )
( defines zuse's identity and ACL entry. It is loaded by ACL.4th       )
( at boot; it must not be loaded before ACL.4th.                        )
(                                                                        )
( Security model:                                                        )
(   no ACL.4th && no zuse.4th  → no security (hosted dev mode)         )
(   ACL.4th present, zuse absent → ACL enforced, REPL locked            )
(   ACL.4th + zuse.4th         → full lockdown; zuse owns the REPL     )
(                                                                        )
( ○ FUTURE: Replace software identity with thumbdrive PKI.              )
(   zuse's private key will live on a physical drive; this capsule      )
(   becomes a bootstrapper that validates the drive certificate against  )
(   the CA root embedded in ACL.4th.                                    )
( ⚠ HUMAN-REVIEW: This capsule's hash is the root of superuser trust.  )
(   Verify it matches the build manifest before any production deploy.  )

Block 2071
( Zuse identity — certificate placeholder                               )
( ○ FUTURE: Embed zuse's CA-signed Ed25519 public key here at build    )
(   time. The CA that signed it lives in ACL.4th (Block 2066).         )
0 CONSTANT ZUSE-CERT-LO    ( low half of zuse's Ed25519 pubkey )
0 CONSTANT ZUSE-CERT-HI    ( high half — placeholder           )

Block 2072
( ACL-ZUSE-BOOT ( -- )                                                  )
( Pins zuse's own capsule words so they cannot be denied or mutated.   )
( Called at end of this capsule.                                        )
: ACL-ZUSE-BOOT ( -- )
  ' ZUSE-CERT-LO  ACL-PIN
  ' ZUSE-CERT-HI  ACL-PIN
  ' ACL-ZUSE-BOOT ACL-PIN ;

Block 2073
( Self-activation                                                        )
ACL-ZUSE-BOOT
