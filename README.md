# Raspberry Pi OS

This project is a minimal operating system designed for the Raspberry Pi 5. It includes essential components such as a bootloader, kernel, drivers, file systems, and a user shell.

## Project Structure

- **boot/**: Contains the bootloader and configuration files.
  - `bootloader.S`: Assembly code for initializing hardware and loading the kernel.
  - `config.txt`: Configuration settings for the boot process.

- **kernel/**: Contains the main kernel source code and build instructions.
  - `kernel.c`: Core functionalities and system calls.
  - `kernel.ld`: Linker script defining memory layout.
  - `start.S`: Assembly code for setting up the execution environment.
  - `Makefile`: Build instructions for the kernel.

- **drivers/**: Contains device drivers for hardware interaction.
  - `gpio.c`: GPIO driver implementation.
  - `uart.c`: UART driver implementation.
  - `timer.c`: Timer driver implementation.

- **fs/**: Contains file system implementations.
  - `vfs.c`: Virtual File System implementation.
  - `fat32.c`: FAT32 file system implementation.

- **lib/**: Contains standard library implementations.
  - `string.c`: String manipulation functions.
  - `stdio.c`: Standard input/output functions.

- **include/**: Contains header files for declarations and definitions.
  - `kernel.h`: Kernel-related functions and structures.
  - `gpio.h`: GPIO driver function declarations.
  - `uart.h`: UART driver function declarations.
  - `timer.h`: Timer driver function declarations.
  - `vfs.h`: VFS function declarations.
  - `fat32.h`: FAT32 file system function declarations.

- **user/**: Contains user-level applications.
  - `shell.c`: Simple shell implementation for user interaction.

- **scripts/**: Contains scripts for automation.
  - `build.sh`: Shell script to automate the build process.

- **LICENSE**: Licensing information for the project.

- **Makefile**: Build instructions for the entire project.

## Setup Instructions

1. Clone the repository to your local machine.
2. Navigate to the project directory.
3. Run the build script using the command:
   ```bash
   ./scripts/build.sh
   ```
4. Follow the instructions in the script to compile the OS components.

## Usage

After building the project, you can load the OS onto a Raspberry Pi 5. Ensure that you have the necessary hardware and follow the boot instructions provided in `config.txt`.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue for any suggestions or improvements.

## License

This project is licensed under the MIT License. See the LICENSE file for more details.