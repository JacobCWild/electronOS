CC = gcc
AS = as
LD = ld
CFLAGS = -Wall -Wextra -Iinclude
LDFLAGS = -T kernel/kernel.ld

# Directories
KERNEL_DIR = kernel
DRIVERS_DIR = drivers
FS_DIR = fs
LIB_DIR = lib
USER_DIR = user

# Source files
KERNEL_SRC = $(KERNEL_DIR)/kernel.c $(KERNEL_DIR)/start.S
DRIVERS_SRC = $(DRIVERS_DIR)/gpio.c $(DRIVERS_DIR)/uart.c $(DRIVERS_DIR)/timer.c
FS_SRC = $(FS_DIR)/vfs.c $(FS_DIR)/fat32.c
LIB_SRC = $(LIB_DIR)/string.c $(LIB_DIR)/stdio.c
USER_SRC = $(USER_DIR)/shell.c

# Object files
KERNEL_OBJ = $(KERNEL_SRC:.c=.o) $(KERNEL_SRC:.S=.o)
DRIVERS_OBJ = $(DRIVERS_SRC:.c=.o)
FS_OBJ = $(FS_SRC:.c=.o)
LIB_OBJ = $(LIB_SRC:.c=.o)
USER_OBJ = $(USER_SRC:.c=.o)

# Final output
OUTPUT = os_image.bin

all: $(OUTPUT)

$(OUTPUT): $(KERNEL_OBJ) $(DRIVERS_OBJ) $(FS_OBJ) $(LIB_OBJ) $(USER_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) -o $@ $<

clean:
	rm -f $(KERNEL_OBJ) $(DRIVERS_OBJ) $(FS_OBJ) $(LIB_OBJ) $(USER_OBJ) $(OUTPUT)

.PHONY: all clean

# Should I use the one below?

#CROSS_COMPILE = arm-none-eabi-
#CC = $(CROSS_COMPILE)gcc
#LD = $(CROSS_COMPILE)ld
#OBJCOPY = $(CROSS_COMPILE)objcopy

#CFLAGS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding
#LDFLAGS = -T linker.ld
#
#SRC = \
#    kernel.c \
#    drivers/gpio.c \
#    drivers/framebuffer.c \
#    drivers/mailbox.c \
#    drivers/font.c \
#    fs/fat32.c \
#    user/shell.c
#
#OBJ = $(SRC:.c=.o)
#
#all: kernel.img
#
#%.o: %.c
#    $(CC) $(CFLAGS) -c $< -o $@
#
#kernel.elf: $(OBJ)
#    $(LD) $(LDFLAGS) -o $@ $^
#
#kernel.img: kernel.elf
#    $(OBJCOPY) kernel.elf -O binary $@
#
#clean:
#    rm -f $(OBJ) kernel.elf kernel.img
#
#.PHONY: all clean