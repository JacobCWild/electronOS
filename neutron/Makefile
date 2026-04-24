# ===========================================================================
# Neutron Kernel — Build System
# ===========================================================================
# Cross-compiles the Neutron kernel for AArch64, producing a flat binary
# suitable for booting via QEMU -kernel or U-Boot.
#
# Usage:
#   make              Build the kernel (neutron.bin)
#   make qemu         Build and run in QEMU
#   make clean        Remove build artifacts
#   make dump         Disassemble the kernel ELF
# ===========================================================================

# ---- Toolchain -----------------------------------------------------------
CROSS   ?= aarch64-linux-gnu-
CC      := $(CROSS)gcc
AS      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

# ---- Directories ---------------------------------------------------------
SRCDIR  := .
ARCHDIR := arch/aarch64
COREDIR := core
LIBDIR  := lib
DRVDIR  := drivers
SECDIR  := security
INCDIR  := include
BUILDDIR:= build

# ---- Compiler flags (hardened) -------------------------------------------
CFLAGS  := -Wall -Wextra -Werror \
           -O2 \
           -ffreestanding -nostdlib -nostartfiles \
           -fno-builtin -fno-common \
           -fstack-protector-strong \
           -fPIE \
           -D_FORTIFY_SOURCE=2 \
           -mcpu=cortex-a76 \
           -I$(INCDIR) \
           -std=gnu11

ASFLAGS := -mcpu=cortex-a76 -D__ASSEMBLY__

LDFLAGS := -nostdlib -static -T neutron.ld

# ---- Source files --------------------------------------------------------
ASM_SRCS := $(ARCHDIR)/boot.S \
            $(ARCHDIR)/vectors.S \
            $(ARCHDIR)/context.S

C_SRCS   := $(COREDIR)/main.c \
            $(COREDIR)/memory.c \
            $(COREDIR)/vmm.c \
            $(COREDIR)/process.c \
            $(COREDIR)/capability.c \
            $(COREDIR)/ipc.c \
            $(COREDIR)/syscall.c \
            $(ARCHDIR)/exceptions.c \
            $(ARCHDIR)/gic.c \
            $(ARCHDIR)/timer.c \
            $(LIBDIR)/string.c \
            $(LIBDIR)/printf.c \
            $(LIBDIR)/stack_guard.c \
            $(DRVDIR)/uart/pl011.c \
            $(SECDIR)/policy.c \
            $(SECDIR)/audit.c

# ---- Object files --------------------------------------------------------
ASM_OBJS := $(patsubst %.S,$(BUILDDIR)/%.o,$(ASM_SRCS))
C_OBJS   := $(patsubst %.c,$(BUILDDIR)/%.o,$(C_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

# ---- Targets -------------------------------------------------------------
KERNEL_ELF := $(BUILDDIR)/neutron.elf
KERNEL_BIN := $(BUILDDIR)/neutron.bin

.PHONY: all clean qemu dump size

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo ""
	@echo "=== Neutron Kernel Build Complete ==="
	@$(SIZE) $(KERNEL_ELF)
	@echo "Binary: $@ ($$(stat -c%s $@ 2>/dev/null || stat -f%z $@ 2>/dev/null) bytes)"
	@echo ""

$(KERNEL_ELF): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Assembly files
$(BUILDDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# C files
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

# ---- QEMU ----------------------------------------------------------------
QEMU     := qemu-system-aarch64
QEMU_FLAGS := -M virt,gic-version=3 \
              -cpu cortex-a76 \
              -m 2G \
              -smp 1 \
              -nographic \
              -kernel $(KERNEL_BIN) \
              -serial mon:stdio \
              -no-reboot \
              -d guest_errors

qemu: $(KERNEL_BIN)
	$(QEMU) $(QEMU_FLAGS)

# QEMU with GDB server (for debugging)
qemu-debug: $(KERNEL_BIN)
	$(QEMU) $(QEMU_FLAGS) -S -s

# ---- Debug helpers -------------------------------------------------------
dump: $(KERNEL_ELF)
	$(OBJDUMP) -d -S $< | less

size: $(KERNEL_ELF)
	$(SIZE) $<
