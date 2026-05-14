# ZeroM2M  <!-- omit from toc -->

## Table of Contents <!-- omit from toc -->

- [Project Overview](#project-overview)
- [Hardware](#hardware)
  - [Required Hardware](#required-hardware)
  - [Recommended Hardware](#recommended-hardware)
- [Configuration](#configuration)
  - [Config.mk](#configmk)
  - [Network Configuration](#network-configuration)
  - [cmdline.txt](#cmdlinetxt)
  - [config.txt](#configtxt)
- [Building](#building)
  - [Prerequisites](#prerequisites)
  - [Root Makefile](#root-makefile)
  - [Kernel Makefile](#kernel-makefile)
  - [Boot Makefile](#boot-makefile)
- [Example Workflow](#example-workflow)
  - [Setup and Build](#setup-and-build)
  - [Option 1: Deploying the Kernel](#option-1-deploying-the-kernel)
  - [Option 2: Developing with the Bootloader](#option-2-developing-with-the-bootloader)
- [Third-Party Components](#third-party-components)
- [License](#license)

## Project Overview

ZeroM2M is a bare-metal oneM2M resource server designed to run directly on the Raspberry Pi Zero 2 W hardware platform without requiring a general-purpose operating system.  
The project implements a functional HTTP-based oneM2M stack with priority given to the development of POST and GET operations.  
Supports the following core oneM2M resources:

- AE (Application Entity)
- CNT (Container)
- CI (Content Instance)
- Subscription resources

## Hardware

ZeroM2M is developed and tested for the **Raspberry Pi Zero 2 W**.

> Default configurations are provided only for this platform.  
> Circle may allow the project to run on other Raspberry Pi models, but this has not been tested and may require adjustments.

### Required Hardware

- Raspberry Pi Zero 2 W
- Micro SD card
- Micro SD card reader/writer
- 5V power supply (micro-USB, capable of at least 2.5A recommended)

### Recommended Hardware

For development and debugging it is strongly recommended to use a **USB-to-TTL serial adapter** connected to the Raspberry Pi UART pins. This allows you to:

- View runtime logs on a serial terminal
- Use the bootloader for easy kernel flashing without touching the SD card

> Logs can also be sent to a display via the Mini HDMI port. See configuration of [cmdline.txt](#cmdlinetxt).

See [Boot Makefile](#boot-makefile) and [Developing with the Bootloader](#option-2-developing-with-the-bootloader) for details.

| Raspberry Pi GPIO | Adapter Pin |
| :--- | :--- |
| Ground | Ground |
| GPIO 14 (TXD) | RXD |
| GPIO 15 (RXD) | TXD |

<img src="uart_connection.png" alt="Raspberry Pi Zero 2 W UART Connection" width="500">

## Configuration

### Config.mk

[Config.example.mk](Config.example.mk) provides a base configuration file containing both Circle and project-specific settings. This includes options such as the compiler toolchain prefix, target architecture, C++ standard, and kernel settings like the reboot magic word, serial port, and baud rates used for flashing and runtime communication.

To configure the project, copy `Config.example.mk` to `Config.mk` and adjust the options as needed.

During the build process, this file is symlinked into the Circle directory as `third_party/circle/Config.mk`. This allows both Circle and the project to use the same configuration file for building and runtime settings remain consistent.

### Network Configuration

Network settings are currently defined directly in the kernel source file [kernel.cpp](kernel/src/kernel.cpp#L21).  
The USE_DHCP macro controls whether the system obtains its network configuration automatically via DHCP or uses the static IP parameters defined in the same file.

An example Wi-Fi configuration file is provided at [boot/wpa_supplicant.example.conf](boot/wpa_supplicant.example.conf). Copy it to `boot/wpa_supplicant.conf` and edit the SSID/PSK or other network parameters as needed for your network.

### cmdline.txt

[boot/cmdline.txt](boot/cmdline.txt) is the Raspberry Pi boot command line file. It can contain multiple boot parameters.  
In this project it is currently used only to set the log output device.  
If you want to see logs on a screen, simply remove this file from the SD Card.

### config.txt

[boot/config.txt](boot/config.txt) is the Raspberry Pi firmware configuration file.  
It controls low-level boot settings such as the CPU mode, memory layout, enabled peripherals, and which kernel image is loaded.

## Building

The ZeroM2M project uses a **hierarchical Makefile-based build system** with three main levels:

1. **Root Makefile** ([Makefile](Makefile)): Top-level orchestrator
2. **Kernel Makefile** ([kernel/Makefile](kernel/Makefile)): Builds the ZeroM2M kernel
3. **Boot Makefile** ([boot/Makefile](boot/Makefile)): Manages bootloader and firmware components

> Although ZeroM2M has its own build structure, it integrates with Circle's build system where necessary.  
> In particular, the project reuses Circle's `Rules.mk` to ensure consistent compilation flags and toolchain configuration.

The project depends on third‑party build systems provided via submodules.

- **Circle Build System** (`third_party/circle/makeall`): Builds the bare-metal framework and core libraries used by the project.

### Prerequisites

The SD card must have a single partition formatted as FAT32.

Before building, ensure you have the required software dependencies installed:

| Tool | Purpose |
| :--- | :--- |
| make | Build system |
| git | Version control and submodules |
| wget | For downloading Raspberry Pi firmware |
| aarch64-none-elf-gcc \* | 64-bit ARM compiler |
| aarch64-none-elf-ld \* | 64-bit ARM linker |
| aarch64-none-elf-objcopy \* | 64-bit ARM binary utilities |

> \* aarch64-none-elf, Download from: <https://developer.arm.com/downloads/-/gnu-a>

#### Optional (for development/flashing)

| Tool | Purpose |
| :--- | :--- |
| putty or minicom or picocom | Serial terminal for debugging/reboot |
| flashy \* | SD card flashing tool |

> \* flashy is inside Circle, you need to go to `third_party/circle/tools/flashy/` to and run `npm install` to use it.

### Root Makefile

The [Root Makefile](Makefile) is the **entry point for the entire build system**. It orchestrates building the project by coordinating three main tasks: initializing submodules, building third-party components, and building the kernel.

#### Root Key Targets

| Target | Purpose |
| :--- | :--- |
| `all` | Default target; builds the entire project |
| `kernel` | Builds the ZeroM2M kernel |
| `circle` | Builds the Circle bare-metal framework |
| `flash` \* | Flashes the built kernel to the Raspberry Pi via UART |
| `monitor-<terminal>` \*\* | Opens a serial terminal to the Raspberry Pi for logs and interaction |
| `submodules` | Initializes the Circle submodules if not already present |
| `clean` | Removes all build artifacts |
| `clean-circle` | Cleans Circle's build output |
| `clean-kernel` | Cleans the kernel's build output |

> \* You will need to go into `third_party/circle/tools/flashy/` and run `npm install` to use the `flashy` tool for flashing.  
> \*\* Opens a terminal putty, minicom or picocom depending on your choice.

### Kernel Makefile

The [Kernel Makefile](kernel/Makefile) compiles the ZeroM2M kernel from C++ sources, links against third-party libraries, handles dependency generation, and project-specific compilation flags.  
Build-time flags injected include:

- **COMMIT_HASH:** Git commit hash for version tracking
- **REBOOTMAGIC:** Reboot magic word from Config.mk
- **USERBAUD:** Serial baud rate from Config.mk

#### Kernel Key Targets

| Target | Purpose |
| :--- | :--- |
| `all` | Default target; compiles the kernel binary |
| `clean` | Removes all build artifacts |
| `flash` \* | Flashes the built kernel to the Raspberry Pi via UART |
| `monitor-<terminal>` \*\* | Opens a serial terminal to the Raspberry Pi for logs and interaction |

> \* You will need to go into `third_party/circle/tools/flashy/` and run `npm install` to use the `flashy` tool for flashing.  
> \*\* Opens a terminal putty, minicom or picocom depending on your choice.

### Boot Makefile

The [Boot Makefile](boot/Makefile) manages Raspberry Pi firmware and project boot files, downloads official firmware at a specified commit, and prepares everything for copying to an SD card.  
It supports two build modes:

- **ZeroM2M kernel:** builds a complete custom kernel that boots directly
- **Bootloader:** builds a lightweight bootloader that lets you select kernels at runtime

> You may need to change some configurations on [Config.mk](#configmk) to adjust baud rates and serial port depending on your setup.

#### Boot Key Targets

| Target | Purpose |
| :--- | :--- |
| `zerom2m` | Builds the ZeroM2M kernel and prepares it for SD card deployment |
| `bootloader` | Builds a bootloader that allows kernel switching without SD card access |
| `cp` | Copies firmware, config, and kernel/bootloader files to SD card |
| `firmware` | Downloads firmware files |
| `clean-zerom2m` | Removes ZeroM2M kernel build artifacts |
| `clean-bootloader` | Removes bootloader build artifacts |
| `clean-firmware` | Removes downloaded firmware files |

## Example Workflow

### Setup and Build

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

Go to the boot directory and copy the example Wi-Fi configuration, adjust the network settings as needed:

```bash
cd boot
cp wpa_supplicant.example.conf wpa_supplicant.conf
```

At this point, you have two possible workflows depending on whether you want to deploy the kernel directly or work with the bootloader for development.

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
make flash && make monitor-putty
```

## Third-Party Components

This project uses the Circle bare-metal framework:

- Circle: <https://github.com/rsta2/circle>  
  License: GNU General Public License v3.0 (GPL-3.0)

## License

ZeroM2M is licensed under the GNU General Public License v3.0 (GPL-3.0).

Copyright (C) 2026 ZeroM2M Authors

See the [LICENSE file](LICENSE) for the full license text.
