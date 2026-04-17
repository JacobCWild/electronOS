# electronOS — AI Agent Instructions

**electronOS** is a security-focused custom operating system for Raspberry Pi 5, built from the ground up with Apple-level security principles. This document helps AI coding agents understand the project structure, build system, and development workflows.

## Project Overview

- **Target**: ARM64 Raspberry Pi 5 running Linux (kernel 6.6 LTS)
- **Build System**: Buildroot 2024.11.1 with custom external tree
- **Architecture**: Custom kernel, drivers, login manager (SDL2), interactive shell, security stack (AppArmor, nftables, LUKS2, audit)
- **Language**: C and Assembly
- **Documentation**: See [README.md](README.md) for features, security architecture, and user guide

## Quick Build Commands

```bash
# Full OS build (30-90 min first time)
./scripts/build.sh              # Auto-installs deps, downloads Buildroot, builds image
make build                      # Same, step-by-step control

# Build individual components (for development)
./scripts/setup-dev.sh          # Install dev deps, build login manager + shell locally
cd src/login && make            # Build just login manager
cd src/shell && make            # Build just shell

# Flash to SD card
./scripts/flash.sh --list       # List available devices
make flash SD=/dev/sdX          # Flash image to device

# Configuration
make menuconfig                 # Interactive Buildroot config (adds packages, features)
make linux-menuconfig           # Interactive kernel configuration
```

**Key build targets in [Makefile](Makefile)**: `setup`, `config`, `build`, `menuconfig`, `linux-menuconfig`, `flash`, `clean`, `distclean`

## Project Structure

```
electronOS/
├── src/                         # Custom source code
│   ├── boot/                    # Plymouth boot animation theme
│   ├── login/                   # Graphical login manager (SDL2 + PAM)
│   ├── shell/                   # Interactive command shell (readline)
│   └── security/                # Security hardening scripts (LUKS, AppArmor setup)
├── kernel/                      # Custom kernel code (minimal, mostly config)
├── drivers/                     # Low-level device drivers
│   ├── framebuffer.c            # GPU display output (VideoCore VII)
│   ├── gpio.c                   # GPIO interface
│   ├── uart.c                   # Serial console
│   ├── timer.c                  # System timer
│   ├── mailbox.c                # Firmware mailbox
│   └── font*                    # Font rendering
├── fs/                          # Filesystem support (VFS, FAT32)
├── include/                     # Shared C headers
├── buildroot/                   # Buildroot integration
│   ├── configs/                 # Defconfigs for target platforms
│   ├── external/                # Custom package definitions
│   │   └── package/
│   │       ├── electronos-boot/
│   │       ├── electronos-login/
│   │       └── electronos-shell/
│   └── board/electronos/        # Board support files
│       ├── overlay/             # Root filesystem overlays
│       ├── post-build.sh        # Security hardening hooks
│       ├── genimage.cfg         # Partition layout definition
│       └── post-image.sh        # Image generation
├── configs/                     # System configurations
│   ├── kernel/                  # Kernel config fragments
│   │   ├── security.fragment
│   │   └── graphics.fragment
│   └── security/                # Runtime security configs
│       ├── apparmor/            # AppArmor profiles
│       ├── nftables.conf        # Firewall rules
│       ├── audit.rules          # Audit logging
│       └── limits.conf          # Resource limits
├── scripts/                     # Build and deployment scripts
│   ├── build.sh                 # One-command full build (entry point)
│   ├── flash.sh                 # Flash to SD card with verification
│   └── setup-dev.sh             # Setup local dev environment
├── Makefile                     # Top-level build orchestration
└── README.md                    # User and deployment documentation
```

## Component Details

### Login Manager (`src/login/`)
- **Purpose**: macOS-inspired graphical login screen on RPi5 VideoCore VII GPU
- **Technology**: SDL2 (graphics), PAM (authentication), C
- **Key Files**: 
  - `main.c` — Entry point, session launch, signal handling
  - `ui.c/ui.h` — SDL2 UI rendering (dark theme, animations, avatar)
  - `auth.c/auth.h` — PAM integration, credential handling
  - `Makefile` — With hardened CFLAGS: `-fstack-protector-strong`, `-fPIE`, `-pie -Wl,-z,relro,-z,now`
- **Dev Testing**: `cd src/login && make && ./electronos-login --test`

### Shell (`src/shell/`)
- **Purpose**: Interactive REPL with 30 commands, piping, tab completion, history
- **Technology**: Readline (line editing, completion), C
- **Key Files**:
  - `main.c` — REPL loop, piping, redirection, signal handling
  - `commands.c/commands.h` — 30 command implementations (cd, pwd, help, sysinfo, alias, export, etc.)
  - `Makefile` — Hardened build flags like login manager
- **Dev Testing**: `cd src/shell && make && ./electronos-shell`

### Boot Theme (`src/boot/`)
- **Purpose**: Custom Plymouth boot animation with logo and progress bar
- **Technology**: Plymouth scripting language
- **Key Files**:
  - `electronos.script` — Boot animation definition
  - `electronos.plymouth` — Theme metadata

### Security Stack (`configs/security/`, `src/security/`)
- **Components**: AppArmor (MAC), nftables (firewall), LUKS2 (encryption), Linux Audit, hardened sysctl
- **Files**: [AppArmor profiles](configs/security/apparmor/), [nftables rules](configs/security/nftables.conf), [audit rules](configs/security/audit.rules), [sysctl hardening](buildroot/board/electronos/post-build.sh)
- **Note**: These are mostly configuration-driven; changes in `buildroot/board/electronos/post-build.sh` apply at build time

### Drivers (`drivers/`)
- **Framebuffer**: RPi5 GPU display output
- **GPIO**: Raspberry Pi GPIO interface
- **UART**: Serial console communication
- **Timer**: System timing
- **Mailbox**: ARM-to-VC firmware communication
- **Font**: On-screen text rendering

## Build System Conventions

### Buildroot Integration
- **External Tree**: `buildroot/external/` — Custom packages without modifying Buildroot
- **Board Support**: `buildroot/board/electronos/` — Board-specific overlays, post-build hooks, partition layout
- **Defconfig**: `buildroot/configs/electronos_rpi5_defconfig` — Pre-configured Buildroot settings
- **Workflow**: `make setup` → `make config` → `make build` → `output/images/sdcard.img`

### Build Flags Convention
All C components use hardened compilation flags (per component Makefiles):
```makefile
CFLAGS = -Wall -Wextra -Werror -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
LDFLAGS = -pie -Wl,-z,relro,-z,now
```
This enables stack protection, PIE (position-independent executable), and RELRO (read-only relocations).

### Adding New Packages
1. Create `buildroot/external/package/your-package/` with `Config.in` and `your-package.mk`
2. Register in `buildroot/external/Config.in`
3. Enable in `buildroot/configs/electronos_rpi5_defconfig`
4. Rebuild with `make build`

## Common Development Workflows

### Modify Shell Commands
1. Edit `src/shell/commands.c` (command implementations) or `commands.h` (declarations)
2. `cd src/shell && make` to rebuild locally
3. Test: `./electronos-shell`
4. For full OS: `make build` to include in Buildroot image

### Modify Login Manager UI/Logic
1. Edit `src/login/ui.c` (UI rendering), `auth.c` (authentication), or `main.c` (entry point)
2. Install SDL2 dev libs if needed: `./scripts/setup-dev.sh`
3. `cd src/login && make && ./electronos-login --test`
4. For full OS: `make build`

### Change Kernel Configuration
```bash
make linux-menuconfig          # Interactive kernel config
# Your changes are in output/.config
# To persist, create a kernel fragment:
scripts/diffconfig output/.config > configs/kernel/my-feature.fragment
# Rebuild: make build
```

### Modify Security Configuration
- **AppArmor profiles**: Edit `configs/security/apparmor/*`
- **Firewall rules**: Edit `configs/security/nftables.conf`
- **Kernel hardening**: Edit kernel fragments in `configs/kernel/`
- **Audit logging**: Edit `configs/security/audit.rules`
- **sysctl hardening**: Edit `buildroot/board/electronos/post-build.sh` (kernel parameters section)

### Add Custom Systemd Service
1. Create service file: `buildroot/board/electronos/overlay/usr/lib/systemd/system/my-service.service`
2. Enable in `buildroot/board/electronos/post-build.sh` (symlink to `multi-user.target.wants/`)
3. Rebuild: `make build`

## Dependencies and Prerequisites

### Build Host Requirements
- **OS**: Linux (Ubuntu 22.04+ recommended)
- **Space**: ~15 GB free disk space
- **Build tools**: `build-essential gcc g++ make patch gzip bzip2 xz-utils` (auto-installed by `build.sh`)
- **Buildroot Version**: 2024.11.1 (auto-downloaded)

### Ingredient Libraries (installed by `setup-dev.sh`)
- SDL2 dev libs (login manager graphics)
- PAM dev libs (authentication)
- Readline (shell line editing)
- Fonts (text rendering)
- Valgrind, GDB, strace (debugging)

## Running Locally vs Full Build

| Task | Command | Output | Speed |
|------|---------|--------|-------|
| Test login manager UI | `./scripts/setup-dev.sh && cd src/login && ./electronos-login --test` | Windowed SDL2 window on dev machine | Seconds |
| Test shell | `./scripts/setup-dev.sh && cd src/shell && ./electronos-shell` | REPL on dev machine | Seconds |
| Full OS image | `./scripts/build.sh` or `make build` | `output/images/sdcard.img` (bootable on RPi5) | 30-90 min |
| Flash to SD | `make flash SD=/dev/sdX` or `./scripts/flash.sh /dev/sdX` | Bootable SD card | ~5 min |

## File Link References

For quick navigation:
- **Build orchestration**: [Makefile](Makefile)
- **Main build script**: [scripts/build.sh](scripts/build.sh)
- **Full requirements**: [README.md](README.md)
- **Security features**: [README.md#Security](README.md) (Apple-inspired security table)
- **Development guide**: [README.md#Development](README.md)

## Key Conventions for AI Agents

1. **Always preserve hardened CFLAGS** when editing component Makefiles
2. **Security first**: Assume all code runs in a security-critical context
3. **Buildroot is the source of truth**: Local changes must be integrated via `buildroot/external/` or board overlays
4. **Component isolation**: Login manager, shell, and drivers have separate Makefiles and can be tested independently
5. **Partition layout**: Immutable in `buildroot/board/electronos/genimage.cfg` (3-partition: boot, rootfs, data)
6. **PAM integration**: Authentication changes require corresponding PAM service files in overlays
7. **systemd services**: All long-running processes are systemd units (enabled in post-build.sh)
8. **ARM64 target**: All code must support ARM64 architecture; no x86 assumptions

## Debugging and Troubleshooting

- **Component build fails**: Ensure dev deps installed: `./scripts/setup-dev.sh`
- **Missing headers**: Check `include/` and component-local `.h` files
- **Buildroot download timeout**: Edit `Makefile` to use `curl` instead of `wget`, or set `https_proxy`
- **SD card write fails**: Verify device with `./scripts/flash.sh --list` and ensure unmounted
- **Security component not loading**: Check `buildroot/board/electronos/post-build.sh` for enable/disable logic

---

**Last updated**: April 2026  
**Target Buildroot version**: 2024.11.1  
**Target kernel**: Linux 6.6 LTS ARM64
