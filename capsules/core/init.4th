\ capsules/core/init.4th - Mama FORTH Init Capsule
\
\ This capsule establishes Mama's PERSONALITY.
\ Executed once at kernel boot, before any baby VMs are born.
\
\ Mama has full access to:
\ - capsule_directory (CapsuleDirHeader in .rodata)
\ - capsule_descriptors[] (all CapsuleDesc entries)
\ - capsule_arena[] (all payload bytes)
\
\ Mama can:
\ - Enumerate all capsules
\ - Birth baby VMs from (p) production capsules
\ - Run (e) experiment capsules on herself
\ - Log parity records for determinism validation

\ ============================================================================
\ Mama Identity
\ ============================================================================

: MAMA-BANNER
  CR
  ." LithosAnanke Mama FORTH" CR
  ." Capsule-based VM birth protocol (M7.1)" CR
  CR
;

\ Mama's VM ID is always 0
0 CONSTANT MAMA-VM-ID

\ ============================================================================
\ Capsule Vocabulary (requires kernel primitives)
\
\ The kernel registers these primitives before executing this init:
\   CAPSULE-COUNT-PRIM  ( -- n )       Number of capsules
\   CAPSULE@-PRIM       ( idx -- desc ) Get descriptor address
\   CAPSULE-HASH@       ( desc -- hash ) Get capsule hash from descriptor
\   CAPSULE-FLAGS@      ( desc -- flags ) Get capsule flags
\   CAPSULE-LEN@        ( desc -- len ) Get payload length
\   CAPSULE-BIRTH-PRIM  ( hash -- vm-id ) Birth baby from (p) capsule
\   CAPSULE-RUN-PRIM    ( hash -- ) Run (e) capsule on Mama
\ ============================================================================

\ Flag constants (must match capsule.h)
HEX
01 CONSTANT FLAG-ACTIVE
10 CONSTANT FLAG-PRODUCTION
20 CONSTANT FLAG-EXPERIMENT
40 CONSTANT FLAG-MAMA-INIT
DECIMAL

\ Check if capsule has given flag
: CAPSULE-FLAG? ( flags mask -- flag )
  AND 0<>
;

\ Check capsule mode
: CAPSULE-IS-PRODUCTION? ( flags -- flag )
  FLAG-PRODUCTION CAPSULE-FLAG?
;

: CAPSULE-IS-EXPERIMENT? ( flags -- flag )
  FLAG-EXPERIMENT CAPSULE-FLAG?
;

: CAPSULE-IS-ACTIVE? ( flags -- flag )
  FLAG-ACTIVE CAPSULE-FLAG?
;

\ ============================================================================
\ Capsule Enumeration (placeholder until primitives available)
\ ============================================================================

\ These words will work once kernel primitives are registered

: .CAPSULE-MODE ( flags -- )
  DUP FLAG-MAMA-INIT AND IF ." [m]" DROP EXIT THEN
  DUP FLAG-PRODUCTION AND IF ." [p]" DROP EXIT THEN
  FLAG-EXPERIMENT AND IF ." [e]" ELSE ." [?]" THEN
;

\ List all capsules (requires CAPSULE-COUNT-PRIM and CAPSULE@-PRIM)
\ : CAPSULES
\   CR ." Capsule Directory:" CR
\   CAPSULE-COUNT-PRIM 0 ?DO
\     I CAPSULE@-PRIM
\     DUP CAPSULE-FLAGS@ .CAPSULE-MODE
\     SPACE
\     DUP CAPSULE-HASH@ U.
\     SPACE
\     CAPSULE-LEN@ . ." bytes"
\     CR
\   LOOP
\ ;

\ ============================================================================
\ Birth and Execution (placeholder until primitives available)
\ ============================================================================

\ Birth a baby VM from production capsule
\ : BIRTH ( hash -- vm-id )
\   CAPSULE-BIRTH-PRIM
\ ;

\ Run experiment capsule on Mama
\ : RUN-EXPERIMENT ( hash -- )
\   CAPSULE-RUN-PRIM
\ ;

\ ============================================================================
\ Mama Initialization Complete
\ ============================================================================

: MAMA-READY
  ." Mama FORTH ready." CR
  ." Dictionary established. Capsule system active." CR
;

\ Execute initialization
MAMA-BANNER
MAMA-READY
