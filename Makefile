include project.mk
.DEFAULT_GOAL := all

SRC := src

# --- Toolchain selection -----------------------------------------------------
# Self-contained build: a bare-metal AArch64 toolchain is bundled in ./toolchain
# (ARM GNU Toolchain, aarch64-none-elf). The build uses it automatically.
#
# Override on the command line to use a different toolchain, e.g.:
#   make XDIR=/u/cs452/public/xdev          (Waterloo course environment)
#   make XDIR=/Users/darcyliu/bin/xdev
REPO_ROOT  := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
BUNDLED_TC := $(firstword $(wildcard $(REPO_ROOT)toolchain/arm-gnu-toolchain-*-aarch64-none-elf))
XDIR       ?= $(BUNDLED_TC)

ifeq ($(strip $(XDIR)),)
  $(error No toolchain found. Bundled toolchain missing from ./toolchain; \
          set XDIR=/path/to/toolchain (expects $$XDIR/bin/aarch64-none-elf-gcc))
endif
# -----------------------------------------------------------------------------
ARCH  := cortex-a72
MARCH := armv8-a
TRIPLE=aarch64-none-elf
XBINDIR:=$(XDIR)/bin
CC:=$(XBINDIR)/$(TRIPLE)-gcc
OBJCOPY:=$(XBINDIR)/$(TRIPLE)-objcopy
OBJDUMP:=$(XBINDIR)/$(TRIPLE)-objdump

# COMPILE OPTIONS
# -ffunction-sections causes each function to be in a separate section (linker script relies on this)
WARNINGS=-Wall -Wextra -Wpedantic -Wno-unused-const-variable

# Raspberry Pi 4 (BCM2711) only — kernel @ 0x80000, QEMU -M raspi4b or real hardware.
LINK_BASE := 0x80000

CFLAGS:=-g -pipe -static $(WARNINGS) -ffreestanding -nostartfiles \
	-mcpu=$(ARCH) -march=$(MARCH) -mstrict-align -fno-builtin -mgeneral-regs-only \
	-I$(SRC)/layer0-assembly -I$(SRC)/layer1-processes \
	-I$(SRC)/layer1-processes/q_learning -I$(SRC)/layer1-processes/timer \
	-I$(SRC)/layer2-messaging -I$(SRC)/layer3-services -I$(SRC)/layer3-services/uart/io_api -I$(SRC)/layer3-services/uart/io_common -I$(SRC)/layer3-services/uart/io_notifier -I$(SRC)/layer3-services/uart/UART1_CONSOLE_server -I$(SRC)/layer3-services/uart/UART2_MARKLIN_server \
	-I$(SRC)/layer5-applications -I$(SRC)/library -fno-builtin-memcpy
# Platform / transport flag.
# MARKLIN_HW_UART3=0  → AUX mini-UART (QEMU, standard serial1)  [default]
# MARKLIN_HW_UART3=1  → PL011 UART3   (real Pi 4 hat, 2400 baud)
# Override: make MARKLIN_HW_UART3=1
MARKLIN_HW_UART3 ?= 0
CFLAGS += -DMARKLIN_HW_UART3=$(MARKLIN_HW_UART3)
# Build-time config overrides, e.g.  make EXTRA_CFLAGS="-DUI_CANVAS_WMAX=160 -DUI_CANVAS_HMAX=48"
CFLAGS += $(EXTRA_CFLAGS)

# -Wl,option tells g++ to pass 'option' to the linker with commas replaced by spaces
# doing this rather than calling the linker ourselves simplifies the compilation procedure
LDFLAGS:=-Wl,-nmagic -Wl,-T$(SRC)/linker.ld -Wl,--defsym=LINK_BASE=$(LINK_BASE) -nostdlib

# Source files and include dirs

# DarcyOS APU: terminal-only, no heap, no display/UI layer.
EXCLUDE_SRCS := \
	$(wildcard $(SRC)/layer4-ui/*.c) \
	$(SRC)/layer3-services/displayserver.c \
	$(SRC)/layer3-services/display_client.c \
	$(SRC)/layer3-services/disp_tetris.c \
	$(SRC)/layer3-services/display_tetris_client.c \
	$(SRC)/layer3-services/apuserver.c \
	$(SRC)/layer1-processes/shell.c \
	$(SRC)/layer1-processes/malloc/malloc.c \
	$(SRC)/layer1-processes/task_heap.c \
	$(SRC)/layer5-applications/snake.c \
	$(SRC)/layer5-applications/tetris.c \
	$(SRC)/layer5-applications/pong.c \
	$(SRC)/layer5-applications/pong_view.c \
	$(SRC)/layer5-applications/pong_model.c \
	$(SRC)/layer5-applications/pong_controller.c \
	$(SRC)/layer5-applications/game.c \
	$(SRC)/layer5-applications/rps_app.c \
	$(SRC)/layer3-services/tests/display_tests.c \
	$(SRC)/layer3-services/tests/layer4_tests.c \
	$(SRC)/layer1-processes/tests/layer1_tests.c \
	$(wildcard $(SRC)/layer0-assembly/tests/*.c)

SOURCES := $(filter-out $(EXCLUDE_SRCS), \
           $(wildcard $(SRC)/library/*.c) \
           $(wildcard $(SRC)/layer2-messaging/*.c) \
           $(wildcard $(SRC)/layer2-messaging/tests/*.c) \
           $(wildcard $(SRC)/layer3-services/*.c) \
           $(wildcard $(SRC)/layer3-services/uart/*/*.c) \
           $(wildcard $(SRC)/layer3-services/tests/*.c) \
           $(wildcard $(SRC)/layer5-applications/*.c) \
           $(wildcard $(SRC)/layer1-processes/*.c) \
           $(wildcard $(SRC)/layer1-processes/q_learning/*.c) \
           $(wildcard $(SRC)/layer1-processes/timer/*.c) \
           $(wildcard $(SRC)/layer0-assembly/*.S) \
           $(wildcard $(SRC)/layer0-assembly/tests/*.c))

# Create .o and .d files for every .cc and .S (hand-written assembly) file
OBJECTS := $(patsubst %.c, %.o, $(patsubst %.S, %.o, $(SOURCES)))
DEPENDS := $(patsubst %.c, %.d, $(patsubst %.S, %.d, $(SOURCES)))

# The first rule is the default, ie. "make", "make all" and "make $(KERNEL_IMG)" mean the same
all: $(PROJECT_H) $(KERNEL_IMG)

$(PROJECT_H): project.mk
	@printf '%s\n' \
	  '/* Generated from project.mk — edit names there only. */' \
	  '#ifndef _PROJECT_H_' \
	  '#define _PROJECT_H_ 1' \
	  '#define PROJECT_USER "$(PROJECT_ID)"' \
	  '#define PROJECT_HOSTNAME "$(PROJECT_NAME)"' \
	  '#define PROJECT_SHELL_USER PROJECT_USER' \
	  '#define PROJECT_DISPLAY_NAME PROJECT_HOSTNAME' \
	  '#endif' > $@

clean:
	rm -f $(OBJECTS) $(DEPENDS) $(KERNEL_ELF) $(KERNEL_IMG)
	find $(SRC) -name '*.o' -o -name '*.d' | xargs rm -f 2>/dev/null || true

# Output directory for kernel artifacts (os/).
os/:
	@mkdir -p $@

$(KERNEL_IMG): $(KERNEL_ELF)
	$(OBJCOPY) $< -O binary $@

$(KERNEL_ELF): $(OBJECTS) $(SRC)/linker.ld | $(PROJECT_H) os/
	$(CC) $(CFLAGS) $(filter-out %.ld, $^) -o $@ $(LDFLAGS)
	@$(OBJDUMP) -d $(KERNEL_ELF) | fgrep -q q0 && printf "\n***** WARNING: SIMD INSTRUCTIONS DETECTED! *****\n\n" || true

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

%.o: %.S Makefile
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPENDS)
