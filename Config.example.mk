# Project Configuration
# Copy this file to Config.mk and edit as needed.
#
# Only change the options you care about.
# Most settings work with the defaults.
#
# NOTE:
# This configuration file was created based on the Circle project's `configure`
# script. You can still use the original Circle `configure` tool to generate a
# Config.mk automatically if you know what you are doing, but you should place
# that file in our project root and not in the Circle to avoid it being
# overwritten by our building system.

# ==============================================================================
# User configuration
# ==============================================================================

# Use flashy tool for flashing firmware
USEFLASHY = 1

# Serial device connected to the Raspberry Pi
# Examples:
#   Linux: /dev/ttyUSB0 or /dev/ttyACM0
SERIALPORT = /dev/ttyUSB0

# Command used to reboot the board through the serial interface
# Requires rebuilding kernel/ if changed
REBOOTMAGIC = reboot

# Baud rate used by the running program
# Requires rebuilding kernel/ if changed
USERBAUD = 115200

# Baud rate used when flashing firmware
# Requires rebuilding bootloader and redoing the SD Card setup if changed
FLASHBAUD = 921600

# Path to the Circle-patched QEMU binary
#QEMU_BINARY = /path/to/qemu/build/qemu-system-aarch64

# Additional custom configuration options can be added here as needed.
# Example:
# MAKEFLAGS += -j$(shell nproc)
# MY_SETTING = value

# ==============================================================================
# Circle & ZeroM2M configuration
#
# Any changes here require rebuilding circle and kernel
# ==============================================================================

# Raspberry Pi model
#
# Valid values:
# | RASPPI | Models                   | Optimized for |
# | ------ | ------------------------ | ------------- |
# | 1      | A, B, A+, B+, Zero, (CM) | ARM1176JZF-S  |
# | 2      | 2, 3, Zero 2, (CM3)      | Cortex-A7     |
# | 3      | 3, Zero 2, (CM3)         | Cortex-A53    |
# | 4      | 4B, 400, CM4             | Cortex-A72    |
RASPPI = 3

# Architecture
#
# 32 = AArch32
# 64 = AArch64
#
# Note:
#   AArch64 requires Raspberry Pi 3 or newer.
AARCH = 64

# Toolchain prefix
#
# Typical values:
#
# 32-bit builds:
#   PREFIX = arm-none-eabi-
#
# 64-bit builds:
#   PREFIX64 = aarch64-none-elf-
#
# Only set the one matching your architecture.
#PREFIX = arm-none-eabi-
PREFIX64 = aarch64-none-elf-

# C++ standard
#
# Default used by Circle: C++14
# Uncomment to use C++17
STANDARD = -std=c++17

# Multicore support
#
# Enables multi-core.
DEFINE += -DARM_ALLOW_MULTI_CORE

# Real-time mode
#
# Improves IRQ latency.
#DEFINE += -DREALTIME

# Default USB keyboard layout
#
# Supported:
#   DE ES FR IT UK US
#
# Example:
#DEFINE += -DDEFAULT_KEYMAP=\"US\"

# QEMU support
#
# Required when running under QEMU.
#DEFINE += -DNO_SDHOST
#DEFINE += -DNO_SCREEN_DMA_BURST_LENGTH
# Needed only for 32-bit QEMU builds
#DEFINE += -DNO_PHYSICAL_COUNTER

# Additional custom defines
#
# Example:
#DEFINE += -DMY_FEATURE
#DEFINE += -DDEBUG_UART
