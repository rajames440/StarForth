Block 2200
( init-0.4th - mat transpose )
: TRANSPOSE-CHUNK
  64 0 DO
    64 0 DO
      I 8 * J +      ( index1 )
      J 8 * I +      ( index2 )
      + DROP
    LOOP
  LOOP
;
Block 2201
: RUN-TRANSPOSE
  256 0 DO
    TRANSPOSE-CHUNK 46 EMIT
  LOOP
;
( RUN-TRANSPOSE )
TRANSPOSE-CHUNK
