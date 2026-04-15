#!/bin/bash
# ============================================================================
# electronOS Flash Script
# ============================================================================
# Flash the electronOS image to an SD card for Raspberry Pi 5.
#
# Usage:
#   ./scripts/flash.sh /dev/sdX
#   ./scripts/flash.sh --list          # List available devices
#   ./scripts/flash.sh --verify /dev/sdX  # Flash and verify
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${PROJECT_DIR}/output/images/sdcard.img"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

VERIFY=false
DEVICE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --list)
            echo -e "${CYAN}Available removable devices:${NC}"
            lsblk -d -o NAME,SIZE,TYPE,TRAN,MODEL | grep -E "(usb|mmc|sd)" || \
                echo "  No removable devices found."
            exit 0 ;;
        --verify)
            VERIFY=true; shift ;;
        --help)
            echo "Usage: flash.sh [--list] [--verify] /dev/sdX"
            exit 0 ;;
        /dev/*)
            DEVICE="$1"; shift ;;
        *)
            echo "Unknown option: $1"; exit 1 ;;
    esac
done

if [[ -z "$DEVICE" ]]; then
    echo -e "${RED}Error: No device specified.${NC}"
    echo "Usage: flash.sh /dev/sdX"
    echo "Use --list to see available devices."
    exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
    echo -e "${RED}Error: Image not found at ${IMAGE}${NC}"
    echo "Run 'make build' or './scripts/build.sh' first."
    exit 1
fi

if [[ ! -b "$DEVICE" ]]; then
    echo -e "${RED}Error: ${DEVICE} is not a block device.${NC}"
    exit 1
fi

# Safety check: don't flash to the system drive
ROOT_DEV=$(findmnt -no SOURCE / | sed 's/[0-9]*$//')
if [[ "$DEVICE" == "$ROOT_DEV" ]]; then
    echo -e "${RED}Error: ${DEVICE} appears to be the system drive!${NC}"
    exit 1
fi

# Show device info
echo -e "${CYAN}"
echo "  ┌─────────────────────────────────────┐"
echo "  │       electronOS Flash Tool          │"
echo "  └─────────────────────────────────────┘"
echo -e "${NC}"

echo -e "  Image:  ${CYAN}$(basename "$IMAGE")${NC} ($(du -h "$IMAGE" | cut -f1))"
echo -e "  Device: ${YELLOW}${DEVICE}${NC}"
lsblk "$DEVICE" -o NAME,SIZE,MODEL 2>/dev/null | head -2 || true
echo ""

echo -e "${RED}  WARNING: ALL data on ${DEVICE} will be ERASED!${NC}"
read -p "  Type 'yes' to continue: " confirm
if [[ "$confirm" != "yes" ]]; then
    echo "Aborted."
    exit 0
fi

# Unmount any mounted partitions
echo -e "\n${CYAN}Unmounting partitions...${NC}"
for part in "${DEVICE}"*; do
    if mountpoint -q "$part" 2>/dev/null || mount | grep -q "^$part "; then
        sudo umount "$part" 2>/dev/null || true
    fi
done

# Flash
echo -e "${CYAN}Flashing image...${NC}"
sudo dd if="$IMAGE" of="$DEVICE" bs=4M status=progress conv=fsync
sudo sync

echo -e "${GREEN}Flash complete!${NC}"

# Verify
if $VERIFY; then
    echo -e "\n${CYAN}Verifying flash...${NC}"
    IMAGE_SIZE=$(stat -c%s "$IMAGE")
    IMAGE_MD5=$(md5sum "$IMAGE" | cut -d' ' -f1)
    DEVICE_MD5=$(sudo dd if="$DEVICE" bs=4M count=$(( (IMAGE_SIZE + 4194303) / 4194304 )) 2>/dev/null | \
        head -c "$IMAGE_SIZE" | md5sum | cut -d' ' -f1)

    if [[ "$IMAGE_MD5" == "$DEVICE_MD5" ]]; then
        echo -e "  ${GREEN}Verification PASSED (MD5: ${IMAGE_MD5})${NC}"
    else
        echo -e "  ${RED}Verification FAILED!${NC}"
        echo "  Image:  $IMAGE_MD5"
        echo "  Device: $DEVICE_MD5"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  Ready! Insert the SD card into your Pi 5.${NC}"
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo ""
echo "  Default login:"
echo "    Username: Guest"
echo "    Password: guest"
echo ""
