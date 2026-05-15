#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CONFIG_MK="$SCRIPT_DIR/../Config.mk"
KERNEL_PATH="${1:-$SCRIPT_DIR/../kernel/build/kernel8.img}"

if [ -f "$CONFIG_MK" ]; then
    QEMU_BINARY=$(grep -E '^\s*QEMU_BINARY\s*=' "$CONFIG_MK" | sed 's/.*=\s*//' | tail -1)
    QEMU_BINARY=$(eval echo "$QEMU_BINARY")
fi

if [ -z "$QEMU_BINARY" ]; then
    echo "Error: QEMU binary not found."
    echo "Set QEMU_BINARY in Config.mk or add it to your PATH."
    echo "See docs/qemu.md for build instructions."
    exit 1
fi

echo "Using QEMU: $QEMU_BINARY"

"$QEMU_BINARY" -M raspi3b -kernel "$KERNEL_PATH" \
    -netdev user,id=net0,hostfwd=tcp::8080-:80 \
    -device usb-net,netdev=net0 \
    -drive file="$SCRIPT_DIR/sd.img",if=sd,format=raw \
    -serial stdio \
    -no-reboot \
    -global bcm2835-fb.xres=1024 -global bcm2835-fb.yres=768