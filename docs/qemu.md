# QEMU Setup <!-- omit from toc -->

[Back to README](../README.md)

## Table of Contents <!-- omit from toc -->

- [Prerequisites](#prerequisites)
- [Build QEMU with the Circle Patch](#build-qemu-with-the-circle-patch)
- [Build with QEMU\_SAFE](#build-with-qemu_safe)
- [Create the SD Card Image](#create-the-sd-card-image)
- [Run QEMU](#run-qemu)
- [Access the HTTP Server](#access-the-http-server)

## Prerequisites

You need `fdisk`, `mkfs.vfat`, and `libslirp` (standard on most Linux distros), plus a patched build of QEMU (see below).

## Build QEMU with the Circle Patch

The official QEMU supports USB and TCP/IP networking for Raspberry Pi, but running Circle applications with TCP/IP networking still requires a patch. The patched source is available at:

```txt
https://codeberg.org/larchcone/qemu
```

### Clone and build

```bash
git clone https://codeberg.org/larchcone/qemu.git

cd qemu
mkdir build
cd build

../configure --target-list=arm-softmmu,aarch64-softmmu --enable-slirp
make
```

> `--enable-slirp` requires the `libslirp` package installed on your machine.

After building, `qemu-system-aarch64` will be at `build/qemu-system-aarch64`.

### Configure ZeroM2M to use the patched QEMU

Create a local [Config.mk](../Config.mk) if you do not already have one:

```bash
cp Config.example.mk Config.mk
```

Then set the full path to your patched QEMU binary:

```makefile
QEMU_BINARY = /path/to/qemu/build/qemu-system-aarch64
```

## Build with QEMU_SAFE

Uncomment the flag on your [Config.mk](../Config.mk) before building:

```makefile
DEFINE += -DQEMU_SAFE
```

This disables hardware-specific modules that won't work in QEMU:

| Skipped module | Reason |
| --- | --- |
| WLAN | No BCM4343x in QEMU |
| WPA supplicant | Depends on WLAN |

> Networking still works through USB RNDIS emulation, so the HTTP server remains fully functional.

## Create the SD Card Image

The kernels requires a **partitioned** image (MBR + FAT32).

### Create and partition the image

```bash
# Got to the scripts directory
cd scripts

# Create a blank 256 MB image
dd if=/dev/zero of=sd.img bs=1M count=256

# Create MBR partition table with one FAT32 partition
printf "o\nn\np\n1\n2048\n\nt\nb\nw\n" | fdisk sd.img

# Format the partition (offset = 2048 sectors x 512 bytes)
mkfs.vfat -F 32 --offset 2048 sd.img
```

### Copy firmware files into the image

```bash
# From the scripts directory
# Create a local mount point
mkdir -p mnt

# Mount the partition
# offset = 2048 sectors x 512 bytes = 1048576
sudo mount -o loop,offset=1048576 sd.img mnt/

# Build firmware + kernel
cd ../boot
make

# Copy firmware files into the mounted image
sudo make cp DEST=../scripts/mnt/

# Unmount the image
sudo umount ../scripts/mnt/
```

## Run QEMU

The helper script, using your patched QEMU, is located at [scripts/qemu.sh](../scripts/qemu.sh).

Both kernel and root Makefile proviade a `qemu` will run the script:

```bash
make qemu
```

You can optionally run it manually to provide a custom kernel image:

```bash
./scripts/qemu.sh path/to/kernel8.img
```

## Access the HTTP Server

Once the kernel prints:

```txt
kernel: HTTP server listening at http://<ip>/
```

open your browser at:

```txt
http://localhost:8080
```
