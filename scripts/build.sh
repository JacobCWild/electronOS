#!/bin/bash

# Set the architecture and output directory
ARCH=arm
OUTPUT_DIR=../output

# Create output directory if it doesn't exist
mkdir -p $OUTPUT_DIR

# Build the bootloader
echo "Building bootloader..."
as -o $OUTPUT_DIR/bootloader.o boot/bootloader.S

# Build the kernel
echo "Building kernel..."
gcc -c -o $OUTPUT_DIR/kernel.o kernel/kernel.c -Iinclude
ld -T kernel/kernel.ld -o $OUTPUT_DIR/kernel.elf $OUTPUT_DIR/kernel.o $OUTPUT_DIR/bootloader.o

# Build drivers
echo "Building drivers..."
gcc -c -o $OUTPUT_DIR/gpio.o drivers/gpio.c -Iinclude
gcc -c -o $OUTPUT_DIR/uart.o drivers/uart.c -Iinclude
gcc -c -o $OUTPUT_DIR/timer.o drivers/timer.c -Iinclude

# Build file system
echo "Building file system..."
gcc -c -o $OUTPUT_DIR/vfs.o fs/vfs.c -Iinclude
gcc -c -o $OUTPUT_DIR/fat32.o fs/fat32.c -Iinclude

# Build libraries
echo "Building libraries..."
gcc -c -o $OUTPUT_DIR/string.o lib/string.c -Iinclude
gcc -c -o $OUTPUT_DIR/stdio.o lib/stdio.c -Iinclude

# Build user shell
echo "Building user shell..."
gcc -c -o $OUTPUT_DIR/shell.o user/shell.c -Iinclude

# Link all objects to create the final OS image
echo "Linking all components..."
ld -o $OUTPUT_DIR/os_image $OUTPUT_DIR/kernel.elf $OUTPUT_DIR/gpio.o $OUTPUT_DIR/uart.o $OUTPUT_DIR/timer.o $OUTPUT_DIR/vfs.o $OUTPUT_DIR/fat32.o $OUTPUT_DIR/string.o $OUTPUT_DIR/stdio.o $OUTPUT_DIR/shell.o

echo "Build process completed successfully!"