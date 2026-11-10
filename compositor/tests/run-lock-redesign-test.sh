#!/bin/bash
# Vile lock screen v2 — headless test (video-style redesign, Nov 10)
#
# Usage: ./run-lock-redesign-test.sh [correct|wrong] [password]
#   correct (default): types the real password for VILE_LOCK_USER and
#     expects the overlay to disappear AND the terminal to come back
#     (log: "unlock OK").
#   wrong: types a wrong password and expects the screen to STAY locked
#     with "Wrong password" (log: "unlock FAILED") and NO terminal
#     visible behind the lock UI at any point.
#
# Produces:
#   correct: ../screenshots/vile-lock-10.png (locked, clean) and
#            ../screenshots/vile-lock-11.png (unlocked — desktop + foot back)
#   wrong:   ../screenshots/vile-lock-10.png (locked, clean) and
#            ../screenshots/vile-lock-12.png (still locked, error shown)
#
# The locked shot is the point of this test: it must show ONLY the
# blurred purple Arch wallpaper + the floating lock UI (huge clock,
# date, user card, gear). No foot window, no title bar, no window
# edges anywhere. Check it by eye before calling this done.
set -u
cd "$(dirname "$0")/.."

MODE="${1:-correct}"
PASSWORD="${2:-vile}"
if [ "$MODE" = "wrong" ]; then
  PASSWORD="wrongpassword"
  OUT2="$HOME/workspace/vile-os/screenshots/vile-lock-12.png"
else
  OUT2="$HOME/workspace/vile-os/screenshots/vile-lock-11.png"
fi
OUT1="$HOME/workspace/vile-os/screenshots/vile-lock-10.png"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=25
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK=1
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export VILE_STARTUP="foot"
export LC_ALL=C.UTF-8
rm -f "$VILE_SOCKET_FILE"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting LOCKED Vile compositor v2 (user $VILE_LOCK_USER, mode $MODE)..."
./build/vile -l -s foot > /tmp/vile-lock2-compositor.log 2>&1 &
VILE_PID=$!

for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-lock2-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3

echo "==> [1] Locked screenshot (must be clean, no terminal): $OUT1"
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUT1" 2>&1 | tee /tmp/vile-lock2-grim.log
python3 -c "from PIL import Image; im=Image.open('$OUT1'); print('locked shot size:', im.size, im.mode)" 2>&1 || file "$OUT1"

echo "==> [2] Typing password via wtype (virtual keyboard) + Enter..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$PASSWORD" 2>&1 | tee -a /tmp/vile-lock2-grim.log
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype -k Return 2>&1 | tee -a /tmp/vile-lock2-grim.log
sleep 2

echo "==> [3] After-unlock screenshot: $OUT2"
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUT2" 2>&1 | tee -a /tmp/vile-lock2-grim.log
python3 -c "from PIL import Image; im=Image.open('$OUT2'); print('after shot size:', im.size, im.mode)" 2>&1 || file "$OUT2"

wait $VILE_PID 2>/dev/null
echo "==> Lock log lines:"
grep -E "Vile: (lock|unlock|virtual keyboard|window mapped|keyboard focus)" /tmp/vile-lock2-compositor.log || tail -20 /tmp/vile-lock2-compositor.log
echo "==> Locked shot: $OUT1"
echo "==> After shot:  $OUT2"
ls -lh "$OUT1" "$OUT2" 2>&1
