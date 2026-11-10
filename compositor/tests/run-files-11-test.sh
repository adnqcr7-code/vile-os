#!/bin/bash
# Vile Files v1.1 — test harness, stage 1
#
# Usage: ./run-files-11-test.sh
#   Runs the real Vile compositor headless (snapshot binary), opens
#   the real Files window, and captures REAL grim screenshots:
#
#   vile-files-11-sidebar.png       home grid, sidebar shows ALL
#                                   standard folders + This PC;
#                                   status bar shows free space
#   vile-files-11-devices.png       This PC page: real drives with
#                                   sizes + usage bars
#   vile-files-11-drive-added.png   after a tmpfs drive is mounted
#                                   at /mnt/vile-test-drive — the new
#                                   drive must appear LIVE in the list
#
# The mount is left in place for stage 2 (run-files-11-test2.sh),
# which clicks into the drive and then unmounts it live.
#
# NOTE: the Piece 7 snapshot binary auto-launches the Setup wizard
# on first boot (no ~/.config/vile/setup-done marker). If the marker
# is absent, this harness closes the wizard with its own X button
# (top-right of the right-hand tile) so Files gets the full width —
# it does NOT skip setup and does NOT create the marker.
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
APP="$HOME/workspace/vile-os/filemanager/vile-files.py"
DRIVE_MNT="/mnt/vile-test-drive"
mkdir -p "$SHOTS"

export XDG_RUNTIME_DIR="/tmp/vile-files11-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg"
export VILE_SOCKET_FILE="/tmp/vile-files11-socket.txt"
export VILE_QUIT_AFTER_SEC=240
export LC_ALL=C.UTF-8
unset DISPLAY
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
VILE_BIN="/tmp/vile-files-vile"
[ -x "$VILE_BIN" ] || cp ./build/vile "$VILE_BIN"

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-files11-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

# Click coords, measured from the probe shot (single tiled window
# 1896x932 at (12,12); see BUILD_LOG File Manager v1.1 section).
# Setup wizard close (X) — top-right of the right-hand tile:
WIZ_X=${WIZ_X:-1882}
WIZ_Y=${WIZ_Y:-36}
# "This PC" sidebar row centre:
PC_X=${PC_X:-110}
PC_Y=${PC_Y:-477}

echo "==> Starting compositor..."
rm -f "$VILE_SOCKET_FILE"
"$VILE_BIN" > /tmp/vile-files11-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-files11-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"

echo "==> Opening Files at the real home..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$APP" > /tmp/vile-files11-app.log 2>&1 &
APP_PID=$!
sleep 5

if [ ! -f "$HOME/.config/vile/setup-done" ]; then
  echo "==> No setup-done marker: closing the Setup wizard ($WIZ_X,$WIZ_Y)..."
  ./tests/vptr_test "$WIZ_X" "$WIZ_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
  sleep 2
else
  echo "==> setup-done marker present: no wizard to close."
fi

echo "==> Shot 1: $SHOTS/vile-files-11-sidebar.png (all standard folders)"
shot "$SHOTS/vile-files-11-sidebar.png"

echo "==> Clicking This PC ($PC_X,$PC_Y)..."
./tests/vptr_test "$PC_X" "$PC_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
sleep 2
echo "==> Shot 2: $SHOTS/vile-files-11-devices.png (This PC page)"
shot "$SHOTS/vile-files-11-devices.png"
echo "==> df cross-check (exact bytes):"
df -B1 / /home/hatch | cat

echo "==> Mounting the stand-in drive (tmpfs 64M at $DRIVE_MNT)..."
mkdir -p "$DRIVE_MNT"
mount -t tmpfs -o size=64M tmpfs "$DRIVE_MNT" 2>&1 || echo "MOUNT FAILED"
df -B1 "$DRIVE_MNT" | cat
sleep 3
echo "==> Shot 3: $SHOTS/vile-files-11-drive-added.png (drive must appear)"
shot "$SHOTS/vile-files-11-drive-added.png"

kill $APP_PID 2>/dev/null || true
sleep 1
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
echo "==> Stage 1 done. Drive left mounted for stage 2."
echo "==> App log tail:"
tail -5 /tmp/vile-files11-app.log
