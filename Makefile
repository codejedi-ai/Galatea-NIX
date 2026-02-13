XDIR:=/u/cs452/public/xdev
# XDIR:=/Users/darcyliu/bin/xdev
ARCH=cortex-a72
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump

# COMPILE OPTIONS
# -ffunction-sections causes each function to be in a separate section (linker script relies on this)
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable

CFLAGS:=-g -pipe -static $(WARNINGS) -ffreestanding -nostartfiles\
	-mcpu=$(ARCH) -static-pie -mstrict-align -fno-builtin -mgeneral-regs-only \
	-Ilayer0-assembly -Ilayer1-processes -Ilayer2-messaging -fno-builtin-memcpy

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-Tlinker.ld -nostdlib

# Source files and include dirs

SOURCES := $(wildcard layer2-messaging/*.c) $(wildcard layer2-messaging/tests/*.c) $(wildcard layer1-processes/*.c) $(wildcard layer1-processes/tests/*.c) $(wildcard layer0-assembly/*.S) $(wildcard layer0-assembly/tests/*.c) $(wildcard *.c) $(wildcard *.S) 
# Create .o and .d files for every .cc and .S (hand-written assembly) file
OBJECTS := $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(SOURCES)))
DEPENDS := $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(SOURCES)))

# The first rule is the default, ie. "make", "make all" and "make 0-d273liu8.img" mean the same
all: 0-d273liu.img

clean:
	rm -f $(OBJECTS) $(DEPENDS) 0-d273liu.elf 0-d273liu.img
	rm -f *.o *.d
	rm -f layer0-assembly/*.o layer0-assembly/*.d
	rm -f layer0-assembly/tests/*.o layer0-assembly/tests/*.d
	rm -f layer1-processes/*.o layer1-processes/*.d
	rm -f layer1-processes/tests/*.o layer1-processes/tests/*.d
	rm -f layer2-messaging/*.o layer2-messaging/*.d
	rm -f layer2-messaging/tests/*.o layer2-messaging/tests/*.d

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
