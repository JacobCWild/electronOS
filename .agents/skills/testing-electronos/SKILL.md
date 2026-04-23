# Testing electronOS Components

This skill covers how to test the electronOS login manager, shell, and desktop environment locally on a dev machine.

## Prerequisites

### System Dependencies
```bash
sudo apt-get install -y build-essential gcc make pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libpam0g-dev libreadline-dev fonts-dejavu-core \
  wmctrl xdotool
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

# Desktop
cd src/desktop && make clean && make
```

All compile with `-Wall -Wextra -Werror`. The compiler IS the linter — zero warnings means clean code.

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

## Testing the Desktop Environment

### Launching the Desktop
The desktop does NOT require sudo. The shell must be on PATH for the terminal app:
```bash
export PATH=/home/ubuntu/repos/electronOS/src/shell:$PATH
cd src/desktop
DISPLAY=:0 ./electronos-desktop --test >/tmp/desktop-stdout.log 2>/tmp/desktop-stderr.log &
```

### What `--test` Mode Does (Desktop)
- Opens a 1280x720 windowed SDL2 window (not fullscreen)
- Window title: "electronOS Desktop"
- All apps launchable from the dock

### Desktop UI Layout
- **Top panel** (28px): "Activities" left, clock center, "Power" right
- **Left dock** (56px wide): 3 icons — ">_" (Terminal), "*" (Settings), "A" (Text Editor)
- **Wallpaper**: Purple gradient background
- **Windows**: Glass titlebar (32px) with traffic-light buttons (red/yellow/green, 14px diameter)

### Interacting with SDL Windows via xdotool
The desktop is an SDL2 application inside an X11 window. To interact with it programmatically:

1. **Find the X11 window**: `DISPLAY=:0 xdotool search --name "electronOS Desktop" getwindowgeometry --shell`
2. **Focus it**: `wmctrl -i -a <WINDOW_ID>` or `DISPLAY=:0 xdotool windowactivate <WINDOW_ID>`
3. **Click on SDL elements**: Calculate actual screen coordinates = SDL coords + X11 window position

### Coordinate Mapping
The computer tool screenshots may use a different resolution than the actual screen. To click precisely on SDL elements:

1. Get actual screen resolution: `DISPLAY=:0 xdpyinfo | grep dimensions`
2. Get SDL window position: `DISPLAY=:0 xdotool search --name "electronOS Desktop" getwindowgeometry --shell`
3. Read the source code for element positions (constants in `window.h`, `dock.h`, `panel.h`)
4. Calculate: `actual_screen_coords = SDL_coords + window_position`
5. Use `DISPLAY=:0 xdotool mousemove --screen 0 <x> <y> && xdotool click 1`

**Key constants:**
- `PANEL_HEIGHT = 28` (panel.h)
- `DOCK_WIDTH = 56` (dock.h)
- `TITLEBAR_HEIGHT = 32` (window.h)
- `WINDOW_BTN_SIZE = 14` (window.h)
- `WINDOW_BTN_MARGIN = 8` (window.h)

### Window Positioning
Windows are cascaded. Position calculation (from `desktop.c`):
```
base_x = DOCK_WIDTH + 40 = 96
base_y = PANEL_HEIGHT + 30 = 58
offset = (next_window_id % 8) * 28
window_x = base_x + offset
window_y = base_y + offset
```
`next_window_id` starts at 1 and increments with each window created (does not decrement on close).

### Close Button Coordinates
For a window at SDL position (wx, wy):
```
close_center_x = wx + WINDOW_BTN_MARGIN + WINDOW_BTN_SIZE/2 = wx + 15
close_center_y = wy + (TITLEBAR_HEIGHT - WINDOW_BTN_SIZE)/2 + WINDOW_BTN_SIZE/2 = wy + 16
```
Hit test radius: `WINDOW_BTN_SIZE/2 + 2 = 9` pixels.

### Window Drag Testing
SDL processes mouse events internally. The computer tool's `left_click_drag` may not trigger SDL's motion events properly. Use xdotool with incremental movements instead:
```bash
DISPLAY=:0 xdotool mousemove --screen 0 <titlebar_x> <titlebar_y>
sleep 0.2
DISPLAY=:0 xdotool mousedown 1
sleep 0.2
for i in $(seq 1 10); do
  DISPLAY=:0 xdotool mousemove_relative -- 20 10
  sleep 0.05
done
sleep 0.2
DISPLAY=:0 xdotool mouseup 1
```

### Desktop Test Scenarios

1. **Panel rendering**: Verify "Activities", clock (updates every second), "Power" text
2. **Dock icons**: Click each icon to open Terminal, Settings, Text Editor
3. **Terminal app**: Should launch electronos-shell in a PTY. Type commands and verify output
4. **Settings app**: Click sidebar categories (Display, Network, Security, About). Each shows different content
5. **Text Editor**: Type text, verify line numbers in gutter. Press Enter to create new lines
6. **Window focus**: Open 2+ windows, click background window titlebar — it should come to front
7. **Window close**: Click red traffic-light button — window should disappear
8. **Window drag**: Drag titlebar — window should move

### Dock Icon Click Positions (SDL coordinates)
From `dock.c`, icon positions:
```
y_start = PANEL_HEIGHT + DOCK_PADDING + 4 = 28 + 8 + 4 = 40
icon_y(index) = y_start + index * (DOCK_ICON_SIZE + DOCK_PADDING) = 40 + index * 48
icon_x = (DOCK_WIDTH - DOCK_ICON_SIZE) / 2 = 8
icon_center_x = 8 + 20 = 28
```
- Terminal (index 0): center SDL (28, 60)
- Settings (index 1): center SDL (28, 108)
- Editor (index 2): center SDL (28, 156)

## Devin Secrets Needed

None — all testing is local. No external services or API keys required.

## Common Issues

- **"SDL_Init failed: No available video device"**: Need `xhost +local:root` before running with sudo, or set `DISPLAY=:0`
- **"Authorization required, but no authorization protocol specified"**: Same fix — `xhost +local:root`
- **"XDG_RUNTIME_DIR not set"**: Pass `XDG_RUNTIME_DIR=/run/user/1000` when running with sudo
- **PAM config missing after VM restart**: The `/etc/pam.d/electronos-login` file might not persist across session restarts — recreate it
- **Guest user missing**: Recreate with `sudo useradd -m -s /bin/bash Guest && echo Guest:guest | sudo chpasswd`
- **Logo not showing**: Make sure CWD is `src/login/` so the relative path `../../assets/electronOSlogo.png` resolves
- **Desktop process killed after system restart**: Background processes don't survive VM restarts. Relaunch with the command above.
- **Click not registering on SDL window**: Make sure the SDL window has focus. Use `wmctrl -i -a <WINDOW_ID>` to bring it to front. If another window (like konsole) is overlapping, it will intercept clicks.
- **Computer tool clicks miss small SDL buttons**: Screen resolution scaling (e.g., 1600x1122 actual vs 1024x768 tool) can cause imprecise coordinate mapping. Use xdotool with calculated actual screen coordinates from `xdotool getwindowgeometry` + code constants instead.
- **Window drag doesn't work via computer tool**: SDL processes mouse motion internally. Use xdotool with incremental `mousemove_relative` steps (see Window Drag Testing section above).
