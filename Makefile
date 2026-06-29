XDIR:=/u/cs452/public/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump

# COMPILE OPTIONS
# -ffunction-sections causes each function to be in a separate section (linker script relies on this)
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable

# Platform configuration: QEMU_VIRT (default) or RPI4
PLATFORM ?= QEMU_VIRT

# Set TARGET_QEMU_VIRT flag based on PLATFORM
ifeq ($(PLATFORM),QEMU_VIRT)
  PLATFORM_FLAG := -DTARGET_QEMU_VIRT=1
else ifeq ($(PLATFORM),RPI4)
  PLATFORM_FLAG := -DTARGET_QEMU_VIRT=0
else
  $(error Invalid PLATFORM. Use PLATFORM=QEMU_VIRT or PLATFORM=RPI4)
endif

CFLAGS:=-g -pipe -static $(WARNINGS) -ffreestanding -nostartfiles\
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only \
	-Isrc/layer0-assembly -Isrc/layer1-processes -Isrc/layer1-processes/malloc -Isrc/layer1-processes/timer -Isrc/layer2-messaging -Ilibrary -fno-builtin-memcpy \
	$(PLATFORM_FLAG)

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld -nostdlib

# Source files and include dirs

# NOTE: servers/ (e.g. the Q-learning scheduler) is decoupled from the kernel
# and intentionally NOT part of the kernel image build.
SOURCES := $(wildcard library/*.c) $(wildcard src/layer2-messaging/*.c) $(wildcard src/layer2-messaging/tests/*.c) $(wildcard src/layer1-processes/*.c) $(wildcard src/layer1-processes/malloc/*.c) $(wildcard src/layer1-processes/timer/*.c) $(wildcard src/layer1-processes/tests/*.c) $(wildcard src/layer0-assembly/*.S) $(wildcard src/layer0-assembly/tests/*.c) $(wildcard *.c) $(wildcard *.S)
# Create .o and .d files for every .cc and .S (hand-written assembly) file
OBJECTS := $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(SOURCES)))
DEPENDS := $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(SOURCES)))

# The first rule is the default, ie. "make", "make all" and "make 0-d273liu8.img" mean the same
all: 0-d273liu.img

clean:
	rm -f $(OBJECTS) $(DEPENDS) 0-d273liu.elf 0-d273liu.img
	rm -f *.o *.d
	rm -f library/*.o library/*.d
	rm -f src/layer0-assembly/*.o src/layer0-assembly/*.d
	rm -f src/layer0-assembly/tests/*.o src/layer0-assembly/tests/*.d
	rm -f src/layer1-processes/*.o src/layer1-processes/*.d
	rm -f src/layer1-processes/malloc/*.o src/layer1-processes/malloc/*.d
	rm -f src/layer1-processes/timer/*.o src/layer1-processes/timer/*.d
	rm -f src/layer1-processes/tests/*.o src/layer1-processes/tests/*.d
	rm -f src/layer2-messaging/*.o src/layer2-messaging/*.d
	rm -f src/layer2-messaging/tests/*.o src/layer2-messaging/tests/*.d
	rm -f servers/q_learning/*.o servers/q_learning/*.d

0-d273liu.img: 0-d273liu.elf
	$(OBJCOPY) $< -O binary $@

0-d273liu.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
	@$(OBJDUMP) -d 0-d273liu.elf | fgrep -q q0 && printf "\n***** WARNING: SIMD INSTRUCTIONS DETECTED! *****\n\n" || true

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.S Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
