Block 2057
( BOOT-BANNER - LithosAnanke REPL greeting )
: .SEP ." ================================================" CR ;
: BOOT-BANNER
  CR .SEP
  ." LithosAnanke bare-metal DoE  (FORTH-79 microkernel)" CR
  .SEP
  ." DoE words loaded. ok> prompt ready." CR
  CR
  ."   12345 3 EXEC-DOE   ( standard 3-rep run        )" CR
  ."   DOE                ( same, convenience wrapper  )" CR
  ."   12345 30 EXEC-DOE  ( full 30-rep study          )" CR
  ."   42    1 EXEC-DOE   ( quick 1-rep smoke check    )" CR
  CR
  ." Docs: experiments/bare_metal/README.md" CR
  .SEP CR ;
Block 2049
( first init.4th )
: STAR 42 EMIT ;
: STARS 0 DO STAR LOOP ;
: MARGIN 30 SPACES ;
: BAR MARGIN 5 STARS CR ;
: BLIP MARGIN STAR CR ;
: F CR BAR BLIP BAR BLIP BLIP CR ;
\ S" Hermes" BIRTH
\ S" Artemis" BIRTH

S" ACL.4th" EXEC
\ S" doe.4th" EXEC
BOOT-BANNER
