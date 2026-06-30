Block 4016
( zuse.4th - Bootstrap superuser for StarForth ACL )
( Named for Konrad Zuse, pioneer of programmable computers. )
( Sole superuser; mints credentials; owns emergency REPL. )
( Loaded by ACL.4th; must not load before ACL.4th. )
( FUTURE: Replace with thumbdrive Ed25519 PKI. )
( HUMAN-REVIEW: capsule hash = root of superuser trust. )
0 CONSTANT ZUSE-CERT-LO
0 CONSTANT ZUSE-CERT-HI

Block 4017
( ACL-ZUSE-BOOT ( -- )                               )
( Authenticates zuse session (sets vm->zuse_session=1)
( via C primitive) and pins zuse capsule words.      )
( ZUSE-AUTHENTICATE is C-only; no FORTH word grants  )
( god-mode except through this boot sequence.        )
: ACL-ZUSE-BOOT ( -- )
  ZUSE-AUTHENTICATE
  ['] ZUSE-CERT-LO  ACL-PIN
  ['] ZUSE-CERT-HI  ACL-PIN
  LOG-INFO" zuse: activated"
  ['] ACL-ZUSE-BOOT ACL-PIN ;

Block 4018
( Self-activation )
ACL-ZUSE-BOOT
