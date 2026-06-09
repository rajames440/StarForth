Block 2049
( first init.4th )
: STAR 42 EMIT ;
: STARS 0 DO STAR LOOP ;
: MARGIN 30 SPACES ;
: BAR MARGIN 5 STARS CR ;
: BLIP MARGIN STAR CR ;
: F CR BAR BLIP BAR BLIP BLIP CR ;
( S" Hermes" BIRTH )
( S" Artemis" BIRTH )
S" doe.4th" EXEC
: EXEC-DOE 123456 3 L8-DOE ;

Block 2050
: .DOE-TOP
." +--------------------------------------------------------------+" CR
." |                                                              |" CR
." |    LithosAnanke  --  L8 Jacquard  DoE  Harness               |" CR
." |                                                              |" CR
." |    The experiment does not run automatically.                |" CR
." |                                                              |" CR
." |    At the ok> prompt:                                        |" CR
." |      EXEC-DOE              run defaults  seed=123456 n=3     |" CR ;

Block 2051
: .DOE-BOT
." |      <seed> <reps> L8-DOE  custom seed and rep count         |" CR
." |                                                              |" CR
." |    doe.4th is loaded automatically; EXEC-DOE calls L8-DOE     |" CR
." |    Edit capsules/init.4th  to change the defaults.           |" CR
." |                                                              |" CR
." |    See experiments/bare_metal/README.md  for details.        |" CR
." |                                                              |" CR
." +--------------------------------------------------------------+" CR ;
: .DOE-BANNER .DOE-TOP .DOE-BOT ;
.DOE-BANNER
