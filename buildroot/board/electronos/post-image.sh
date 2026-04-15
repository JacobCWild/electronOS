#!/bin/bash
# ============================================================================
# electronOS post-image script
# Creates the final bootable SD card image using genimage.
# ============================================================================
set -euo pipefail

BOARD_DIR="$(dirname "$0")"
GENIMAGE_CFG=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --genimage)
            GENIMAGE_CFG="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

if [ -z "${GENIMAGE_CFG}" ]; then
    echo "Error: --genimage <config> is required"
    exit 1
fi

# Create the SD card image
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"
rm -rf "${GENIMAGE_TMP}"

genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo "==> SD card image created: ${BINARIES_DIR}/sdcard.img"
