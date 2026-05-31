# ZeroM2M Makefile
CIRCLE_DIR := ../third_party/circle
# Circle is using this env var
CIRCLEHOME := $(CIRCLE_DIR)


BUILD := build

TARGET := $(BUILD)/kernel8

CPP_SRCS := $(shell find src -name '*.cpp')
C_SRCS   := $(shell find src -name '*.c')
CPP_OBJS := $(CPP_SRCS:src/%.cpp=$(BUILD)/src/%.o)
C_OBJS   := $(C_SRCS:src/%.c=$(BUILD)/src/%.o)
OBJS := $(CPP_OBJS) $(C_OBJS)

# Link to Third-party Libraries
LIBS := \
	$(CIRCLE_DIR)/lib/libcircle.a \
	$(CIRCLE_DIR)/lib/usb/libusb.a \
	$(CIRCLE_DIR)/lib/input/libinput.a \
	$(CIRCLE_DIR)/lib/fs/libfs.a \
	$(CIRCLE_DIR)/lib/net/libnet.a \
	$(CIRCLE_DIR)/lib/sched/libsched.a \
	$(CIRCLE_DIR)/lib/sound/libsound.a \
	$(CIRCLE_DIR)/addon/SDCard/libsdcard.a \
	$(CIRCLE_DIR)/addon/fatfs/libfatfs.a \
	$(CIRCLE_DIR)/addon/wlan/libwlan.a \
	$(CIRCLE_DIR)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a


# CPP Flags
COMMIT_HASH := $(shell git rev-parse --short=10 HEAD)
CPPFLAGS += -DCOMMIT_HASH=\"$(COMMIT_HASH)\"
CPPFLAGS += -DREBOOTMAGIC=\"$(REBOOTMAGIC)\"
CPPFLAGS += -DUSERBAUD=$(USERBAUD)

# SQLite Flags
CFLAGS += -DSQLITE_OS_OTHER=1
CFLAGS += -DSQLITE_THREADSAFE=0
CFLAGS += -DSQLITE_MUTEX_NOOP=1
CFLAGS += -DSQLITE_OMIT_LOAD_EXTENSION
CFLAGS += -DSQLITE_OMIT_WAL
CFLAGS += -DSQLITE_OMIT_AUTOINIT
CFLAGS += -DSQLITE_OMIT_LOCALTIME

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
	@$(CPP) $(CPPFLAGS) -M -MT $(@:.d=.o) -MT $@ -MF $@ $<

$(BUILD)/%.d: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -M -MT $(@:.d=.o) -MT $@ -MF $@ $<

EXTRACLEAN += $(shell find $(BUILD) -type f)

check:
	@echo "  CHECK"
	@$(CPP) $(CPPFLAGS) -fsyntax-only $(CPP_SRCS)

test:
	@if [ -z "$(TESTS)" ]; then \
		echo 'Please specify a test to run with TESTS="test1.py test2.py ..."'; \
		exit 1; \
	fi
	@cd ../third_party/acme/ && uv run runTests.py $(TESTS) -v --no-failfast

test-all:
	@cd ../third_party/acme/ && uv run runTests.py testCSE.py testAE.py testCNT.py testCIN.py testCNT_CIN.py testAddressing.py testSUB.py -v  --no-failfast

# Tools
monitor-picocom:
	@picocom --baud $(USERBAUD) --omap crlf --stopbits 1 $(SERIALPORT)

monitor-minicom:
	@minicom -b $(USERBAUD) -o -D $(SERIALPORT)

# Using circle monitor
monitor-putty: monitor

qemu: $(TARGET).img
	@../scripts/qemu.sh $<
