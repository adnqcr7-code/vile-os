#!/bin/bash
# Vile lock v2 — settings panel test (gear click via virtual pointer).
#
# Starts the compositor locked, clicks the settings gear (bottom-right)
# with the virtual-pointer test client, and screenshots the panel.
# Then clicks the gear again to close it. Verifies the log lines
# "lock settings panel opened/closed".
#
# Produces ../screenshots/vile-lock-13.png (panel open) — check it by
# eye: SETTINGS, Hide password toggle, Reveal delay, SYSTEM Reboot /
# Power Off, on the right edge, over the blurred wallpaper only.
set -u
cd "$(dirname "$0")/.."

OUT="$HOME/workspace/vile-os/screenshots/vile-lock-13.png"
mkdir -p "$(dirname "$OUT")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=15
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK=1
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export VILE_STARTUP="foot"
export LC_ALL=C.UTF-8
rm -f "$VILE_SOCKET_FILE"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./tests/vptr_test ]; then
  echo "Building vptr_test..."
  (cd tests && /usr/local/bin/wayland-scanner client-header /tmp/vile-wlroots/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml wlr-virtual-pointer-unstable-v1-client-protocol.h && /usr/local/bin/wayland-scanner private-code /tmp/vile-wlroots/wlroots/protocol/wlr-virtual-pointer-unstable-v1.xml wlr-virtual-pointer-unstable-v1-client-protocol.c && gcc vptr_test.c wlr-virtual-pointer-unstable-v1-client-protocol.c -o vptr_test $(pkg-config --cflags --libs wayland-client))
fi

echo "==> Starting LOCKED Vile compositor v2..."
./build/vile -l -s foot > /tmp/vile-lock3-compositor.log 2>&1 &
VILE_PID=$!

for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-lock3-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3

echo "==> Clicking settings gear (1870,1030) via virtual pointer..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test 1870 1030 click
sleep 1.5
echo "==> Panel screenshot: $OUT"
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUT" 2>&1 | tee /tmp/vile-lock3-grim.log
python3 -c "from PIL import Image; im=Image.open('$OUT'); print('panel shot size:', im.size, im.mode)" 2>&1 || file "$OUT"

echo "==> Clicking gear again to close the panel..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test 1870 1030 click
sleep 1

wait $VILE_PID 2>/dev/null
echo "==> Settings log lines:"
grep -E "Vile: lock settings" /tmp/vile-lock3-compositor.log || echo "(none — gear click did NOT register)"
echo "==> Panel shot: $OUT"
ls -lh "$OUT" 2>&1
