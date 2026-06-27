#!/bin/sh
# Hermes Tripod integration test — exercises all 6 inter-VM message paths.
# Uses the DEFER VM-EXEC / IS VM-EXEC mechanism from init.4th to inject a
# name-length dispatcher shim before Hermes loads.
#
# Run: bash tools/hermes_tripod_smoke.sh
#
# Pass/fail: greps for '^>>>' lines; checks expected recv order and
# that the arena is clean (0) after reap.

SF=./build/amd64/standard/starforth
CORE_INIT=./capsules/core/init.4th
HERMES=./capsules/hermes/init.4th
BACKUP=${CORE_INIT}.tripod_bak

cleanup() {
    if [ -f "$BACKUP" ]; then
        mv "$BACKUP" "$CORE_INIT"
    fi
}
trap cleanup EXIT INT TERM

cp "$CORE_INIT" "$BACKUP"

# Build combined init: core block 1 (contains DEFER VM-EXEC / IS VM-EXEC-STUB)
# + VM-EXEC dispatcher shim block
# + Hermes capsule blocks
# + 6-path test blocks
#
# NOTE: read from BACKUP — redirect to CORE_INIT would truncate it first.
{
cat "$BACKUP"

cat <<'SHIMBLOCK'
Block 9996
( Tripod VM-EXEC dispatcher shim )
: VM-EXEC-TRIPOD ( paddr plen name-addr name-len -- )
  >R >R 2DROP R> R>
  DUP 4 = IF 2DROP ." [Hera recv]"    CR EXIT THEN
  DUP 6 = IF 2DROP ." [Hermes recv]"  CR EXIT THEN
             2DROP ." [Artemis recv]"  CR ;
' VM-EXEC-TRIPOD IS VM-EXEC
SHIMBLOCK

cat "$HERMES"

cat <<'TESTBLOCK'
Block 9997
( Hermes Tripod integration test — 6-path sequence )
: TRIP-ZERO-6 ( -- )
  MSG-ARENA MSG-SCAN !
  6 0 DO
    0 MSG-SCAN @ MSG-HEAT!
    MSG-SCAN @ MSG-CELLS CELLS + MSG-SCAN !
  LOOP ;
CD-INIT
." >>> Tripod integration test" CR
." >>> [1] Hera->Hermes SPAWN-EVENT" CR
SPAWN-EVENT 0 1 S" boot" MSG-SEND
." >>> [2] Hera->Artemis store request" CR
10 0 2 S" boot-rec" MSG-SEND
." >>> [3] Hermes->Hera delivery confirm" CR
SPAWN-EVENT 1 0 S" ok" MSG-SEND
." >>> [4] Hermes->Artemis log store" CR
11 1 2 S" msglog" MSG-SEND
." >>> [5] Artemis->Hera ACK" CR
12 2 0 S" stored" MSG-SEND
." >>> [6] Artemis->Hermes route result" CR
13 2 1 S" result" MSG-SEND
." >>> HERMES-TICK (cool all 6)" CR
HERMES-TICK
." >>> HERMES-TICK ok" CR
TRIP-ZERO-6 MSG-REAP
." >>> MSG-ARENA clean (expect 0): " MSG-ARENA MSG-TYPE@ . CR
." >>> === Tripod integration complete ===" CR
BYE
TESTBLOCK

} > "$CORE_INIT"

OUTPUT=$($SF --log-none 2>&1)
echo "$OUTPUT" | grep '^>>>\|\[\(Hera\|Hermes\|Artemis\) recv\]'

# Validation checks
PASS=0
FAIL=0

check() {
    label="$1"
    pattern="$2"
    if echo "$OUTPUT" | grep -qF "$pattern"; then
        echo "  PASS: $label"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $label  (expected: $pattern)"
        FAIL=$((FAIL + 1))
    fi
}

echo ""
echo "=== Validation ==="
check "test started"              ">>> Tripod integration test"
check "path1 header"              ">>> [1] Hera->Hermes SPAWN-EVENT"
check "path1 recv"                "[Hermes recv]"
check "path2 header"              ">>> [2] Hera->Artemis store request"
check "path2 recv"                "[Artemis recv]"
check "path3 header"              ">>> [3] Hermes->Hera delivery confirm"
check "path3 recv"                "[Hera recv]"
check "path4 header"              ">>> [4] Hermes->Artemis log store"
check "path4 recv"                "[Artemis recv]"
check "path5 header"              ">>> [5] Artemis->Hera ACK"
check "path5 recv"                "[Hera recv]"
check "path6 header"              ">>> [6] Artemis->Hermes route result"
check "path6 recv"                "[Hermes recv]"
check "HERMES-TICK ok"            ">>> HERMES-TICK ok"
check "arena clean"               ">>> MSG-ARENA clean (expect 0): 0"
check "integration complete"      ">>> === Tripod integration complete ==="

echo ""
echo "Result: $PASS passed, $FAIL failed"
if [ "$FAIL" -eq 0 ]; then
    echo "TRIPOD SMOKE: PASS"
    exit 0
else
    echo "TRIPOD SMOKE: FAIL"
    exit 1
fi
