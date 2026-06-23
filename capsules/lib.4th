Block 4050
( lib.4th — shared serial output primitives )
( Common words reused by doe-campaign.4th and future capsules. )
( Load this before any capsule that needs N. COMMA CRLF CSV-COL. )
: N. ( n -- )
  DUP 0 < IF 45 EMIT ABS THEN
  0 SWAP <# #S #> TYPE ;
: COMMA    44 EMIT ;
: CRLF     13 EMIT 10 EMIT ;
: CSV-COL  ( n -- ) N. COMMA ;
: CSV-LAST ( n -- ) N. CRLF ;
: Q.SHOW ( q -- ) Q.PRINT CR ;
