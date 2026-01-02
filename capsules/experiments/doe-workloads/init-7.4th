Block 2150
( init-7.4th - matrix mul )
: MM-CHUNK
  16 0 DO
    16 0 DO
      16 0 DO
        I J + DROP 46 EMIT
      LOOP
    LOOP
  LOOP
;
: RUN-MMUL
  10 0 DO
    MM-CHUNK 46 EMIT
  LOOP
;
RUN-MMUL
