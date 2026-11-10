#!/bin/bash
# Vile — ScreenFit dock icon test (UI polish round)
#
# Usage: ./run-screenfit-test.sh
#   Runs the real Vile compositor headless (no startup command), drives
#   it with vptr_test (pointer clicks), and captures REAL grim
#   screenshots:
#
#   vile-screenfit-01.png  bare desktop — the dock has THREE icons
#                          (foot, Settings, ScreenFit) above the
#                          bottom bar
#   vile-screenfit-02.png  after clicking the ScreenFit dock icon —
#                          ScreenFit open (tiled), or the honest
#                          failure if it cannot run in this headless
#                          environment (report the exact error, do
#                          not fake the icon's behaviour)
#
# ScreenFit icon centre comes from the compositor log ("shell created:
# ... screenfit centre (x,y)"); the hardcoded value is a fallback
# only. At 1920x1080 on the bottom-bar layout the dock pill is
# 224x80 at (848,956) and the ScreenFit centre is (1032,996).
#
# All proof shots live under screenshots/ — NEVER /tmp. Logs stay in
# /tmp; they are not proof. All shots must be checked by eye.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-screenfit-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-screenfit-02.png"
LOG="/tmp/vile-screenfit-compositor.log"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=90
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"  # default wallpaper for these shots

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (unlocked, NO startup — bare desktop)..."
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
sleep 3  # shell (bar/dock) renders

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-screenfit-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-screenfit-grim.log
}

# ScreenFit icon centre, from the compositor log (fallback: design coords).
sleep 1
CREATED_LINE="$(grep -m1 'shell created:' "$LOG" || true)"
SF_X="$(echo "$CREATED_LINE" | sed -n 's/.*screenfit centre (\([0-9]*\),\([0-9]*\)).*/\1/p')"
SF_Y="$(echo "$CREATED_LINE" | sed -n 's/.*screenfit centre (\([0-9]*\),\([0-9]*\)).*/\2/p')"
SF_X="${SF_X:-1032}"
SF_Y="${SF_Y:-996}"
echo "==> ScreenFit dock icon centre: ($SF_X,$SF_Y) [log: $CREATED_LINE]"

echo "==> Shot 01: bare desktop (dock with three icons)..."
shot "$OUT1"

echo "==> Click ScreenFit dock icon ($SF_X,$SF_Y)..."
click "$SF_X" "$SF_Y"
sleep 4
grep -E "dock: open ScreenFit|window mapped 'ScreenFit'|ScreenFit" "$LOG" | tail -5 || true
echo "==> Shot 02: ScreenFit open (or the honest failure)..."
shot "$OUT2"

echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Compositor exited. Log tail:"
tail -5 "$LOG"
echo "==> Shots: $OUT1 $OUT2"
ls -lh "$OUT1" "$OUT2"
