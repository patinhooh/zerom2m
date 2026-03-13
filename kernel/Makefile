# Kernel Makefile

CIRCLE_DIR := ../third_party/circle
# Circle is using this env var
CIRCLEHOME := $(CIRCLE_DIR)

BUILD := build

TARGET := $(BUILD)/kernel8

CPP_SRCS := $(wildcard src/*.cpp)
OBJS := $(CPP_SRCS:src/%.cpp=$(BUILD)/src/%.o)

# Link to Third-party Libraries
LIBS := \
	$(CIRCLE_DIR)/lib/libcircle.a \
	$(CIRCLE_DIR)/lib/usb/libusb.a \
	$(CIRCLE_DIR)/lib/input/libinput.a \
	$(CIRCLE_DIR)/lib/fs/libfs.a \
	$(CIRCLE_DIR)/lib/net/libnet.a \
	$(CIRCLE_DIR)/lib/sched/libsched.a \
	$(CIRCLE_DIR)/lib/sound/libsound.a

# CPP Flags
COMMIT_HASH := $(shell git rev-parse --short=10 HEAD)
CPPFLAGS += -DCOMMIT_HASH=\"$(COMMIT_HASH)\"
CPPFLAGS += -DREBOOTMAGIC=\"$(REBOOTMAGIC)\"
CPPFLAGS += -DUSERBAUD=$(USERBAUD)

# So we don't have warning from third party code
CPPFLAGS += -isystem $(CIRCLE_DIR)/include

CPPFLAGS += -Iinclude
CPPFLAGS += -Wall -Wextra -Werror
CPPFLAGS += -Wno-error=unused-parameter \
	-Wno-error=unused-function \
	-Wno-error=unused-variable

include $(CIRCLE_DIR)/Rules.mk

# Compile rules
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

-include $(OBJS:.o=.d)

# Dependency generation
$(BUILD)/%.d: %.cpp
	@mkdir -p $(dir $@)
	@$(CPP) $(CPPFLAGS) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

$(BUILD)/%.d: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

EXTRACLEAN += $(shell find $(BUILD) -type f)

# Tools
monitor-picocom:
	@picocom --baud $(USERBAUD) --omap crlf --stopbits 1 $(SERIALPORT)

monitor-minicom:
	@minicom -b $(USERBAUD) -o -D $(SERIALPORT)

# Using circle monitor
monitor-putty: monitor