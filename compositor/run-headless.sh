#!/bin/bash
# Vile compositor — headless test run (Piece 1 + Piece 2)
# Runs the real Vile compositor on wlroots' headless backend + pixman
# renderer, then captures a REAL screenshot with `grim` (an independent
# Wayland screencopy client) — proof of what the compositor presents.
# No mockup, no PIL render: if grim's PNG shows the purple Arch wallpaper
# (Piece 1) and the foot window on top of it (Piece 2), the compositor
# really drew it.
#
# Usage: ./run-headless.sh [seconds-to-run] [screenshot-path]
# Defaults: 6 seconds, ~/workspace/vile-os/screenshots/vile-screen-01.png
# Env: VILE_STARTUP="foot" — command the compositor runs once up (Piece 2).
set -u
cd "$(dirname "$0")"

SECONDS_RUN="${1:-6}"
OUT="${2:-$HOME/workspace/vile-os/screenshots/vile-screen-01.png}"
mkdir -p "$(dirname "$OUT")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC="$SECONDS_RUN"
export VILE_NO_SETUP=1  # Piece 7 Part B: headless runs are about the desktop, not first-run
rm -f "$VILE_SOCKET_FILE"

if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
  meson setup build --prefix=/usr/local 2>&1 | tail -3
  ninja -C build 2>&1 | tail -3
fi

echo "==> Starting Vile compositor (headless, ${SECONDS_RUN}s)..."
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
VILE_ARGS=()
if [ -n "${VILE_STARTUP:-}" ]; then
  echo "==> Startup command: $VILE_STARTUP"
  VILE_ARGS=(-s "$VILE_STARTUP")
fi
./build/vile "${VILE_ARGS[@]}" > /tmp/vile-compositor.log 2>&1 &
VILE_PID=$!

# Wait for the compositor to print its socket.
for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3  # let frames render and the startup client (foot) map its window

echo "==> Capturing screenshot with grim..."
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUT" 2>&1 | tee /tmp/vile-grim.log
GRIM_EXIT=${PIPESTATUS[0]}
if [ $GRIM_EXIT -ne 0 ] || [ ! -s "$OUT" ]; then
  echo "grim -o failed (exit $GRIM_EXIT). Trying without -o:"
  WAYLAND_DISPLAY="$SOCKET" grim "$OUT" 2>&1 | tee -a /tmp/vile-grim.log
  GRIM_EXIT=${PIPESTATUS[0]}
fi
# Honest check: a real screenshot must be a non-empty PNG with dimensions.
if [ -s "$OUT" ]; then
  python3 -c "from PIL import Image; im=Image.open('$OUT'); print('screenshot size:', im.size, im.mode)" 2>&1 || file "$OUT"
else
  echo "ERROR: screenshot is empty/missing"
  GRIM_EXIT=1
fi

wait $VILE_PID 2>/dev/null
echo "==> Compositor exited. Log tail:"
tail -20 /tmp/vile-compositor.log
echo "==> Screenshot: $OUT"
ls -lh "$OUT" 2>&1
file "$OUT" 2>&1
exit $GRIM_EXIT
