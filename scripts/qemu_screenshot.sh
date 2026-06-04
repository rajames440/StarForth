#!/usr/bin/env bash
# qemu_screenshot.sh — Boot LithosAnanke under QEMU, wait for ok>, capture framebuffer.
#
# Usage (called by Makefile.starkernel qemu-screenshot target):
#   scripts/qemu_screenshot.sh <LOADER_EFI> <OVMF_CODE> <OVMF_VARS_RO> <BUILD_DIR> <LOG_DIR>
#
# Outputs:
#   <BUILD_DIR>/screenshot.ppm  — raw PPM from QEMU screendump
#   <BUILD_DIR>/screenshot.png  — converted PNG (requires imagemagick convert)
#   <LOG_DIR>/qemu-screenshot-YYYYMMDD-HHMMSS.log  — serial log
#
# Requires: qemu-system-x86_64, socat, xorriso, mtools, ovmf, imagemagick

set -e

LOADER_EFI="$1"
OVMF_CODE="$2"
OVMF_VARS_RO="$3"
BUILD_DIR="$4"
LOG_DIR="$5"

if [ -z "$LOADER_EFI" ] || [ -z "$OVMF_CODE" ] || [ -z "$OVMF_VARS_RO" ] \
   || [ -z "$BUILD_DIR" ] || [ -z "$LOG_DIR" ]; then
    echo "Usage: $0 <LOADER_EFI> <OVMF_CODE> <OVMF_VARS_RO> <BUILD_DIR> <LOG_DIR>"
    exit 1
fi

for dep in qemu-system-x86_64 socat xorriso mformat; do
    if ! command -v "$dep" >/dev/null 2>&1; then
        echo "Error: $dep not found. Install: apt-get install ${dep%%_*}"
        exit 1
    fi
done

# --------------------------------------------------------------------------
# Build ISO (same process as the qemu: target)
# --------------------------------------------------------------------------
echo "  Building ISO for screendump..."
mkdir -p "${BUILD_DIR}/iso"
dd if=/dev/zero of="${BUILD_DIR}/iso/efi.img" bs=512 count=8192 2>/dev/null
mformat -i "${BUILD_DIR}/iso/efi.img" ::
mmd     -i "${BUILD_DIR}/iso/efi.img" ::/EFI
mmd     -i "${BUILD_DIR}/iso/efi.img" ::/EFI/BOOT
mcopy   -i "${BUILD_DIR}/iso/efi.img" "${LOADER_EFI}" ::/EFI/BOOT/BOOTX64.EFI

xorriso -as mkisofs -r -J \
    -e efi.img -no-emul-boot \
    -o "${BUILD_DIR}/starkernel.iso" "${BUILD_DIR}/iso" 2>&1 | grep -v "^xorriso" || true

# --------------------------------------------------------------------------
# Setup paths
# --------------------------------------------------------------------------
mkdir -p "${LOG_DIR}" "${BUILD_DIR}"

TS=$(date +%Y%m%d-%H%M%S)
SERIAL_LOG="${LOG_DIR}/qemu-screenshot-${TS}.log"
VARS_COPY="${BUILD_DIR}/OVMF_VARS_screenshot.fd"
MONITOR_SOCK="${BUILD_DIR}/qemu-monitor-${TS}.sock"
PPM_OUT="${BUILD_DIR}/screenshot.ppm"
PNG_OUT="${BUILD_DIR}/screenshot.png"
QEMU_PID_FILE="${BUILD_DIR}/qemu-screenshot.pid"

cp "${OVMF_VARS_RO}" "${VARS_COPY}"

echo "  Serial log : ${SERIAL_LOG}"
echo "  Monitor    : ${MONITOR_SOCK}"

# --------------------------------------------------------------------------
# Launch QEMU in background with:
#   -serial file:<log>               — capture text output
#   -monitor unix:<sock>,server,nowait — HMP monitor on UNIX socket
#   -display none                    — headless; VGA surface still exists
#   (VGA is the default device on q35; OVMF GOP maps to it)
# --------------------------------------------------------------------------
qemu-system-x86_64 \
    -machine q35,accel=tcg \
    -cpu qemu64 \
    -m 1024 \
    -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
    -drive if=pflash,format=raw,file="${VARS_COPY}" \
    -cdrom "${BUILD_DIR}/starkernel.iso" -boot d \
    -serial file:"${SERIAL_LOG}" \
    -monitor unix:"${MONITOR_SOCK}",server,nowait \
    -display none \
    -no-reboot \
    &
QEMU_PID=$!
echo "$QEMU_PID" > "${QEMU_PID_FILE}"
echo "  QEMU PID   : ${QEMU_PID}"

# --------------------------------------------------------------------------
# Wait for ok> prompt in serial log (up to 120 s)
# --------------------------------------------------------------------------
echo "  Waiting for ok> prompt..."
DEADLINE=$(( $(date +%s) + 120 ))
while true; do
    if [ "$(date +%s)" -ge "$DEADLINE" ]; then
        echo "  TIMEOUT waiting for ok> — killing QEMU"
        kill "$QEMU_PID" 2>/dev/null || true
        exit 1
    fi
    if grep -q "ok>" "${SERIAL_LOG}" 2>/dev/null; then
        echo "  ok> detected!"
        break
    fi
    sleep 1
done

# Give the framebuffer a moment to fully paint before capturing
sleep 2

# --------------------------------------------------------------------------
# Issue screendump via HMP monitor socket
# --------------------------------------------------------------------------
echo "screendump ${PPM_OUT}" | socat - "UNIX-CONNECT:${MONITOR_SOCK}" 2>/dev/null || true
sleep 1

# --------------------------------------------------------------------------
# Terminate QEMU
# --------------------------------------------------------------------------
echo "quit" | socat - "UNIX-CONNECT:${MONITOR_SOCK}" 2>/dev/null || true
sleep 1
kill "$QEMU_PID" 2>/dev/null || true
wait "$QEMU_PID" 2>/dev/null || true
rm -f "${QEMU_PID_FILE}" "${MONITOR_SOCK}" "${VARS_COPY}"

# --------------------------------------------------------------------------
# Convert PPM → PNG
# --------------------------------------------------------------------------
if [ -f "${PPM_OUT}" ]; then
    echo "  PPM        : ${PPM_OUT} ($(wc -c < "${PPM_OUT}") bytes)"
    if command -v convert >/dev/null 2>&1; then
        convert "${PPM_OUT}" "${PNG_OUT}"
        echo "  PNG        : ${PNG_OUT}"
    else
        echo "  WARN: imagemagick 'convert' not found — PPM only (install imagemagick)"
    fi
    echo ""
    echo "=== Screendump complete ==="
    echo "    Serial log : ${SERIAL_LOG}"
    echo "    Screenshot : ${PNG_OUT}"
else
    echo "  ERROR: PPM not written — screendump may have failed"
    echo "  Serial log: ${SERIAL_LOG}"
    cat "${SERIAL_LOG}" 2>/dev/null | tail -20
    exit 1
fi
