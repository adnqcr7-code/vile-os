#!/bin/bash
# Vile — bare-desktop shot at 1280x800 (QEMU resolution check)
#
# Usage: ./run-look-1280x800.sh
#   Runs the real Vile compositor headless at 1280x800 (VILE_OUT_W /
#   VILE_OUT_H test hook in main.c) with NO startup command and captures
#   ONE grim screenshot:
#
#   vile-look-01-1280x800.png  bare desktop — bottom bar + dock above
#                               it over the purple wallpaper
#
# Proves the bottom-bar layout holds at the QEMU resolution: bar
# flush to the bottom edge, dock 12px above it, no window or clock
# over the wallpaper. (This harness used to prove the desktop clock
# was resolution-aware — the clock was removed Nov 10, 2026.)
# The shot must be checked by eye before calling the fix done.
# All proof shots live under screenshots/ — NEVER /tmp.
set -u
cd "$(dirname "$0")/.."

OUT="$HOME/workspace/vile-os/screenshots/vile-look-01-1280x800.png"
LOG="/tmp/vile-look-1280-compositor.log"
mkdir -p "$(dirname "$OUT")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket-1280.txt"
export VILE_QUIT_AFTER_SEC=60
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export VILE_OUT_W=1280
export VILE_OUT_H=800
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"

export PATH="$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (unlocked, NO startup, 1280x800)..."
./build/vile > "$LOG" 2>&1 &
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
sleep 4  # shell (bar/dock) renders
grep -m1 "headless output, forcing" "$LOG" || true
grep -m1 "wallpaper fitted" "$LOG" || true

echo "==> Shot: bare desktop at 1280x800..."
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUT" 2>&1 | tee -a /tmp/vile-look-1280-grim.log
python3 -c "from PIL import Image; im=Image.open('$OUT'); print('shot:', '$OUT', im.size)" 2>&1 || file "$OUT"

wait $VILE_PID 2>/dev/null
echo "==> Done. Check the shot by eye: bottom bar + dock, nothing over the wallpaper."
