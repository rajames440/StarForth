
Block 3001

( SQUARE WAVE WORKLOAD - Sharp burst/idle transitions )
( Randomized amplitude and frequency )

: COMPUTE ( n -- )
  0 SWAP 0 DO I 7 * + LOOP DROP ;

: BURST ( intensity -- )
  100 * COMPUTE ;

: SQUARE-CYCLE ( base-intensity base-wait -- )
  SWAP
  50 150 RANDOM 100 */ BURST
  SWAP
  50 150 RANDOM 100 */ WAIT ;

Block 3010

( Main square wave loop with random amplitude/frequency )

42 SEED

: SQUARE-WAVE ( cycles -- )
  0 DO
    80 120 RANDOM    ( randomize base intensity 80-120% )
    80 120 RANDOM    ( randomize base wait 80-120% )
    SQUARE-CYCLE
  LOOP ;

20 SQUARE-WAVE

Block 3020

( Second burst pattern - random selection via CASE )

12345 SEED

: SQUARE-BURST ( n -- )
  0 DO
    1 3 RANDOM
    CASE
      1 OF 500 BURST 100 WAIT ENDOF
      2 OF 1000 BURST 50 WAIT ENDOF
      3 OF 1500 BURST 25 WAIT ENDOF
    ENDCASE
  LOOP ;

1500 SQUARE-BURST

Block 3030

( Final cooldown with random micro-bursts )

9999 SEED

: MICRO-BURST ( -- )
  10 30 RANDOM BURST
  50 150 RANDOM WAIT ;

100000 0 DO MICRO-BURST LOOP
