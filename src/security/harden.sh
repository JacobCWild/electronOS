#!/bin/bash
# ============================================================================
# electronOS Security Hardening Script
# ============================================================================
# Run this script on a live electronOS system to apply additional
# security hardening beyond what's configured at build time.
#
# Usage: sudo electronos-harden [--full | --check]
#   --full   Apply all hardening (including potentially breaking changes)
#   --check  Audit-only mode: report findings without making changes
# ============================================================================
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

AUDIT_ONLY=false
FULL_MODE=false
ISSUES=0
FIXED=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)  AUDIT_ONLY=true; shift ;;
        --full)   FULL_MODE=true; shift ;;
        --help)
            echo "Usage: electronos-harden [--full | --check]"
            echo "  --check  Audit only (no changes)"
            echo "  --full   Apply all hardening measures"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

if [[ $EUID -ne 0 ]] && [[ "$AUDIT_ONLY" != true ]]; then
    echo -e "${RED}Error: This script must be run as root${NC}"
    exit 1
fi

echo -e "${CYAN}"
echo "  ┌─────────────────────────────────────┐"
echo "  │   electronOS Security Hardener       │"
echo "  └─────────────────────────────────────┘"
echo -e "${NC}"

if $AUDIT_ONLY; then
    echo -e "${YELLOW}Mode: AUDIT ONLY (no changes will be made)${NC}\n"
else
    echo -e "${GREEN}Mode: APPLY HARDENING${NC}\n"
fi

# ---- Helper functions ----------------------------------------------------
check() {
    local desc="$1"
    local condition="$2"

    if eval "$condition"; then
        echo -e "  ${GREEN}[PASS]${NC} $desc"
    else
        echo -e "  ${RED}[FAIL]${NC} $desc"
        ((ISSUES++))
    fi
}

fix() {
    local desc="$1"
    shift

    if ! $AUDIT_ONLY; then
        echo -e "  ${YELLOW}[FIX]${NC}  $desc"
        "$@"
        ((FIXED++))
    fi
}

# ---- 1. Kernel Parameters ------------------------------------------------
echo -e "${CYAN}── Kernel Parameters ──${NC}"

check "ASLR enabled" \
    "[[ \$(cat /proc/sys/kernel/randomize_va_space) -ge 2 ]]" || \
    fix "Enable full ASLR" sysctl -q kernel.randomize_va_space=2

check "Kernel pointer restriction" \
    "[[ \$(cat /proc/sys/kernel/kptr_restrict) -ge 1 ]]" || \
    fix "Restrict kernel pointers" sysctl -q kernel.kptr_restrict=2

check "dmesg restricted" \
    "[[ \$(cat /proc/sys/kernel/dmesg_restrict) -eq 1 ]]" || \
    fix "Restrict dmesg" sysctl -q kernel.dmesg_restrict=1

check "SysRq disabled" \
    "[[ \$(cat /proc/sys/kernel/sysrq) -eq 0 ]]" || \
    fix "Disable SysRq" sysctl -q kernel.sysrq=0

check "ptrace restricted" \
    "[[ \$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null) -ge 1 ]]" || \
    fix "Restrict ptrace" sysctl -q kernel.yama.ptrace_scope=1

# ---- 2. Network Security -------------------------------------------------
echo -e "\n${CYAN}── Network Security ──${NC}"

check "TCP SYN cookies enabled" \
    "[[ \$(cat /proc/sys/net/ipv4/tcp_syncookies) -eq 1 ]]" || \
    fix "Enable SYN cookies" sysctl -q net.ipv4.tcp_syncookies=1

check "ICMP redirects disabled" \
    "[[ \$(cat /proc/sys/net/ipv4/conf/all/accept_redirects) -eq 0 ]]" || \
    fix "Disable ICMP redirects" sysctl -q net.ipv4.conf.all.accept_redirects=0

check "Source routing disabled" \
    "[[ \$(cat /proc/sys/net/ipv4/conf/all/accept_source_route) -eq 0 ]]" || \
    fix "Disable source routing" sysctl -q net.ipv4.conf.all.accept_source_route=0

check "Reverse path filtering" \
    "[[ \$(cat /proc/sys/net/ipv4/conf/all/rp_filter) -eq 1 ]]" || \
    fix "Enable RP filter" sysctl -q net.ipv4.conf.all.rp_filter=1

check "Firewall active" \
    "nft list ruleset 2>/dev/null | grep -q 'chain input'" || true

# ---- 3. File System Security ---------------------------------------------
echo -e "\n${CYAN}── File System Security ──${NC}"

check "/etc/shadow permissions" \
    "[[ \$(stat -c %a /etc/shadow 2>/dev/null) =~ ^(600|640)$ ]]" || \
    fix "Fix shadow permissions" chmod 600 /etc/shadow

check "/etc/gshadow permissions" \
    "[[ \$(stat -c %a /etc/gshadow 2>/dev/null) =~ ^(600|640)$ ]]" || \
    fix "Fix gshadow permissions" chmod 600 /etc/gshadow

check "Hardlink protection" \
    "[[ \$(cat /proc/sys/fs/protected_hardlinks) -eq 1 ]]" || \
    fix "Enable hardlink protection" sysctl -q fs.protected_hardlinks=1

check "Symlink protection" \
    "[[ \$(cat /proc/sys/fs/protected_symlinks) -eq 1 ]]" || \
    fix "Enable symlink protection" sysctl -q fs.protected_symlinks=1

check "Core dumps disabled" \
    "[[ \$(cat /proc/sys/fs/suid_dumpable) -eq 0 ]]" || \
    fix "Disable SUID core dumps" sysctl -q fs.suid_dumpable=0

# ---- 4. SUID/SGID Audit --------------------------------------------------
echo -e "\n${CYAN}── SUID/SGID Binary Audit ──${NC}"
SUID_COUNT=$(find / -perm /6000 -type f 2>/dev/null | wc -l)
echo -e "  Found ${YELLOW}${SUID_COUNT}${NC} SUID/SGID binaries"
if [[ $SUID_COUNT -gt 10 ]]; then
    echo -e "  ${YELLOW}[WARN]${NC} Consider auditing SUID binaries:"
    find / -perm /6000 -type f 2>/dev/null | head -20 | while read -r f; do
        echo "         $f"
    done
fi

# ---- 5. Service Audit ----------------------------------------------------
echo -e "\n${CYAN}── Service Audit ──${NC}"

check "AppArmor loaded" \
    "systemctl is-active apparmor >/dev/null 2>&1" || true

check "Firewall service active" \
    "systemctl is-active nftables >/dev/null 2>&1" || true

check "SSH (if present) has key-only auth" \
    "! grep -qE '^PasswordAuthentication yes' /etc/ssh/sshd_config 2>/dev/null" || \
    fix "Disable SSH password auth" \
    sed -i 's/^PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config

# ---- 6. User Account Audit -----------------------------------------------
echo -e "\n${CYAN}── User Account Audit ──${NC}"

# Check for accounts with empty passwords
EMPTY_PASS=$(awk -F: '($2 == "" || $2 == "!") && $1 != "root" {print $1}' /etc/shadow 2>/dev/null | wc -l)
check "No empty passwords" "[[ $EMPTY_PASS -eq 0 ]]" || true

# Check root is not directly accessible
check "Root login disabled (SSH)" \
    "! grep -qE '^PermitRootLogin yes' /etc/ssh/sshd_config 2>/dev/null" || true

# Check umask
check "Secure default umask (077)" \
    "grep -q 'umask 077' /etc/profile 2>/dev/null || grep -q 'umask 0077' /etc/login.defs 2>/dev/null" || true

# ---- 7. Encrypted Storage ------------------------------------------------
echo -e "\n${CYAN}── Encrypted Storage ──${NC}"

if command -v cryptsetup >/dev/null 2>&1; then
    echo -e "  ${GREEN}[OK]${NC}   cryptsetup available (LUKS support ready)"
    # Check if data partition is encrypted
    if lsblk -o NAME,FSTYPE 2>/dev/null | grep -q "crypto_LUKS"; then
        echo -e "  ${GREEN}[OK]${NC}   Encrypted partition detected"
    else
        echo -e "  ${YELLOW}[INFO]${NC} No encrypted partitions found"
        echo -e "         Run 'electronos-encrypt-data' to set up encrypted storage"
    fi
else
    echo -e "  ${RED}[FAIL]${NC} cryptsetup not installed"
fi

# ---- Summary --------------------------------------------------------------
echo ""
echo -e "${CYAN}  ─── Summary ───${NC}"
echo -e "  Issues found: ${RED}${ISSUES}${NC}"
if ! $AUDIT_ONLY; then
    echo -e "  Issues fixed: ${GREEN}${FIXED}${NC}"
fi
echo ""

if [[ $ISSUES -eq 0 ]]; then
    echo -e "  ${GREEN}System is properly hardened!${NC}"
else
    if $AUDIT_ONLY; then
        echo -e "  ${YELLOW}Run without --check to apply fixes.${NC}"
    else
        echo -e "  ${YELLOW}Some issues may require manual intervention.${NC}"
    fi
fi
echo ""
