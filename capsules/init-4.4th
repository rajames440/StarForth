Block 2130
( init-4.4th - pi series )
VARIABLE ACC
VARIABLE SIGN
1000000 CONSTANT SCALE
: RESET-PI 0 ACC ! 1 SIGN ! ;
: FLIP-SIGN
  SIGN @ 0< IF 1 SIGN ! ELSE -1 SIGN ! THEN
;
." 4:blk2130 ok" CR
Block 2131
: PI-STEP ( n -- )
  2 * 1 +
  SCALE SWAP / SIGN @ * ACC @ + ACC !
  FLIP-SIGN
;
: PI-CHUNK
  100 0 DO
    I PI-STEP
  LOOP
;
." 4:blk2131 ok" CR
Block 2132
: DO-PI
  RESET-PI
  5 0 DO
    PI-CHUNK
  LOOP
;
: RUN-PI
  8 0 DO
    DO-PI 46 EMIT
  LOOP
;
." 4:blk2132 ok" CR
RUN-PI
." 4:done" CR
