Block 2057
( BOOT-BANNER - LithosAnanke REPL greeting )
: .SEP ." ================================================" CR ;
: BOOT-BANNER
  CR .SEP
  ." LithosAnanke — FORTH-79 bare-metal kernel" CR
  .SEP
  ." Children: CONNECT-HERMES   CONNECT-ARTEMIS" CR
  .SEP CR ;
Block 2049
( Hera init — Mama VM personality )

S" ACL.4th" EXEC
\ S" doe.4th" EXEC
BOOT-BANNER
