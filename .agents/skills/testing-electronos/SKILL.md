# Testing electronOS Components

This skill covers how to test the electronOS login manager and shell locally on a dev machine.

## Prerequisites

### System Dependencies
```bash
sudo apt-get install -y build-essential gcc make pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libpam0g-dev libreadline-dev fonts-dejavu-core \
  wmctrl
```

### PAM Configuration
The login manager requires a PAM service config at `/etc/pam.d/electronos-login`:
```bash
sudo tee /etc/pam.d/electronos-login > /dev/null << 'EOF'
auth    required    pam_unix.so
account required    pam_unix.so
session required    pam_unix.so
EOF
```

### Guest Test User
Create a Guest user for testing authentication:
```bash
sudo useradd -m -s /bin/bash -c "Guest User" Guest
echo "Guest:guest" | sudo chpasswd
```

## Building Components

```bash
# Login manager
cd src/login && make clean && make

# Shell
cd src/shell && make clean && make
```

Both compile with `-Wall -Wextra -Werror`. The compiler IS the linter — zero warnings means clean code.

## Testing the Login Manager

### Important: X Display Authorization
The login manager needs `sudo` for PAM (reads `/etc/shadow`), but `sudo` drops X authorization. You must:
```bash
xhost +local:root
```
Then launch with:
```bash
cd src/login
sudo DISPLAY=:0 XDG_RUNTIME_DIR=/run/user/1000 ./electronos-login --test
```

### What `--test` Mode Does
- Opens a 1280x720 windowed SDL2 window (instead of fullscreen)
- On successful login, prints `LOGIN OK: user=<username> shell=<shell>` to stdout and exits
- Mouse cursor stays visible

### Logo Path Resolution
The login manager tries these paths in order:
1. `/usr/share/electronos/login/logo.png` (production)
2. `../../assets/electronOSlogo.png` (dev — relative to CWD)
3. `assets/electronOSlogo.png`

You must run from `src/login/` for the dev path to resolve correctly.

### Test Scenarios

1. **Successful auth**: Pre-filled "Guest" → Enter → type "guest" → Enter → should print `LOGIN OK: user=Guest shell=/bin/bash`
2. **Failed auth**: Enter wrong password → red border + shake animation (400ms), password cleared, auto-clears error after 3 seconds
3. **UI states**: USERNAME → (Enter) → PASSWORD → (Enter) → SUBMITTED → SUCCESS/ERROR
4. **Back navigation**: In password state, Escape or "< Back" returns to username state

### Capturing LOGIN OK Output
When running in background, redirect stdout to capture the message:
```bash
sudo DISPLAY=:0 XDG_RUNTIME_DIR=/run/user/1000 ./electronos-login --test >/tmp/login-stdout.log 2>/tmp/login-stderr.log &
# After login completes:
cat /tmp/login-stdout.log  # Should show: LOGIN OK: user=Guest shell=/bin/bash
cat /tmp/login-stderr.log  # Should be empty on success, or show PAM errors on failure
```

## Testing the Shell

```bash
cd src/shell && ./electronos-shell
```

No sudo needed. For visual testing, run in a GUI terminal (konsole):
```bash
DISPLAY=:0 konsole --noclose -e bash -c "cd /home/ubuntu/repos/electronOS/src/shell && ./electronos-shell" &
```

### What to Verify
- ASCII art "ELECTRON OS" banner in cyan
- "v1.0.0" version string
- "Welcome, {user}!" with timestamp
- Command table: 30 commands in 5 categories (Files: 10, Text: 5, System: 7, Network: 6, Shell: 2)
- Additional Built-ins section: whoami, history, alias, export, sysinfo, exit
- Colored prompt: `{user}@electronos:{cwd}$`

### Command Tests
- `pwd` — prints CWD
- `whoami` — prints username
- `echo hello world | grep hello` — tests piping
- `echo test > /tmp/file && cat /tmp/file` — tests redirection
- `sysinfo` — prints system info table
- `exit` — prints "Goodbye." and terminates

## Devin Secrets Needed

None — all testing is local. No external services or API keys required.

## Common Issues

- **"SDL_Init failed: No available video device"**: Need `xhost +local:root` before running with sudo
- **"Authorization required, but no authorization protocol specified"**: Same fix — `xhost +local:root`
- **"XDG_RUNTIME_DIR not set"**: Pass `XDG_RUNTIME_DIR=/run/user/1000` when running with sudo
- **PAM config missing after VM restart**: The `/etc/pam.d/electronos-login` file might not persist across session restarts — recreate it
- **Guest user missing**: Recreate with `sudo useradd -m -s /bin/bash Guest && echo Guest:guest | sudo chpasswd`
- **Logo not showing**: Make sure CWD is `src/login/` so the relative path `../../assets/electronOSlogo.png` resolves
