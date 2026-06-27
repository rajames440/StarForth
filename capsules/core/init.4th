Block 1
( hosted-init: Q48.16 stubs + VM-EXEC shim for smoke tests )
65536 CONSTANT Q.1
0 CONSTANT Q.0
65208 CONSTANT Q-DECAY
: Q.* * 65536 / ;
: Q.+ + ;
: Q.- - ;
: Q.> > ;
: Q.PRINT 65536 /MOD SWAP . 46 EMIT . ;
: VM-EXEC 2DROP 2DROP ;
