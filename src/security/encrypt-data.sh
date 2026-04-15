#!/bin/bash
# ============================================================================
# electronOS Encrypted Data Partition Setup
# ============================================================================
# Sets up LUKS encryption on the data partition (partition 3).
# This is the equivalent of Apple's FileVault for electronOS.
#
# Usage: sudo electronos-encrypt-data [--device /dev/mmcblk0p3]
# ============================================================================
set -euo pipefail

DEVICE="/dev/mmcblk0p3"
MAPPER_NAME="electronos-data"
MOUNT_POINT="/data"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --device) DEVICE="$2"; shift 2 ;;
        --help)
            echo "Usage: electronos-encrypt-data [--device /dev/mmcblk0p3]"
            echo "Sets up LUKS encryption on the data partition."
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}Error: This script must be run as root${NC}"
    exit 1
fi

if ! command -v cryptsetup >/dev/null 2>&1; then
    echo -e "${RED}Error: cryptsetup is not installed${NC}"
    exit 1
fi

echo -e "${CYAN}"
echo "  ┌─────────────────────────────────────┐"
echo "  │  electronOS Encrypted Storage Setup  │"
echo "  └─────────────────────────────────────┘"
echo -e "${NC}"

echo -e "  Device:     ${YELLOW}${DEVICE}${NC}"
echo -e "  Mapper:     ${MAPPER_NAME}"
echo -e "  Mount:      ${MOUNT_POINT}"
echo ""

# Check if device exists
if [[ ! -b "$DEVICE" ]]; then
    echo -e "${RED}Error: Device $DEVICE does not exist${NC}"
    exit 1
fi

# Check if already encrypted
if cryptsetup isLuks "$DEVICE" 2>/dev/null; then
    echo -e "${YELLOW}Device is already LUKS-encrypted.${NC}"
    echo -e "To unlock: cryptsetup open $DEVICE $MAPPER_NAME"
    exit 0
fi

# Confirm
echo -e "${RED}WARNING: This will ERASE all data on ${DEVICE}!${NC}"
read -p "  Type 'ENCRYPT' to confirm: " confirm
if [[ "$confirm" != "ENCRYPT" ]]; then
    echo "Aborted."
    exit 0
fi

# Set up LUKS with strong parameters
echo -e "\n${CYAN}Setting up LUKS encryption...${NC}"
cryptsetup luksFormat \
    --type luks2 \
    --cipher aes-xts-plain64 \
    --key-size 512 \
    --hash sha512 \
    --pbkdf argon2id \
    --pbkdf-memory 1048576 \
    --pbkdf-parallel 4 \
    --iter-time 3000 \
    --label "electronos-data" \
    "$DEVICE"

echo -e "${GREEN}LUKS container created.${NC}\n"

# Open the encrypted volume
echo -e "${CYAN}Opening encrypted volume...${NC}"
cryptsetup open "$DEVICE" "$MAPPER_NAME"

# Create filesystem
echo -e "${CYAN}Creating ext4 filesystem...${NC}"
mkfs.ext4 -L "electronos-data" "/dev/mapper/${MAPPER_NAME}"

# Mount
mkdir -p "$MOUNT_POINT"
mount "/dev/mapper/${MAPPER_NAME}" "$MOUNT_POINT"

# Create user data directories
mkdir -p "${MOUNT_POINT}/home"
mkdir -p "${MOUNT_POINT}/secure"
chmod 700 "${MOUNT_POINT}/secure"

echo -e "\n${GREEN}Encrypted storage is ready!${NC}"
echo ""
echo "  Mounted at: ${MOUNT_POINT}"
echo "  To add to /etc/crypttab for automatic unlock on boot:"
echo "    echo '${MAPPER_NAME} ${DEVICE} none luks' >> /etc/crypttab"
echo ""
echo "  To mount automatically in /etc/fstab:"
echo "    echo '/dev/mapper/${MAPPER_NAME} ${MOUNT_POINT} ext4 defaults 0 2' >> /etc/fstab"
echo ""
