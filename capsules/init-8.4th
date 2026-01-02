Block 2160
( init-8.4th - fib )
: FIB-SEQ
  0 1
  100000 0 DO
    OVER + SWAP
  LOOP
  DROP DROP
;
: RUN-FIB
  20 0 DO
    FIB-SEQ 46 EMIT
  LOOP
;
FIB-SEQ
( RUN-FIB )


