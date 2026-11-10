#!/bin/bash
# Probe only: start the headless compositor, open Files at home, take
# one screenshot so the This PC sidebar row can be measured.
# Usage: ./probe-files-11.sh
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
APP="$HOME/workspace/vile-os/filemanager/vile-files.py"
mkdir -p "$SHOTS"

export XDG_RUNTIME_DIR="/tmp/vile-files11-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg"
export VILE_SOCKET_FILE="/tmp/vile-files11-socket.txt"
export VILE_QUIT_AFTER_SEC=120
export LC_ALL=C.UTF-8
unset DISPLAY
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
VILE_BIN="/tmp/vile-files-vile"

rm -f "$VILE_SOCKET_FILE"
"$VILE_BIN" > /tmp/vile-files11-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "compositor up on $SOCKET (pid $VILE_PID)"
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$APP" > /tmp/vile-files11-app.log 2>&1 &
APP_PID=$!
sleep 5
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$SHOTS/vile-files-11-probe.png" 2>&1
python3 -c "from PIL import Image; print(Image.open('$SHOTS/vile-files-11-probe.png').size)"
kill $APP_PID 2>/dev/null || true
sleep 1
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
echo "probe done; app log tail:"
tail -5 /tmp/vile-files11-app.log
