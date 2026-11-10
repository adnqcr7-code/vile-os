#!/bin/bash
# Vile Files v1.1 — test harness, stage 2
#
# Usage: ./run-files-11-test2.sh
#   Prereq: stage 1 left the tmpfs drive mounted at
#   /mnt/vile-test-drive (re-mounts it if it is gone).
#   Captures REAL grim screenshots:
#
#   vile-files-11-drive-open.png    clicked into the new drive —
#                                   its (empty) listing, path bar
#                                   shows the mount point
#   vile-files-11-drive-removed.png after the drive is unmounted —
#                                   it must disappear LIVE from This PC
#
# Click coords for the drive row come from the stage-1 shot
# (DRIVE_X / DRIVE_Y env vars; see BUILD_LOG File Manager v1.1).
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

WIZ_X=${WIZ_X:-1882}
WIZ_Y=${WIZ_Y:-36}
PC_X=${PC_X:-110}
PC_Y=${PC_Y:-477}
DRIVE_X=${DRIVE_X:-400}
DRIVE_Y=${DRIVE_Y:-298}

echo "==> Making sure the stand-in drive is mounted..."
mkdir -p "$DRIVE_MNT"
mountpoint -q "$DRIVE_MNT" || mount -t tmpfs -o size=64M tmpfs "$DRIVE_MNT" 2>&1 || echo "MOUNT FAILED"
df -B1 "$DRIVE_MNT" | cat

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
  echo "==> No setup-done marker: closing the Setup wizard..."
  ./tests/vptr_test "$WIZ_X" "$WIZ_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
  sleep 2
fi

echo "==> Clicking This PC ($PC_X,$PC_Y)..."
./tests/vptr_test "$PC_X" "$PC_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
sleep 2

echo "==> Clicking the new drive row ($DRIVE_X,$DRIVE_Y)..."
./tests/vptr_test "$DRIVE_X" "$DRIVE_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
sleep 2
echo "==> Shot 1: $SHOTS/vile-files-11-drive-open.png (inside the drive)"
shot "$SHOTS/vile-files-11-drive-open.png"

echo "==> Back to This PC ($PC_X,$PC_Y)..."
./tests/vptr_test "$PC_X" "$PC_Y" click 2>&1 | tee -a /tmp/vile-files11-grim.log
sleep 1.5

echo "==> Unmounting the stand-in drive live..."
umount "$DRIVE_MNT" 2>&1 || echo "UMOUNT FAILED"
sleep 3
echo "==> Shot 2: $SHOTS/vile-files-11-drive-removed.png (drive must be gone)"
shot "$SHOTS/vile-files-11-drive-removed.png"

kill $APP_PID 2>/dev/null || true
sleep 1
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
rmdir "$DRIVE_MNT" 2>/dev/null || true
echo "==> Stage 2 done."
echo "==> App log tail:"
tail -5 /tmp/vile-files11-app.log
