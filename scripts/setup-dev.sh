#!/bin/bash
# ============================================================================
# electronOS Development Environment Setup
# ============================================================================
# Sets up a development environment for building and testing electronOS
# components without a full Buildroot build.
#
# Usage: ./scripts/setup-dev.sh
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# ---- Spinner function for background tasks --------------------------------
spinner() {
    local spin=( '⠋' '⠙' '⠹' '⠸' '⠼' '⠴' '⠦' '⠧' '⠇' '⠏' )
    local i=0
    while [ -d /proc/$! ] 2>/dev/null; do
        printf "\r  ${YELLOW}${spin[$i]}${NC} "
        i=$(( (i+1) % ${#spin[@]} ))
        sleep 0.1
    done
    printf "\r"
}

echo -e "${CYAN}"
echo "  ┌─────────────────────────────────────┐"
echo "  │   electronOS Dev Environment Setup  │"
echo "  └─────────────────────────────────────┘"
echo -e "${NC}"

# ---- Install development dependencies ------------------------------------
echo -e "${CYAN}[1/3] Installing development dependencies...${NC}"

PACKAGES=(
    # Core build tools
    build-essential
    gcc
    make
    pkg-config

    # SDL2 for login manager
    libsdl2-dev
    libsdl2-image-dev
    libsdl2-ttf-dev

    # PAM for authentication
    libpam0g-dev

    # Readline for shell
    libreadline-dev

    # Fonts
    fonts-dejavu-core

    # Useful for testing
    valgrind
    gdb
    strace
)

echo -e "  Installing: ${YELLOW}${PACKAGES[*]}${NC}"
printf "  ${YELLOW}⠋${NC} Installing dependencies"
sudo apt-get update > /dev/null 2>&1 &
spinner
sudo apt-get install -y "${PACKAGES[@]}" > /dev/null 2>&1 &
spinner
printf "${GREEN}Done.${NC}\n"

# ---- Build components locally ---------------------------------------------
echo -e "\n${CYAN}[2/3] Building electronOS components...${NC}"

echo -e "  Building login manager..."
cd "${PROJECT_DIR}/src/login"
make clean 2>/dev/null || true
make
echo -e "  ${GREEN}Login manager built: src/login/electronos-login${NC}"

echo -e "  Building shell..."
cd "${PROJECT_DIR}/src/shell"
make clean 2>/dev/null || true
make
echo -e "  ${GREEN}Shell built: src/shell/electronos-shell${NC}"

# ---- Set up test PAM config -----------------------------------------------
echo -e "\n${CYAN}[3/3] Setting up test environment...${NC}"

# Create PAM config for testing (non-destructive)
if [[ ! -f /etc/pam.d/electronos-login ]]; then
    echo -e "  ${YELLOW}Creating test PAM config (requires sudo)...${NC}"
    sudo tee /etc/pam.d/electronos-login > /dev/null << 'EOF'
# electronOS test PAM config
auth    required    pam_unix.so
account required    pam_unix.so
session required    pam_unix.so
EOF
    echo -e "  ${GREEN}PAM config installed.${NC}"
else
    echo -e "  ${GREEN}PAM config already exists.${NC}"
fi

# ---- Summary --------------------------------------------------------------
echo ""
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo -e "${GREEN}  Development environment ready!              ${NC}"
echo -e "${GREEN}  ═══════════════════════════════════════════${NC}"
echo ""
echo "  Test the login manager:"
echo "    cd src/login && ./electronos-login --test"
echo ""
echo "  Test the shell:"
echo "    cd src/shell && ./electronos-shell"
echo ""
echo "  Full OS build:"
echo "    make build"
echo ""
