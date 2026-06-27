#!/bin/sh
# Hermes channel negotiation smoke test.
# Exercises the full CH-REQUEST → CH-ACCEPT → CH-CLOSE → reap lifecycle.
#
# Protocol under test (from HERMES.md):
#   1. X (Hera=0) publishes request on COMMON via CH-REQUEST
#   2. Y (Hermes=1) accepts via CH-ACCEPT → creates ephemeral channel
#   3. Y sends channel_id back to X via MSG-SEND on COMMON
#   4. Business on ephemeral channel (simulated MSG-SEND)
#   5. Y closes channel via CH-CLOSE
#   6. Channel cools → HERMES-TICK reaps it → CH-ACTIVE = COMMON-CH only
#
# Run: bash tools/hermes_channel_smoke.sh

SF=./build/amd64/standard/starforth
CORE_INIT=./capsules/core/init.4th
HERMES=./capsules/hermes/init.4th
BACKUP=${CORE_INIT}.chan_bak

cleanup() {
    if [ -f "$BACKUP" ]; then
        mv "$BACKUP" "$CORE_INIT"
    fi
}
trap cleanup EXIT INT TERM

cp "$CORE_INIT" "$BACKUP"

# NOTE: read from BACKUP — redirect to CORE_INIT truncates it first.
{
cat "$BACKUP"
cat "$HERMES"
cat <<'TESTBLOCK'
Block 9998
( Channel negotiation smoke — helpers )
VARIABLE MYCHAN
: ASSERT-CH ( ch -- )
  DUP 0= IF ." FAIL: CH-ACCEPT returned 0" CR BYE THEN DROP ;
: TRIP-ZERO-CH ( -- )
  0 MYCHAN @ CH-HEAT! HERMES-TICK ;
Block 9999
( Channel negotiation smoke — part 1 )
CD-INIT
." >>> [1] CH-REQUEST Hera->Hermes negotiate" CR
SPAWN-EVENT 0 1 S" negotiate" CH-REQUEST
." >>> [2] CH-ACCEPT (Hermes creates ephemeral ch)" CR
CH-ACCEPT DUP MYCHAN ! ASSERT-CH
." >>> [3] CH-STATE (expect 1=OPEN): " MYCHAN @ CH-STATE@ . CR
." >>> [4] CH-ID (expect non-0): " MYCHAN @ CH-ID@ . CR
." >>> [5] CH-OWNER (expect 1=Hermes): " MYCHAN @ CH-OWNER@ . CR
." >>> [6] Y sends chanid to X on COMMON" CR
MYCHAN @ CH-ID@ 0 1 S" chanid" MSG-SEND
." >>> [7] COMMON-CH OPEN (expect 1): " COMMON-CH @ CH-STATE@ . CR
." >>> [8] business msg on ephemeral ch" CR
42 1 0 S" work" MSG-SEND
." >>> [9] CH-CLOSE" CR
MYCHAN @ CH-CLOSE
." >>> [10] CH-STATE (expect 2=CLOSING): " MYCHAN @ CH-STATE@ . CR
." >>> [11] HERMES-TICK (hot channel, no reap yet)" CR
HERMES-TICK
." >>> [12] CH-STATE after tick (expect 2=CLOSING): " MYCHAN @ CH-STATE@ . CR
Block 9000
( Channel negotiation smoke — part 2 )
." >>> [13] Zero heat + HERMES-TICK (reap)" CR
TRIP-ZERO-CH
." >>> [14] COMMON-CH OPEN (expect 1): " COMMON-CH @ CH-STATE@ . CR
." >>> [15] CH-ACTIVE is COMMON-CH (expect 1): "
CH-ACTIVE @ COMMON-CH @ = . CR
." >>> === Channel negotiation smoke complete ===" CR
BYE
TESTBLOCK

} > "$CORE_INIT"

OUTPUT=$($SF --log-none 2>&1)
echo "$OUTPUT" | grep '^>>>'

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
check "test started"              ">>> [1] CH-REQUEST"
check "CH-ACCEPT ok"              ">>> [2] CH-ACCEPT"
check "ch state OPEN"             ">>> [3] CH-STATE (expect 1=OPEN): 1"
check "ch id non-zero"            ">>> [4] CH-ID (expect non-0):"
check "ch owner Hermes"           ">>> [5] CH-OWNER (expect 1=Hermes): 1"
check "chanid sent"               ">>> [6] Y sends chanid to X on COMMON"
check "COMMON still OPEN"         ">>> [7] COMMON-CH OPEN (expect 1): 1"
check "business msg"              ">>> [8] business msg on ephemeral ch"
check "CH-CLOSE called"           ">>> [9] CH-CLOSE"
check "state CLOSING"             ">>> [10] CH-STATE (expect 2=CLOSING): 2"
check "tick no reap"              ">>> [11] HERMES-TICK"
check "still CLOSING after tick"  ">>> [12] CH-STATE after tick (expect 2=CLOSING): 2"
check "zero+tick reap"            ">>> [13] Zero heat + HERMES-TICK (reap)"
check "COMMON still OPEN after"   ">>> [14] COMMON-CH OPEN (expect 1): 1"
check "CH-ACTIVE is COMMON-CH"    ">>> [15] CH-ACTIVE is COMMON-CH (expect 1): -1"
check "smoke complete"            ">>> === Channel negotiation smoke complete ==="

echo ""
echo "Result: $PASS passed, $FAIL failed"
if [ "$FAIL" -eq 0 ]; then
    echo "CHANNEL SMOKE: PASS"
    exit 0
else
    echo "CHANNEL SMOKE: FAIL"
    exit 1
fi
