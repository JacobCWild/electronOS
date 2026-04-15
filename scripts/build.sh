#!/bin/bash
# ============================================================================
# electronOS Build Script
# ============================================================================
# One-command build for electronOS. Handles dependency checking, Buildroot
# download, configuration, and full image build.
#
# Usage:
#   ./scripts/build.sh              # Full build
#   ./scripts/build.sh --deps-only  # Install dependencies only
#   ./scripts/build.sh --clean      # Clean and rebuild
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

DEPS_ONLY=false
CLEAN=false
JOBS=$(nproc 2>/dev/null || echo 4)

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --deps-only)  DEPS_ONLY=true; shift ;;
        --clean)      CLEAN=true; shift ;;
        -j*)          JOBS="${1#-j}"; shift ;;
        --help)
            echo "Usage: build.sh [--deps-only] [--clean] [-jN]"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo -e "${CYAN}"
echo "  ┌─────────────────────────────────────┐"
echo "  │      electronOS Build System         │"
echo "  └─────────────────────────────────────┘"
echo -e "${NC}"

# ---- Step 1: Check / Install dependencies --------------------------------
echo -e "${CYAN}[1/4] Checking build dependencies...${NC}"

REQUIRED_PKGS=(
    build-essential
    gcc
    g++
    make
    patch
    gzip
    bzip2
    xz-utils
    perl
    cpio
    unzip
    rsync
    file
    bc
    wget
    git
    python3
    libncurses-dev
    libssl-dev
    device-tree-compiler
    qemu-user-static
)

MISSING=()
for pkg in "${REQUIRED_PKGS[@]}"; do
    if ! dpkg -l "$pkg" >/dev/null 2>&1; then
        MISSING+=("$pkg")
    fi
done

if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo -e "  ${YELLOW}Installing missing packages: ${MISSING[*]}${NC}"
    sudo apt-get update -qq
    sudo apt-get install -y -qq "${MISSING[@]}"
fi

echo -e "  ${GREEN}All dependencies satisfied.${NC}"

if $DEPS_ONLY; then
    echo -e "\n${GREEN}Dependencies installed. Exiting (--deps-only).${NC}"
    exit 0
fi

# ---- Step 2: Clean if requested ------------------------------------------
if $CLEAN; then
    echo -e "\n${CYAN}[2/4] Cleaning previous build...${NC}"
    cd "$PROJECT_DIR"
    make clean
fi

# ---- Step 3: Configure ---------------------------------------------------
echo -e "\n${CYAN}[2/4] Downloading Buildroot and configuring...${NC}"
cd "$PROJECT_DIR"
make setup
make config

# ---- Step 4: Build -------------------------------------------------------
echo -e "\n${CYAN}[3/4] Building electronOS image (this may take 30-90 minutes)...${NC}"
echo -e "  Using ${YELLOW}${JOBS}${NC} parallel jobs"

START_TIME=$(date +%s)

cd "$PROJECT_DIR"
make -C "build/buildroot-"* \
    BR2_EXTERNAL="$PROJECT_DIR/buildroot/external" \
    O="$PROJECT_DIR/output" \
    -j"$JOBS"

END_TIME=$(date +%s)
ELAPSED=$(( END_TIME - START_TIME ))
MINUTES=$(( ELAPSED / 60 ))
SECONDS=$(( ELAPSED % 60 ))

# ---- Done -----------------------------------------------------------------
echo ""
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  electronOS build complete! (${MINUTES}m ${SECONDS}s)${NC}"
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo ""
echo -e "  Image: ${CYAN}${PROJECT_DIR}/output/images/sdcard.img${NC}"
echo ""
echo -e "  To flash: ${YELLOW}make flash SD=/dev/sdX${NC}"
echo -e "  Or:       ${YELLOW}./scripts/flash.sh /dev/sdX${NC}"
echo ""
