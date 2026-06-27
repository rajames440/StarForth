Block 4400
( fleet-k.4th: per-VM rolling window + K conservation )
( K-REBALANCE K-SPAWN-HOOK K-KILL-HOOK K-CONSERVED? )
( sum(K-LOCAL all VMs) = Q.1 — K-TARGET[idx] per CD-TICK )
8 CONSTANT K-WINDOW-DEPTH
CREATE K-WINDOWS VM-COUNT K-WINDOW-DEPTH * CELLS ALLOT
CREATE K-HEADS   VM-COUNT CELLS ALLOT
CREATE K-LOCAL   VM-COUNT CELLS ALLOT
CREATE K-TARGET  VM-COUNT CELLS ALLOT
VARIABLE ACTIVE-VMS
Block 4401
( K-SLOT: slot address for VM idx at its current ring head )
: K-SLOT ( idx -- slot-addr )
  DUP K-WINDOW-DEPTH * CELLS K-WINDOWS +
  SWAP CELLS K-HEADS + @ CELLS + ;
( K-PUSH: write q48 sample into VM idx ring and advance head )
: K-PUSH ( q48 idx -- )
  DUP K-SLOT ROT SWAP !
  CELLS K-HEADS +
  DUP @ 1+ K-WINDOW-DEPTH MOD SWAP ! ;
( K-LOCAL@: 8-sample avg; unrolled, avoids nested-DO bug )
: K-LOCAL@ ( idx -- q48 )
  K-WINDOW-DEPTH * CELLS K-WINDOWS +
  DUP @ OVER 1 CELLS + @ + OVER 2 CELLS + @ +
  OVER 3 CELLS + @ + OVER 4 CELLS + @ +
  OVER 5 CELLS + @ + OVER 6 CELLS + @ +
  OVER 7 CELLS + @ + SWAP DROP K-WINDOW-DEPTH / ;
Block 4402
( K-FLEET: sum K-LOCAL@ all VMs; unrolled for VM-COUNT=3 )
: K-FLEET ( -- q48 )
  0 K-LOCAL@ 1 K-LOCAL@ + 2 K-LOCAL@ + ;
( K-REBALANCE: K-TARGET=Q.1/ACTIVE-VMS; unrolled VM-COUNT=3 )
: K-REBALANCE ( -- )
  ACTIVE-VMS @ 0 > IF
    Q.1 ACTIVE-VMS @ /
    DUP 0 CELLS K-TARGET + !
    DUP 1 CELLS K-TARGET + !
        2 CELLS K-TARGET + !
  THEN ;
Block 4403
( K-SPAWN-HOOK: adjust target when VM spawned )
: K-SPAWN-HOOK ( -- )
  ACTIVE-VMS @ 1+ ACTIVE-VMS !
  K-REBALANCE ;
( K-KILL-HOOK: redistribute target when VM killed )
: K-KILL-HOOK ( -- )
  ACTIVE-VMS @ 1 > IF ACTIVE-VMS @ 1- ACTIVE-VMS ! THEN
  K-REBALANCE ;
( K-CONSERVED?: |K-FLEET - Q.1| < 5% of Q.1 )
3277 CONSTANT K-EPSILON
: K-CONSERVED? ( -- f )
  K-FLEET Q.1 -
  DUP 0 < IF NEGATE THEN
  K-EPSILON < ;
( K-BUMP: push K-TARGET[idx] into window — once per CD-TICK )
: K-BUMP ( idx -- ) DUP CELLS K-TARGET + @ SWAP K-PUSH ;
Block 4404
( K-STATUS: conservation report; unrolled for VM-COUNT=3 )
: K-STATUS ( -- )
  ." K-ACTIVE=" ACTIVE-VMS @ . CR
  ." K-TARGET=" Q.1 ACTIVE-VMS @ / Q.PRINT CR
  ." VM[0] local=" 0 K-LOCAL@ Q.PRINT CR
  ." VM[1] local=" 1 K-LOCAL@ Q.PRINT CR
  ." VM[2] local=" 2 K-LOCAL@ Q.PRINT CR
  ." K-FLEET= " K-FLEET Q.PRINT CR
  K-CONSERVED? IF ." CONSERVED" CR ELSE ." DRIFTED" CR THEN ;
Block 4405
( K-INIT: zero all windows and rebalance targets )
: K-INIT ( -- )
  K-WINDOWS VM-COUNT K-WINDOW-DEPTH * CELLS 0 FILL
  K-HEADS   VM-COUNT CELLS 0 FILL
  K-LOCAL   VM-COUNT CELLS 0 FILL
  VM-COUNT ACTIVE-VMS !
  K-REBALANCE ;
( CD-TICK: K heartbeat - advances all VM windows )
: CD-TICK ( -- ) 0 K-BUMP 1 K-BUMP 2 K-BUMP ;
K-INIT
