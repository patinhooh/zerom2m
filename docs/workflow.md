# Example Workflow <!-- omit from toc -->

[Back to README](../README.md#docs)

## Table of Contents <!-- omit from toc -->

- [Setup and Build](#setup-and-build)
  - [Option 1: Deploying the Kernel](#option-1-deploying-the-kernel)
  - [Option 2: Developing with the Bootloader](#option-2-developing-with-the-bootloader)
  - [Option 3: Running with QEMU](#option-3-running-with-qemu)

## Setup and Build

Clone the repository:

```bash
git clone <repository-url>
cd zero-m2m
```

Copy the example configuration and adjust as needed:

```bash
cp Config.example.mk Config.mk
```

Build the project:

```bash
make
# or use all cores for building
make -j$(nproc)
```

Go to the boot directory and edit the kernel configurations:

```bash
cd boot
# Edit the configuration file if needed
nano zerom2m.cfg
```

If using Wi-Fi, adjust the network settings as needed:

```bash
cp wpa_supplicant.example.conf wpa_supplicant.conf
```

At this point, you have three possible workflows depending on whether you want to deploy the kernel directly or work with the bootloader for development.

### Option 1: Deploying the Kernel

Prepare the boot files:

```bash
# from the boot directory
make zerom2m
```

Copy the built files to the SD card (adjust DEST to your SD card mount point):

```bash
make cp DEST=/path/to/sd/card
```

Eject the SD card and insert it into the Raspberry Pi to boot

### Option 2: Developing with the Bootloader

Prepare the boot files:

```bash
# from the boot directory
make bootloader
```

Copy the built files to the SD card (adjust `DEST` to your SD card mount point):

```bash
make cp DEST=/path/to/sd/card
```

Eject the SD card and insert it into the Raspberry Pi to boot into the bootloader.  
Now you can simply send new kernels through the serial terminal without needing to reflash the SD card.

Make changes and Build the kernel:

```bash
cd ../kernel
make
```

Send the new kernel to the bootloader through the UART:

```bash
make flash
# or to also open the a serial monitor after flashing
make flash && make monitor-picocom
```

### Option 3: Running with QEMU

If you want to run the kernel in a virtual machine, see [docs/qemu.md](qemu.md) for the full QEMU setup and usage steps.
