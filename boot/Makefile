# Boot Makefile

ROOT_DIR ?= ..
ZEROM2M_DIR ?= $(ROOT_DIR)/zerom2m
CIRCLE_DIR ?= $(ROOT_DIR)/third_party/circle

FIRMWARE_HASH ?= bc7f439c234e19371115e07b57c366df59cc1bc7
FIRMWARE_URL= https://github.com/raspberrypi/firmware/raw/$(FIRMWARE_HASH)/boot
FIRMWARE_FILES = LICENCE.broadcom \
	COPYING.linux \
	bootcode.bin \
	start.elf fixup.dat \
	start4.elf fixup4.dat \
	start_cd.elf fixup_cd.dat \
	start4cd.elf fixup4cd.dat \
	bcm2710-rpi-zero-2-w.dtb

# Non-free firmware files (e.g. for WiFi)
FIRMWARE_NONFREE_HASH = c9d3ae6584ab79d19a4f94ccf701e888f9f87a53
FIRMWARE_NONFREE_URL = https://github.com/RPi-Distro/firmware-nonfree/raw/$(FIRMWARE_NONFREE_HASH)/debian/config/brcm80211

# List of non-free firmware as filename:remote-path (remote path is appended to
# $(FIRMWARE_NONFREE_URL)). This lets some files come from /cypress and others
# from /brcm.
FIRMWARE_NONFREE_LIST = \
	brcmfmac43430-sdio.bin:/cypress/cyfmac43430-sdio.bin \
	brcmfmac43430-sdio.txt:/brcm/brcmfmac43430-sdio.txt \
	brcmfmac43430-sdio.clm_blob:/cypress/cyfmac43430-sdio.clm_blob \
	brcmfmac43436-sdio.bin:/brcm/brcmfmac43436-sdio.bin \
	brcmfmac43436-sdio.txt:/brcm/brcmfmac43436-sdio.txt \
	brcmfmac43436-sdio.clm_blob:/brcm/brcmfmac43436-sdio.clm_blob \
	brcmfmac43436s-sdio.bin:/brcm/brcmfmac43436s-sdio.bin \
	brcmfmac43436s-sdio.txt:/brcm/brcmfmac43436s-sdio.txt \
	brcmfmac43455-sdio.bin:/cypress/cyfmac43455-sdio-minimal.bin \
	brcmfmac43455-sdio.txt:/brcm/brcmfmac43455-sdio.txt \
	brcmfmac43455-sdio.clm_blob:/cypress/cyfmac43455-sdio.clm_blob \
	brcmfmac43456-sdio.bin:/brcm/brcmfmac43456-sdio.bin \
	brcmfmac43456-sdio.txt:/brcm/brcmfmac43456-sdio.txt \
	brcmfmac43456-sdio.clm_blob:/brcm/brcmfmac43456-sdio.clm_blob

# Extract just the filenames for inclusion in copy/clean targets
FIRMWARE_NONFREE_FILES := $(foreach x,$(FIRMWARE_NONFREE_LIST),$(firstword $(subst :, ,$(x))))

CONFIG_FILES := cmdline.txt config.txt wpa_supplicant.conf system.cfg

TARGET := kernel8.img

.DEFAULT_GOAL := zerom2m

cp:
	@if [ -z "$(DEST)" ]; then \
		echo "Usage: make cp DEST=/path/to/SDCARD"; \
		exit 1; \
	fi
	@if [ ! -f "wpa_supplicant.conf" ]; then \
		echo ">>> Please create a wpa_supplicant.conf file based on the wpa_supplicant.example.conf and run make again"; \
		exit 1; \
	fi
	@for file in $(FIRMWARE_FILES) $(FIRMWARE_NONFREE_FILES) $(CONFIG_FILES) $(TARGET); do \
		if [ ! -f "$$file" ]; then \
			echo "Error: Required file '$$file' not found. Please run 'bootloader' or 'zerom2m'."; \
			exit 1; \
		fi; \
	done
	@echo "Copying firmware files to $(DEST)..."
	@cp -v $(FIRMWARE_FILES) $(CONFIG_FILES) $(TARGET) $(DEST)
	@mkdir -p $(DEST)/firmware
	@cp -v $(FIRMWARE_NONFREE_FILES) $(DEST)/firmware


# Build the zerom2m kernel
zerom2m: firmware
	@echo
	@echo ">>>> Building ZeroM2M"
	$(MAKE) -C $(ROOT_DIR) $(MAKEFLAGS)
	@rm -f $(TARGET)
	@cp $(ZEROM2M_DIR)/build/$(TARGET) .
	@echo
	@echo "Now you can run 'make cp' target to copy the zerom2m to SD Card"

clean-zerom2m:
	@rm -f $(TARGET)
# clean only the zerom2m build artifacts
	$(MAKE) -C $(ZEROM2M_DIR) clean $(MAKEFLAGS)

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

	@# Download non-free firmware files with explicit remote paths
	@if [ -z "$(FIRMWARE_NONFREE_URL)" ]; then \
		echo "  Skipping non-free firmware (no URL configured)"; \
	else \
		for entry in $(FIRMWARE_NONFREE_LIST) ; \
		do \
			file=$$(printf '%s' "$$entry" | cut -d: -f1); \
			path=$$(printf '%s' "$$entry" | cut -d: -f2-); \
			if [ -f "$$file" ]; then \
				echo "  '$$file' exists"; \
				continue; \
			fi; \
			echo "  Downloading $$file from $(FIRMWARE_NONFREE_URL)$$path"; \
			wget -q -O $$file $(FIRMWARE_NONFREE_URL)$$path ; \
		done; \
	fi

clean-firmware:
	rm -f $(FIRMWARE_FILES) $(FIRMWARE_NONFREE_FILES)
