
Block 3001

( SINUSOIDAL WAVE WORKLOAD - Smooth oscillation )
( Approximated via lookup table with random modulation )
( Randomized amplitude and frequency )

: COMPUTE ( n -- )
  0 SWAP 0 DO I 7 * + LOOP DROP ;

Block 3010

( Sine lookup - 16 steps per quarter wave via CASE )
( sin values: 0,10,20,31,41,50,59,67,74,81,87,91,95,98,99,100 )

: SIN-STEP ( phase -- intensity )
  15 AND
  CASE
    0 OF 0 ENDOF
    1 OF 10 ENDOF
    2 OF 20 ENDOF
    3 OF 31 ENDOF
    4 OF 41 ENDOF
    5 OF 50 ENDOF
    6 OF 59 ENDOF
    7 OF 67 ENDOF
    8 OF 74 ENDOF
    9 OF 81 ENDOF
    10 OF 87 ENDOF
    11 OF 91 ENDOF
    12 OF 95 ENDOF
    13 OF 98 ENDOF
    14 OF 99 ENDOF
    15 OF 100 ENDOF
  ENDCASE ;

Block 3020

( Full sine wave via quarter-wave symmetry )

: FULL-SIN ( phase -- intensity )
  63 AND
  DUP 16 < IF SIN-STEP EXIT THEN
  DUP 32 < IF 31 SWAP - SIN-STEP EXIT THEN
  DUP 48 < IF 32 - SIN-STEP EXIT THEN
  63 SWAP - SIN-STEP ;

Block 3030

( Sinusoidal wave with random amplitude/frequency modulation )

42 SEED

: SINE-PULSE ( phase amplitude -- )
  SWAP FULL-SIN * 100 / 1+
  COMPUTE ;

: SINE-CYCLE ( steps base-amp base-wait -- )
  ROT 0 DO
    I
    2 PICK 80 120 RANDOM 100 */
    SINE-PULSE
    DUP 80 120 RANDOM 100 */ WAIT
  LOOP 2DROP ;

Block 3040

( Main sinusoidal wave generator )

: SINE-WAVE ( cycles -- )
  0 DO
    64                     ( 64 steps = full wave )
    70 130 RANDOM          ( randomize amplitude 70-130% )
    40 80 RANDOM           ( randomize base wait 40-80ms )
    SINE-CYCLE
    100 200 RANDOM WAIT    ( inter-cycle pause )
  LOOP ;

3 SINE-WAVE

Block 3050

( Phase-shifted dual sine - interference pattern )

98765 SEED

: DUAL-SINE ( -- )
  32 0 DO
    I FULL-SIN 80 120 RANDOM 100 */
    I 16 + FULL-SIN 80 120 RANDOM 100 */
    + 2 / 1+ COMPUTE
    60 100 RANDOM WAIT
  LOOP ;

4 0 DO DUAL-SINE LOOP

Block 3060

( Damped sine - decaying amplitude )

11111 SEED

: DAMPED-SINE ( cycles -- )
  100 SWAP               ( starting amplitude )
  0 DO
    64 0 DO
      I OVER FULL-SIN * 100 / 1+
      85 115 RANDOM 100 */ COMPUTE
      50 80 RANDOM WAIT
    LOOP
    85 * 100 /           ( decay amplitude by 15% each cycle )
    DUP 10 < IF DROP 10 THEN
  LOOP DROP ;

3 DAMPED-SINE
