# Root Makefile

# Prevent recursive make output noise
MAKEFLAGS += --no-print-directory

CIRCLE_DIR ?= third_party/circle
KERNEL_DIR ?= kernel

.PHONY: all circle kernel clean clean-circle clean-kernel submodules check-submodules

all: kernel

# Circle
circle: $(CIRCLE_DIR)/Config.mk
	@echo
	@echo ">>>> Building Circle core"
	cd $(CIRCLE_DIR) && ./makeall $(MAKEFLAGS)

	@echo
	@echo ">>>> Building Circle addons"
	@echo ">>>> Building SDCard addon"
	cd $(CIRCLE_DIR)/addon/SDCard && make $(MAKEFLAGS)
	@echo ">>>> Building FATFS addon"
	cd $(CIRCLE_DIR)/addon/fatfs && make $(MAKEFLAGS)
	@echo ">>>> Building Wlan addon"
	cd $(CIRCLE_DIR)/addon/wlan && ./makeall --nosample $(MAKEFLAGS)

$(CIRCLE_DIR)/Config.mk: Config.mk submodules
	@echo
	@echo ">>>> Configuring Circle core"
	@if [ -L "$@" ]; then \
		if [ "$$(readlink -f "$@")" = "$(CURDIR)/Config.mk" ]; then \
			echo "Config.mk symlink already exists and is correct: $@"; \
		else \
			echo "Error: $@ exists but is not the correct symlink"; \
			exit 1; \
		fi \
	elif [ -e "$@" ]; then \
		echo "Error: $@ exists and is not a symlink"; \
		exit 1; \
	else \
		echo "Creating symlink for Config.mk: $@ -> $(CURDIR)/Config.mk"; \
		ln -s "$(CURDIR)/Config.mk" "$@"; \
	fi


Config.mk:
	@echo ">>> Please create a Config.mk file based on the Config.example.mk and run make again"
	@exit 1

# Submodules
submodules:
	@echo
	@echo ">>>> Checking submodules"
	@if [ -d "$(CIRCLE_DIR)" ]; then \
		echo "Submodule already initialized: $(CIRCLE_DIR)"; \
	else \
		echo "Initializing Circle submodules..."; \
		git submodule update --init $(CIRCLE_DIR); \
		cd $(CIRCLE_DIR)/addon/wlan/hostap && git submodule update --init; \
	fi

# Kernel
kernel: circle
	@echo
	@echo ">>>> Building Kernel"
	$(MAKE) -C $(KERNEL_DIR) $(MAKEFLAGS)

# Cleaning
clean: clean-circle clean-kernel

clean-circle: submodules
	@echo
	@echo "\n>>>> Cleaning Circle"
	@cd $(CIRCLE_DIR) && ./makeall clean $(MAKEFLAGS)
	@rm "$(CIRCLE_DIR)/Config.mk"

clean-kernel:
	@echo
	@echo "\n>>>> Cleaning Kernel"
	$(MAKE) -C $(KERNEL_DIR) clean $(MAKEFLAGS)

# Tools
flash:
	$(MAKE) -C $(KERNEL_DIR) flash

monitor-putty:
	$(MAKE) -C $(KERNEL_DIR) monitor-putty

monitor-minicom:
	$(MAKE) -C $(KERNEL_DIR) monitor-minicom

monitor-picocom:
	$(MAKE) -C $(KERNEL_DIR) monitor-picocom
