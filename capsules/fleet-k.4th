Block 4400
( fleet-k.4th — per-VM rolling window + Fleet K conservation invariant )
( Phase 6: K-REBALANCE, K-SPAWN-HOOK, K-KILL-HOOK, K-CONSERVED?       )
( Conservation: sum(K-LOCAL all VMs) = Q.1 — one Q.1 injected per tick )
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
( K-LOCAL@: rolling average of K-WINDOW-DEPTH samples for VM idx )
: K-LOCAL@ ( idx -- q48 )
  K-WINDOW-DEPTH * CELLS K-WINDOWS +
  0 K-WINDOW-DEPTH 0 DO
    OVER I CELLS + @ +
  LOOP NIP K-WINDOW-DEPTH / ;
Block 4402
( K-FLEET: sum of K-LOCAL across all VMs — conserved = Q.1             )
( Proof: each CD-TICK injects exactly Q.1 into one VM window; over any  )
( K-WINDOW-DEPTH ticks total injected = K-WINDOW-DEPTH * Q.1; K-LOCAL@ )
( divides by K-WINDOW-DEPTH; so sum(K-LOCAL@) = Q.1 always.            )
: K-FLEET ( -- q48 )
  0 VM-COUNT 0 DO I K-LOCAL@ + LOOP ;
( K-REBALANCE: set K-TARGET = Q.1/ACTIVE-VMS for each tripod slot )
: K-REBALANCE ( -- )
  ACTIVE-VMS @ 0 > IF
    Q.1 ACTIVE-VMS @ /
    VM-COUNT 0 DO DUP I CELLS K-TARGET + ! LOOP DROP
  THEN ;
( K-INIT: zero windows, set ACTIVE-VMS=VM-COUNT, rebalance targets )
: K-INIT ( -- )
  K-WINDOWS VM-COUNT K-WINDOW-DEPTH * CELLS 0 FILL
  K-HEADS   VM-COUNT CELLS 0 FILL
  K-LOCAL   VM-COUNT CELLS 0 FILL
  VM-COUNT ACTIVE-VMS !
  K-REBALANCE ;
K-INIT
Block 4403
( K-SPAWN-HOOK: call when any VM is spawned — adjusts fair-share target )
: K-SPAWN-HOOK ( -- )
  ACTIVE-VMS @ 1+ ACTIVE-VMS !
  K-REBALANCE ;
( K-KILL-HOOK: call when any VM is killed — redistributes target )
: K-KILL-HOOK ( -- )
  ACTIVE-VMS @ 1 > IF ACTIVE-VMS @ 1- ACTIVE-VMS ! THEN
  K-REBALANCE ;
( K-CONSERVED?: |K-FLEET - Q.1| < 5% of Q.1 )
3277 CONSTANT K-EPSILON
: K-CONSERVED? ( -- f )
  K-FLEET Q.1 -
  DUP 0 < IF NEGATE THEN
  K-EPSILON < ;
( K-BUMP: push Q.1 into VM idx rolling window — called once per CD-TICK )
: K-BUMP ( idx -- ) Q.1 SWAP K-PUSH ;
Block 4404
( K-STATUS: human-readable conservation report )
: K-STATUS ( -- )
  ." K-ACTIVE=" ACTIVE-VMS @ . CR
  ." K-TARGET=" Q.1 ACTIVE-VMS @ / Q.PRINT CR
  VM-COUNT 0 DO
    ." VM[" I . ." ] local=" I K-LOCAL@ Q.PRINT CR
  LOOP
  ." K-FLEET= " K-FLEET Q.PRINT CR
  K-CONSERVED? IF ." CONSERVED" CR ELSE ." DRIFTED" CR THEN ;
