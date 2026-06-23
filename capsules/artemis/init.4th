Block 4110
: WELCOME ." Welcome to Artemis" CR ;
: CONNECT-HERA BYE ;
\ Artemis role: block and memory management with persistence
: BLK-ALLOC   ( -- blk ) 0 ;
: BLK-FREE    ( blk -- ) DROP ;
: BLK-PERSIST ( blk -- ) DROP ;
: BLK-FETCH   ( blk -- ) DROP ;
: LOAD-DOE ( -- ) S" doe.4th" EXEC ;
WELCOME
