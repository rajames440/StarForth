Block 4100
: WELCOME ." Welcome to Hermes" CR ;
: CONNECT-HERA BYE ;
\ Hermes role: message and event management
: MSG-SEND  ( msg -- ) DROP ;
: MSG-RECV  ( -- msg ) 0 ;
: EVENT-EMIT ( event -- ) DROP ;
: EVENT-WAIT ( -- event ) 0 ;
: LOAD-DOE ( -- ) S" doe.4th" EXEC ;
WELCOME
