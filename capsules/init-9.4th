
Block 3001

( TRIANGLE WAVE WORKLOAD - Linear ramp up/down )
( Randomized amplitude and frequency )

: COMPUTE ( n -- )
  0 SWAP 0 DO I 7 * + LOOP DROP ;

: RAMP-STEP ( intensity -- )
  50 * COMPUTE ;

Block 3010

( Rising edge with random perturbation )

42 SEED

: RISING-EDGE ( steps -- )
  0 DO
    I 1+
    90 110 RANDOM 100 */ RAMP-STEP
    10 80 120 RANDOM 100 */ WAIT
  LOOP ;

: FALLING-EDGE ( steps -- )
  DUP 0 DO
    OVER I -
    90 110 RANDOM 100 */ RAMP-STEP
    10 80 120 RANDOM 100 */ WAIT
  LOOP DROP ;

Block 3020

( Triangle wave cycles with random amplitude/frequency )

: TRIANGLE-CYCLE ( peak-steps -- )
  DUP RISING-EDGE
  FALLING-EDGE ;

: TRIANGLE-WAVE ( cycles peak -- )
  SWAP 0 DO
    DUP
    70 130 RANDOM 100 */
    TRIANGLE-CYCLE
  LOOP DROP ;

3 10 TRIANGLE-WAVE

Block 3030

( Asymmetric triangles - fast rise, slow fall )

7777 SEED

: ASYM-TRIANGLE ( -- )
  8 0 DO
    I 1+ 80 120 RANDOM 100 */ RAMP-STEP
    20 WAIT
  LOOP
  8 0 DO
    8 I - 80 120 RANDOM 100 */ RAMP-STEP
    80 120 RANDOM WAIT
  LOOP ;

4 0 DO ASYM-TRIANGLE LOOP

Block 3040

( Sawtooth variant - all rise, sharp drop )

54321 SEED

: SAWTOOTH ( steps -- )
  0 DO
    I 1+ 85 115 RANDOM 100 */ RAMP-STEP
    15 60 140 RANDOM 100 */ WAIT
  LOOP
  50 WAIT ;

3 0 DO 12 SAWTOOTH LOOP
