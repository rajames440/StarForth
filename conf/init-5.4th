\ init-5.4th — Volatile, high-CV workload

: CHAOS-BLOCK
  256 0 DO
    I 4 MOD 0= IF
      64 0 DO  I J + J * DROP  LOOP
    ELSE
      64 0 DO  I J * I + DROP  LOOP
    THEN
    I 8 MOD 0= IF
      128 0 DO  I J + J * I + DROP  LOOP
    THEN
  LOOP
;

: CHAOS-FIELD
  24 0 DO
    CHAOS-BLOCK
  LOOP
;

: CHAOS-RIPPLE
  8 0 DO
    256 0 DO  I J + DROP  LOOP
    256 0 DO  I J * DROP  LOOP
  LOOP
;

: RUN-CHAOS5
1000000 0 DO
  CHAOS-FIELD
  CHAOS-RIPPLE
LOOP
;

RUN-CHAOS5
