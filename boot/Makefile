# Boot Makefile

ROOT_DIR ?= ..
KERNEL_DIR ?= $(ROOT_DIR)/kernel
CIRCLE_DIR ?= $(ROOT_DIR)/third_party/circle

FIRMWARE ?= bc7f439c234e19371115e07b57c366df59cc1bc7
FIRMWARE_URL= https://github.com/raspberrypi/firmware/raw/$(FIRMWARE)/boot

FIRMWARE_FILES = LICENCE.broadcom \
	COPYING.linux \
	bootcode.bin \
	start.elf fixup.dat \
	start4.elf fixup4.dat \
	start_cd.elf fixup_cd.dat \
	start4cd.elf fixup4cd.dat \
	bcm2710-rpi-zero-2-w.dtb

CONFIG_FILES := cmdline.txt config.txt

TARGET := kernel8.img

.DEFAULT_GOAL := zerom2m

cp:
	@if [ -z "$(DEST)" ]; then \
		echo "Usage: make cp DEST=/path/to/SDCARD"; \
		exit 1; \
	fi
	@for file in $(FIRMWARE_FILES) $(CONFIG_FILES) $(TARGET); do \
		if [ ! -f "$$file" ]; then \
			echo "Error: Required file '$$file' not found. Please run 'bootloader' or 'zerom2m'."; \
			exit 1; \
		fi; \
	done
	@echo "Copying firmware files to $(DEST)..."
	cp -v $(FIRMWARE_FILES) $(CONFIG_FILES) $(TARGET) $(DEST)

# Build the zerom2m kernel
zerom2m: firmware
	@echo
	@echo ">>>> Building ZeroM2M"
	$(MAKE) -C $(ROOT_DIR) $(MAKEFLAGS)
	@rm -f $(TARGET)
	@cp $(KERNEL_DIR)/build/$(TARGET) .
	@echo
	@echo "Now you can run 'make cp' target to copy the zerom2m to SD Card"

clean-zerom2m:
	@rm -f $(TARGET)
# clean only the kernel build artifacts
	$(MAKE) -C $(KERNEL_DIR) clean $(MAKEFLAGS)

# Build the bootloader so we can reboot and run other kernels without having to
# touch SD Cards
bootloader: firmware
	@echo
	@echo ">>>> Building Bootloader"
	$(MAKE) -C "$(CIRCLE_DIR)/tools/bootloader" $(TARGET) $(MAKEFLAGS)
	@rm -f $(TARGET)
	@cp $(CIRCLE_DIR)/tools/bootloader/$(TARGET) .
	@echo
	@echo "Now you can run 'make cp' target to copy the bootloader to SD Card"

clean-bootloader:
	rm -f $(TARGET)
	make clean -C "$(CIRCLE_DIR)/tools/bootloader" $(MAKEFLAGS)

firmware:
	@echo
	@echo ">>>> Downloading Firmware Files"
	@for file in $(FIRMWARE_FILES) ; \
	do \
		if [ -f "$$file" ]; then \
			echo "  '$$file' exists"; \
			continue; \
		fi; \
		echo "  Downloading $$file"; \
		wget -q -O $$file $(FIRMWARE_URL)/$$file ; \
	done

clean-firmware:
	rm -f $(FIRMWARE_FILES)
