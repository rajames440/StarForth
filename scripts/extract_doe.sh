#!/usr/bin/env bash
# extract_doe.sh — extract DOE CSV rows from a QEMU serial log
#
# Usage: extract_doe.sh <serial_log> <output_csv>
#
# Strips ANSI escape codes and the [HADES][DOE ] prefix, writing a clean
# CSV file with a header row followed by data rows.  Works on logs from
# all three supported architectures (amd64, aarch64, riscv64).

set -euo pipefail

LOG="${1:-}"
CSV="${2:-}"

if [ -z "$LOG" ] || [ -z "$CSV" ]; then
    echo "Usage: $0 <serial_log> <output_csv>" >&2
    exit 1
fi

if [ ! -f "$LOG" ]; then
    echo "extract_doe: log not found: $LOG" >&2
    exit 1
fi

mkdir -p "$(dirname "$CSV")"

# 1. Keep only lines containing the DOE tag
# 2. Strip all ANSI escape sequences (ESC [ ... m)
# 3. Remove everything up to and including "[HADES][DOE ] "
grep -aP '\[HADES\]\[DOE \]' "$LOG" \
    | sed 's/\x1b\[[0-9;]*[a-zA-Z]//g' \
    | sed 's/.*\[HADES\]\[DOE \] //' \
    > "$CSV"

ROWS=$(wc -l < "$CSV")
echo "  DOE CSV: $CSV ($((ROWS - 1)) data rows)"
