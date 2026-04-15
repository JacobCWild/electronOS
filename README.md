# electronOS

A security-focused operating system for the Raspberry Pi 5, built from the ground up with Apple-level security principles. Features a custom graphical boot screen, macOS-inspired login manager, and an interactive command shell.

![electronOS Logo](assets/electronOSlogo.png)

## Features

### Boot Experience
- **Custom boot splash** — Displays the electronOS logo with an animated progress bar (Plymouth-based)
- **Fast boot** — Optimized startup sequence targeting sub-15-second boot times
- **LUKS password prompt** — Themed unlock screen if encrypted storage is enabled

### Login Screen
- **macOS-inspired design** — Dark theme with centered user avatar, smooth animations, and error shake effects
- **PAM authentication** — Full Linux PAM integration with account lockout and password policy enforcement
- **Guest fallback** — If no user accounts exist, automatically shows Guest login (password: `guest`)
- **SDL2-based rendering** — Hardware-accelerated graphics via DRM/KMS on the Pi 5's VideoCore VII GPU

### Command Shell
- **30 common commands** — Organized reference table displayed on first login
- **Tab completion** — Command name and filename auto-complete via readline
- **Piping & redirection** — Full support for `|`, `>`, `>>`, `<`, `2>`
- **Command history** — Persistent across sessions (`~/.electronos_history`)
- **Built-in commands** — `cd`, `pwd`, `help`, `clear`, `whoami`, `sysinfo`, `history`, `alias`, `export`

### Security (Apple-Inspired)

| macOS Feature | electronOS Equivalent | Implementation |
|---|---|---|
| Secure Boot | Verified boot chain | U-Boot signed kernel/initramfs |
| System Integrity Protection | Read-only root | ext4 with `ro` mount + overlayfs |
| Gatekeeper | Mandatory Access Control | AppArmor profiles for all components |
| FileVault | Encrypted storage | LUKS2 (AES-XTS-512, Argon2id) on data partition |
| XProtect / Firewall | Network firewall | nftables with default-deny + rate limiting |
| Keychain | Credential storage | Kernel keyring integration |
| Audit | Security event logging | Linux Audit framework with comprehensive rules |

**Additional hardening:**
- Hardened kernel (stack protection, ASLR, restricted ptrace, disabled SysRq)
- Account lockout after 5 failed attempts (10-minute cooldown)
- Core dumps disabled, secure umask (077)
- SUID/SGID binary audit and minimization
- Resource limits to prevent fork bombs and memory exhaustion
- SSH hardened (key-only auth, no root login)

## Architecture

```
electronOS
├── Buildroot Build System          # Generates complete bootable image
│   ├── RPi5 kernel (6.6 LTS)      # With security + graphics fragments
│   ├── U-Boot bootloader           # ARM64 support
│   └── Custom packages             # Boot theme, login, shell
│
├── Boot Flow
│   Plymouth splash ──► Login manager ──► User shell
│   (logo + bar)       (SDL2/PAM)        (readline REPL)
│
├── Security Stack
│   ├── AppArmor (MAC)              # Per-application profiles
│   ├── nftables (firewall)         # Default-deny incoming
│   ├── Linux Audit                 # Event monitoring
│   ├── LUKS2 (encryption)          # Optional data partition
│   └── Hardened sysctl             # Kernel parameter tuning
│
└── Partition Layout (GPT)
    ├── boot   (256MB, FAT32)       # Firmware, kernel, DTBs
    ├── rootfs (2GB, ext4)          # Operating system
    └── data   (remaining, LUKS)    # Encrypted user data
```

## Quick Start

### Prerequisites
- Linux host (Ubuntu 22.04+ recommended)
- ~15 GB free disk space
- Internet connection (first build downloads sources)

### Build

```bash
# Clone the repository
git clone https://github.com/JacobCWild/electronOS.git
cd electronOS

# Option A: One-command build
./scripts/build.sh

# Option B: Step-by-step
make setup      # Download Buildroot
make config     # Apply electronOS configuration
make build      # Build full image (~30-90 min first time)
```

### Flash to SD Card

```bash
# List available devices
./scripts/flash.sh --list

# Flash (replace /dev/sdX with your SD card)
make flash SD=/dev/sdX
# or
./scripts/flash.sh --verify /dev/sdX
```

### First Boot

1. Insert the SD card into your Raspberry Pi 5
2. Connect a display and keyboard
3. Power on — you'll see the electronOS boot splash with progress bar
4. At the login screen, enter:
   - **Username:** `Guest`
   - **Password:** `guest`
5. The command shell will launch with a reference of all 30 commands

## Development

### Build Individual Components

You can build and test the login manager and shell on your development machine without running a full Buildroot build:

```bash
# Install dev dependencies and build components
./scripts/setup-dev.sh

# Test the login manager (windowed mode)
cd src/login && ./electronos-login --test

# Test the shell
cd src/shell && ./electronos-shell
```

### Project Structure

```
electronOS/
├── assets/                         # Logo and visual assets
│   └── electronOSlogo.png
├── buildroot/                      # Buildroot integration
│   ├── configs/                    # Defconfig for RPi5
│   ├── external/                   # Custom package definitions
│   │   └── package/
│   │       ├── electronos-boot/    # Plymouth theme package
│   │       ├── electronos-login/   # Login manager package
│   │       └── electronos-shell/   # Shell package
│   └── board/electronos/           # Board support files
│       ├── overlay/                # Root filesystem overlay
│       ├── post-build.sh           # Security hardening at build time
│       ├── post-image.sh           # SD card image generation
│       └── genimage.cfg            # Partition layout
├── src/                            # Source code
│   ├── boot/                       # Plymouth boot theme
│   │   ├── electronos.plymouth     # Theme metadata
│   │   └── electronos.script       # Boot animation script
│   ├── login/                      # Graphical login manager
│   │   ├── main.c                  # Entry point & session launch
│   │   ├── ui.c                    # SDL2 UI rendering
│   │   ├── ui.h
│   │   ├── auth.c                  # PAM authentication
│   │   └── auth.h
│   ├── shell/                      # Interactive command shell
│   │   ├── main.c                  # REPL, piping, redirection
│   │   ├── commands.c              # 30 commands + built-ins
│   │   └── commands.h
│   └── security/                   # Security tools
│       ├── harden.sh               # Runtime security audit/fix
│       └── encrypt-data.sh         # LUKS encrypted storage setup
├── configs/                        # System configurations
│   ├── kernel/                     # Kernel config fragments
│   │   ├── security.fragment       # Security hardening
│   │   └── graphics.fragment       # GPU / display support
│   └── security/                   # Security configurations
│       ├── apparmor/               # AppArmor profiles
│       ├── nftables.conf           # Firewall rules
│       ├── limits.conf             # Resource limits
│       └── audit.rules             # Audit event rules
├── scripts/                        # Build & utility scripts
│   ├── build.sh                    # One-command full build
│   ├── flash.sh                    # Flash to SD card
│   └── setup-dev.sh               # Development environment setup
├── Makefile                        # Top-level build orchestration
├── LICENSE                         # MIT License
└── README.md                       # This file
```

### Adding New Packages

electronOS uses Buildroot's external tree mechanism, making it easy to add new components:

1. Create a new directory under `buildroot/external/package/your-package/`
2. Add `Config.in` and `your-package.mk` files (see existing packages for examples)
3. Register the package in `buildroot/external/Config.in`
4. Enable it in `buildroot/configs/electronos_rpi5_defconfig`
5. Rebuild with `make build`

### Customizing the Kernel

```bash
# Interactive kernel configuration
make linux-menuconfig

# Your changes are saved in the output directory
# To persist, copy back to a fragment:
# scripts/diffconfig output/.config configs/kernel/my-feature.fragment
```

### Customizing the Boot Theme

Edit `src/boot/electronos.script` to modify the boot animation. The Plymouth scripting language supports:
- Image loading and positioning
- Progress bar callbacks
- Text rendering
- Opacity and scaling animations

## Security Hardening

### Post-Install Security Audit

```bash
# Audit current security posture (read-only)
sudo electronos-harden --check

# Apply all recommended fixes
sudo electronos-harden

# Apply full hardening (may affect usability)
sudo electronos-harden --full
```

### Setting Up Encrypted Storage

```bash
# Encrypt the data partition (first boot only)
sudo electronos-encrypt-data

# Or specify a custom device
sudo electronos-encrypt-data --device /dev/mmcblk0p3
```

### Firewall Management

```bash
# View current rules
sudo nft list ruleset

# Temporarily allow a port (e.g., for a web server)
sudo nft add rule inet electronos_firewall input tcp dport 8080 accept

# Persist changes
sudo cp /etc/nftables.conf /etc/nftables.conf.bak
sudo nft list ruleset > /etc/nftables.conf
```

## Extending electronOS

electronOS is designed to be a foundation you can build upon:

- **Add a desktop environment** — Install Sway/Weston (Wayland) or a lightweight X11 DE via Buildroot
- **Add applications** — Any Buildroot package can be enabled in the defconfig
- **Custom services** — Add systemd unit files to the overlay directory
- **OTA updates** — The partition layout supports A/B update schemes (rootfs duplication)
- **Container support** — Enable Docker/Podman in the kernel config for containerized apps
- **GPIO / Hardware** — Full Raspberry Pi 5 GPIO, I2C, SPI, UART support via the kernel

## Troubleshooting

| Issue | Solution |
|---|---|
| Black screen on boot | Check `config.txt` — ensure `dtoverlay=vc4-kms-v3d` is set |
| Login screen not showing | Check `systemctl status electronos-login` on serial console |
| No network | Verify Ethernet cable; WiFi requires additional firmware |
| Build fails | Run `./scripts/build.sh --deps-only` to install all dependencies |
| Slow boot | Disable `quiet splash` in `cmdline.txt` to see boot messages |

## License

MIT — see [LICENSE](LICENSE) for details.
