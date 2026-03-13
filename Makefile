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

$(CIRCLE_DIR)/Config.mk: Config.mk submodules
	@echo
	@echo ">>>> Configuring Circle core"
	@if [ -e "$@" ]; then \
		if [ -L "$@" ] && [ "$$(readlink -f $@)" = "$(CURDIR)/Config.mk" ]; then \
			echo "Config.mk symlink already exists and is correct: $@"; \
		else \
			echo "Error: $@ exists but is not the correct symlink"; \
			exit 1; \
		fi \
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
		echo "Initializing Circle submodule..."; \
		git submodule update --init $(CIRCLE_DIR); \
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
	cd $(CIRCLE_DIR) && ./makeall clean $(MAKEFLAGS)

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