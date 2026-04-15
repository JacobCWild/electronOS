#!/bin/bash
# ============================================================================
# electronOS post-build script
# Runs after the root filesystem is assembled but before image creation.
# ============================================================================
set -euo pipefail

TARGET_DIR="$1"
BOARD_DIR="$(dirname "$0")"

echo "==> electronOS post-build: Applying security hardening..."

# ----------------------------------------------------------------------------
# 1. Set secure file permissions
# ----------------------------------------------------------------------------
chmod 700 "${TARGET_DIR}/root"
chmod 750 "${TARGET_DIR}/etc/sudoers.d" 2>/dev/null || true
chmod 600 "${TARGET_DIR}/etc/shadow" 2>/dev/null || true
chmod 600 "${TARGET_DIR}/etc/gshadow" 2>/dev/null || true

# ----------------------------------------------------------------------------
# 2. Remove unnecessary SUID/SGID binaries
# ----------------------------------------------------------------------------
KEEP_SUID=(
    "/usr/bin/passwd"
    "/usr/bin/su"
    "/usr/bin/sudo"
    "/usr/bin/chsh"
    "/usr/bin/newgrp"
)

find "${TARGET_DIR}" -perm /6000 -type f | while read -r binary; do
    rel_path="${binary#${TARGET_DIR}}"
    keep=false
    for allowed in "${KEEP_SUID[@]}"; do
        if [ "$rel_path" = "$allowed" ]; then
            keep=true
            break
        fi
    done
    if [ "$keep" = false ]; then
        echo "    Removing SUID/SGID from: $rel_path"
        chmod ug-s "$binary"
    fi
done

# ----------------------------------------------------------------------------
# 3. Enable essential systemd services
# ----------------------------------------------------------------------------
mkdir -p "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants"
mkdir -p "${TARGET_DIR}/etc/systemd/system/graphical.target.wants"

# Enable AppArmor
if [ -f "${TARGET_DIR}/usr/lib/systemd/system/apparmor.service" ]; then
    ln -sf /usr/lib/systemd/system/apparmor.service \
        "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/apparmor.service"
fi

# Enable firewall
if [ -f "${TARGET_DIR}/usr/lib/systemd/system/nftables.service" ]; then
    ln -sf /usr/lib/systemd/system/nftables.service \
        "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/nftables.service"
fi

# Enable audit daemon
if [ -f "${TARGET_DIR}/usr/lib/systemd/system/auditd.service" ]; then
    ln -sf /usr/lib/systemd/system/auditd.service \
        "${TARGET_DIR}/etc/systemd/system/multi-user.target.wants/auditd.service"
fi

# Enable login manager
if [ -f "${TARGET_DIR}/usr/lib/systemd/system/electronos-login.service" ]; then
    ln -sf /usr/lib/systemd/system/electronos-login.service \
        "${TARGET_DIR}/etc/systemd/system/graphical.target.wants/electronos-login.service"
fi

# Set graphical target as default
ln -sf /usr/lib/systemd/system/graphical.target \
    "${TARGET_DIR}/etc/systemd/system/default.target"

# ----------------------------------------------------------------------------
# 4. Configure Guest account (default fallback account)
# ----------------------------------------------------------------------------
if ! grep -q "^guest:" "${TARGET_DIR}/etc/passwd" 2>/dev/null; then
    echo "guest:x:1000:1000:Guest User:/home/guest:/usr/bin/electronos-shell" \
        >> "${TARGET_DIR}/etc/passwd"
    echo "guest:x:1000:" >> "${TARGET_DIR}/etc/group"
    # Password: "guest" (SHA-512 hash)
    GUEST_HASH='$6$rounds=10000$electronOS$K8j3VkHDNqJf3Yz.X5tQz0aGrZKp3yd5FJ9Q2KxBVrT1qN5mWz7cR8xY2pL4kA6hJ0vN3sU9wE1iO7gD5fM/'
    echo "guest:${GUEST_HASH}:19500:0:99999:7:::" >> "${TARGET_DIR}/etc/shadow"
    mkdir -p "${TARGET_DIR}/home/guest"
    chown 1000:1000 "${TARGET_DIR}/home/guest"
    chmod 700 "${TARGET_DIR}/home/guest"
fi

# ----------------------------------------------------------------------------
# 5. Configure kernel parameters for security
# ----------------------------------------------------------------------------
cat > "${TARGET_DIR}/etc/sysctl.d/90-electronos-security.conf" << 'EOF'
# electronOS Security Kernel Parameters
# ======================================

# Restrict kernel pointer exposure
kernel.kptr_restrict = 2

# Restrict dmesg access to root
kernel.dmesg_restrict = 1

# Disable magic SysRq key
kernel.sysrq = 0

# Enable ASLR (full randomization)
kernel.randomize_va_space = 2

# Restrict ptrace to parent processes only
kernel.yama.ptrace_scope = 1

# Restrict unprivileged user namespaces
kernel.unprivileged_userns_clone = 0

# Network security
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0
net.ipv4.conf.all.log_martians = 1
net.ipv4.tcp_syncookies = 1
net.ipv4.icmp_echo_ignore_broadcasts = 1
net.ipv6.conf.all.accept_redirects = 0
net.ipv6.conf.default.accept_redirects = 0

# Filesystem hardening
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
fs.protected_fifos = 2
fs.protected_regular = 2
fs.suid_dumpable = 0

# Restrict core dumps
kernel.core_pattern = |/bin/false
EOF

# ----------------------------------------------------------------------------
# 6. Configure login banner
# ----------------------------------------------------------------------------
cat > "${TARGET_DIR}/etc/issue" << 'EOF'

  ┌─────────────────────────────────────┐
  │          electronOS v1.0.0          │
  │    Authorized access only.          │
  │    All activity is monitored.       │
  └─────────────────────────────────────┘

EOF

cat > "${TARGET_DIR}/etc/motd" << 'EOF'

  Welcome to electronOS
  Type 'help' to see available commands.

EOF

echo "==> electronOS post-build: Done."
