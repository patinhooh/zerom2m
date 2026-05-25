# Root Makefile

# Prevent recursive make output noise
MAKEFLAGS += --no-print-directory

CIRCLE_DIR ?= third_party/circle
ZEROM2M_DIR ?= zerom2m

.PHONY: all circle zerom2m check-zerom2m clean clean-circle clean-zerom2m submodules flash monitor-putty monitor-minicom monitor-picocom qemu

all: zerom2m

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

$(CIRCLE_DIR)/Config.mk: Config.mk
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
	@echo ">>>> Checking submodules"

	@git submodule status $(CIRCLE_DIR) | grep -q '^[+-]' && \
		(echo "Initializing $(CIRCLE_DIR)"; git submodule update --init $(CIRCLE_DIR)) || \
		echo "$(CIRCLE_DIR) already initialized"

	@echo "Initializing $(CIRCLE_DIR)/addon/wlan/hostap"
	@if [ -f "$(CIRCLE_DIR)/makeall" ]; then \
		cd $(CIRCLE_DIR) && git submodule update --init addon/wlan/hostap; \
	else \
		echo "circle not available, skipping hostap"; \
	fi

# Zerom2m
zerom2m: circle
	@echo
	@echo ">>>> Building Zerom2m"
	$(MAKE) -C $(ZEROM2M_DIR) $(MAKEFLAGS)

check-zerom2m:
	$(MAKE) -C $(ZEROM2M_DIR) check $(MAKEFLAGS)

# Cleaning
clean: clean-circle clean-zerom2m

clean-circle: submodules
	@echo
	@echo "\n>>>> Cleaning Circle"
	@cd $(CIRCLE_DIR) && ./makeall clean $(MAKEFLAGS)
	@rm "$(CIRCLE_DIR)/Config.mk"

clean-zerom2m:
	@echo
	@echo "\n>>>> Cleaning Zerom2m"
	$(MAKE) -C $(ZEROM2M_DIR) clean $(MAKEFLAGS)

# Tools
flash:
	$(MAKE) -C $(ZEROM2M_DIR) flash

monitor-putty:
	$(MAKE) -C $(ZEROM2M_DIR) monitor-putty

monitor-minicom:
	$(MAKE) -C $(ZEROM2M_DIR) monitor-minicom

monitor-picocom:
	$(MAKE) -C $(ZEROM2M_DIR) monitor-picocom

qemu:
	$(MAKE) -C $(ZEROM2M_DIR) qemu
