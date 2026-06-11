Block 2057
( BOOT-BANNER - LithosAnanke REPL greeting )
: .SEP ." ================================================" CR ;
: BOOT-BANNER
  CR .SEP
  ." LithosAnanke bare-metal  (FORTH-79 microkernel)" CR
  .SEP
  ." ok> prompt ready." CR
  CR
  ." To load DoE: EXEC-FILE doe.4th" CR
  ." Then run:   12345 3 EXEC-DOE" CR
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

\ S" doe.4th" EXEC
BOOT-BANNER
