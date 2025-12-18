# Raspberry Pi 4B Boot Notes (Milestone 0)

## Build Artifact
```bash
make -f Makefile.starkernel ARCH=aarch64 TARGET=kernel
make -f Makefile.starkernel ARCH=aarch64 TARGET=kernel rpi4-image
# Output: build/aarch64/kernel/starkernel-rpi4.img
```

## Serial Wiring (3.3V TTL)
- GND → Pin 6 (GND)
- TX (adapter) → Pin 10 (GPIO 15 / UART RX)
- RX (adapter) → Pin 8 (GPIO 14 / UART TX)
- Do **not** connect VCC.

## Flash Image
```bash
# Identify SD card device (e.g., /dev/sdX). This overwrites the device.
sudo dd if=build/aarch64/kernel/starkernel-rpi4.img of=/dev/sdX bs=4M status=progress
sync
```

## Serial Console
```bash
screen /dev/ttyUSB0 115200
# or: minicom -D /dev/ttyUSB0 -b 115200
```

## Notes
- `tools/make-rpi4-image.sh` pulls firmware files and writes a FAT partition with `starkernel.efi` and `config.txt` (`enable_uart=1`, `uart_2ndstage=1`).
- Pi 4B must boot with UART enabled; ensure the firmware files are up to date if console is silent.
- Early boot output is minimal; expect the StarKernel banner and memory map summary on serial once UEFI hands off.
