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

S" doe.4th" EXEC
: .SEP ." ================================================" CR ;
: BOOT-BANNER
  CR .SEP
  ." LithosAnanke bare-metal DoE  (FORTH-79 microkernel)" CR
  .SEP
  ." DoE ran automatically: 12345 3 EXEC-DOE" CR
  ." 48 runs total: 16 configs x 3 replicates, seed 12345" CR
  CR
  ." Re-run from the ok> prompt:" CR
  ."   DOE                    ( standard 3-rep run      )" CR
  ."   12345 30 EXEC-DOE      ( full 30-rep study        )" CR
  ."   42    1 EXEC-DOE       ( quick 1-rep smoke check  )" CR
  CR
  ." Docs: experiments/bare_metal/README.md" CR
  .SEP CR ;
BOOT-BANNER
