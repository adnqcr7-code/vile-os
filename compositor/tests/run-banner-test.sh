#!/bin/bash
# Vile — foot welcome banner test (UI polish round)
#
# Usage: ./run-banner-test.sh
#   Runs the real Vile compositor headless with a foot startup window,
#   and captures REAL grim screenshots:
#
#   vile-banner-01.png  fresh foot — the Vile banner on top (name +
#                       version + keybind hints), then a prompt
#   vile-banner-02.png  after typing `echo banner-ok` — banner still
#                       there above, command output below, proving
#                       typing works normally after the banner
#
# The banner comes from compositor/vile-foot.sh, which the compositor
# runs instead of plain foot (vile_spawn_foot) — startup, Super+Enter
# and the dock icon all get it from the same place. The banner hands
# over to the interactive shell, so typing and job control work
# exactly as in plain foot.
#
# NOTE (test-env quirk, not a bug): this box has foot 1.16.2, which
# predates the [colors-dark] split, so the shot will also show a red
# "[colors-dark]: invalid section name" line under the banner. The
# shipped Arch foot 1.28 does not print it (proven in the booted ISO).
#
# All proof shots live under screenshots/ — NEVER /tmp. Logs stay in
# /tmp; they are not proof. All shots must be checked by eye.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-banner-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-banner-02.png"
LOG="/tmp/vile-banner-compositor.log"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=60
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"  # default wallpaper for these shots

# Foot's dark title bar: foot reads ~/.config/foot/foot.ini.
mkdir -p "$HOME/.config/foot"
cp "$HOME/workspace/vile-os/foot.ini" "$HOME/.config/foot/foot.ini"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (unlocked, foot startup)..."
./build/vile -s foot > "$LOG" 2>&1 &
VILE_PID=$!

for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat "$LOG"
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3  # foot opens + banner prints

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-banner-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

echo "==> Shot 01: fresh foot with the Vile banner..."
shot "$OUT1"
grep -E "foot|banner" "$LOG" | tail -3 || true

echo "==> Type 'echo banner-ok' + Enter (typing must work after the banner)..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "echo banner-ok" 2>&1 | tee -a /tmp/vile-banner-grim.log
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype -k Return 2>&1 | tee -a /tmp/vile-banner-grim.log
sleep 1.5
echo "==> Shot 02: banner + 'banner-ok' output..."
shot "$OUT2"

echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Compositor exited. Log tail:"
tail -5 "$LOG"
echo "==> Shots: $OUT1 $OUT2"
ls -lh "$OUT1" "$OUT2"
