# Building <!-- omit from toc -->

[Back to README](../README.md#docs)

## Table of Contents <!-- omit from toc -->

- [Prerequisites](#prerequisites)
  - [Optional (for development/flashing)](#optional-for-developmentflashing)
- [Root Makefile](#root-makefile)
  - [Root Key Targets](#root-key-targets)
- [ZeroM2M Makefile](#zerom2m-makefile)
  - [ZeroM2M Key Targets](#zerom2m-key-targets)
- [Boot Makefile](#boot-makefile)
  - [Boot Key Targets](#boot-key-targets)

The ZeroM2M project uses a **hierarchical Makefile-based build system** with three main levels:

1. **Root Makefile** ([Makefile](../Makefile)): Top-level orchestrator
2. **ZeroM2M Makefile** ([zerom2m/Makefile](../zerom2m/Makefile)): Builds the ZeroM2M kernel
3. **Boot Makefile** ([boot/Makefile](../boot/Makefile)): Manages bootloader and firmware components

> Although ZeroM2M has its own build structure, it integrates with Circle's build system where necessary.  
> In particular, the project reuses Circle's `Rules.mk` to ensure consistent compilation flags and toolchain configuration.

The project depends on third-party build systems provided via submodules.

- **Circle Build System** (`third_party/circle/makeall`): Builds the bare-metal framework and core libraries used by the project.
- **Addon Build System** (`third_party/circle/addons/*`): Builds the Addon libraries that we use, which provides additional functionality for Circle and is used by the project.

## Prerequisites

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

### Optional (for development/flashing)

| Tool | Purpose |
| :--- | :--- |
| putty or minicom or picocom | Serial terminal for debugging/reboot |
| flashy \* | SD card flashing tool |

> \* flashy is inside Circle, you need to go to `third_party/circle/tools/flashy/` to and run `npm install` to use it.

## Root Makefile

The [Root Makefile](../Makefile) is the **entry point for the entire build system**. It orchestrates building the project by coordinating three main tasks: initializing submodules, building third-party components, and building the zerom2m kernel.

### Root Key Targets

| Target | Purpose |
| :--- | :--- |
| `all` | Default target; builds the entire project |
| `zerom2m` | Builds the ZeroM2M kernel |
| `circle` | Builds the Circle bare-metal framework |
| `check-zerom2m` | Runs a syntax check on ZeroM2M kernel without generating binaries |
| `test-zerom2m` | Runs tests on a running ZeroM2M kernel |
| `flash` \* | Flashes the built kernel to the Raspberry Pi via UART |
| `monitor-<terminal>` \*\* | Opens a serial terminal to the Raspberry Pi for logs and interaction |
| `qemu` | Run kernel image in QEMU, see [QEMU Setup](qemu.md) |
| `submodules` | Initializes the Circle submodules if not already present |
| `clean` | Removes all build artifacts |
| `clean-circle` | Cleans Circle's build output |
| `clean-zerom2m` | Cleans the zerom2m's build output |

> \* You will need to go into `third_party/circle/tools/flashy/` and run `npm install` to use the `flashy` tool for flashing.  
> \*\* Opens a terminal putty, minicom or picocom depending on your choice.

## ZeroM2M Makefile

The [ZeroM2M Makefile](../zerom2m/Makefile) compiles the ZeroM2M kernel from C++ sources, links against third-party libraries, handles dependency generation, and project-specific compilation flags specified on the `Config.mk` file.

### ZeroM2M Key Targets

| Target | Purpose |
| :--- | :--- |
| `all` | Default target; compiles the zerom2m binary |
| `check` | Runs a syntax check on ZeroM2M kernel without generating binaries |
| `test` | Runs tests on a running ZeroM2M kernel |
| `clean` | Removes all build artifacts |
| `flash` \* | Flashes the built zerom2m to the Raspberry Pi via UART |
| `monitor-<terminal>` \*\* | Opens a serial terminal to the Raspberry Pi for logs and interaction |
| `qemu` | Run kernel image in QEMU, see [QEMU Setup](qemu.md) |

> \* You will need to go into `third_party/circle/tools/flashy/` and run `npm install` to use the `flashy` tool for flashing.  
> \*\* Opens a terminal putty, minicom or picocom depending on your choice.

## Boot Makefile

The [Boot Makefile](../boot/Makefile) manages Raspberry Pi firmware and project boot files, downloads official firmware at a specified commit, and prepares everything for copying to an SD card.  
It supports two build modes:

- **ZeroM2M:** builds a the ZeroM2M kernel that boots directly
- **Bootloader:** builds a lightweight bootloader that lets you send kernels at runtime

> You may need to change some configurations on Config.mk to adjust baud rates and serial port depending on your setup.

### Boot Key Targets

| Target | Purpose |
| :--- | :--- |
| `zerom2m` | Builds the ZeroM2M kernel and prepares it for SD card deployment |
| `bootloader` | Builds a bootloader that allows kernel switching without SD card access |
| `cp` | Copies firmware, config, and kernel/bootloader files to SD card |
| `firmware` | Downloads firmware files |
| `clean-zerom2m` | Removes ZeroM2M kernel build artifacts |
| `clean-bootloader` | Removes bootloader build artifacts |
| `clean-firmware` | Removes downloaded firmware files |
