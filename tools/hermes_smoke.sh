#!/bin/sh
# Hermes v1 hosted smoke test — prints values for manual inspection
# Run: bash tools/hermes_smoke.sh
SF=./build/amd64/standard/starforth
CORE_INIT=./capsules/core/init.4th
HERMES=./capsules/hermes/init.4th
BACKUP=${CORE_INIT}.smoke_bak

cleanup() {
    if [ -f "$BACKUP" ]; then
        mv "$BACKUP" "$CORE_INIT"
    fi
}
trap cleanup EXIT INT TERM

# Back up real init.4th
cp "$CORE_INIT" "$BACKUP"

# Build combined init: core block 1 + all hermes blocks + smoke test block
# NOTE: use BACKUP (not CORE_INIT) as source — redirect > truncates CORE_INIT first
{
cat "$BACKUP"
cat "$HERMES"
cat <<'SMOKEBLOCK'
Block 9998
( Hermes v1 Smoke Part 1 )
." >>> CD-INIT" CR
CD-INIT
." >>> COMMON-CH (expect non-0): " COMMON-CH @ . CR
." >>> COMMON state (expect 1=OPEN): " COMMON-CH @ CH-STATE@ . CR
." >>> COMMON heat (expect 65536=Q.1): " COMMON-CH @ CH-HEAT@ . CR
." >>> MSG-ALLOC #1: " MSG-ALLOC DUP . CR
VARIABLE SLOT1 SLOT1 !
." >>> SLOT1 type before set (expect 0): " SLOT1 @ MSG-TYPE@ . CR
42 SLOT1 @ MSG-TYPE!
." >>> SLOT1 type after set (expect 42): " SLOT1 @ MSG-TYPE@ . CR
99 SLOT1 @ MSG-FROM!
." >>> SLOT1 from (expect 99): " SLOT1 @ MSG-FROM@ . CR
7 SLOT1 @ MSG-TO!
." >>> SLOT1 to (expect 7): " SLOT1 @ MSG-TO@ . CR
Q.1 SLOT1 @ MSG-HEAT!
." >>> SLOT1 heat before cool (expect 65536): " SLOT1 @ MSG-HEAT@ . CR
SLOT1 @ MSG-COOL-ONE
." >>> SLOT1 heat after cool (expect <65536): " SLOT1 @ MSG-HEAT@ . CR
." >>> MSG-ALLOC #2: " MSG-ALLOC DUP . CR
VARIABLE SLOT2 SLOT2 !
." >>> MSG-REAP (SLOT1 still warm): " MSG-REAP
." SLOT1 type after reap (expect 42): " SLOT1 @ MSG-TYPE@ . CR
0 SLOT1 @ MSG-HEAT!
MSG-REAP
Block 9999
( Hermes v1 Smoke Part 2 )
." >>> SLOT1 type after reap (expect 0): " SLOT1 @ MSG-TYPE@ . CR
." >>> CH-ALLOC #1: " CH-ALLOC DUP . CR
VARIABLE CH1 CH1 !
CH-OPEN CH1 @ CH-STATE!
Q.1 CH1 @ CH-HEAT!
CH-ACTIVE @ CH1 @ CH-NEXT!
CH1 @ CH-ACTIVE !
." >>> CH1 state (expect 1=OPEN): " CH1 @ CH-STATE@ . CR
." >>> CH1 heat before cool (expect 65536): " CH1 @ CH-HEAT@ . CR
CH-COOL-ALL
." >>> CH1 heat after cool (expect <65536): " CH1 @ CH-HEAT@ . CR
." >>> MBR-ALLOC #1: " MBR-ALLOC DUP . CR
VARIABLE MBR1 MBR1 !
55 MBR1 @ 1 CELLS + !
." >>> MBR1 vm-id (expect 55): " MBR1 @ MBR-VM@ . CR
MBR1 @ MBR-FREE-NODE
." >>> MBR-FREE-NODE ok" CR
." >>> EVENT-EMIT 77" CR
77 EVENT-EMIT
." >>> EVENT-WAIT (expect 77): " EVENT-WAIT . CR
EVENT-DRAIN
." >>> MSG-ARENA type after DRAIN (expect 0): " MSG-ARENA MSG-TYPE@ . CR
." >>> HERMES-TICK" CR
HERMES-TICK
." >>> HERMES-TICK ok" CR
." >>> === Hermes smoke test complete ===" CR
BYE
SMOKEBLOCK
} > "$CORE_INIT"

$SF --log-none 2>&1 | grep '^>>>'
