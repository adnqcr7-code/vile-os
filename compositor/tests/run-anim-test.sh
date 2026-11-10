#!/bin/bash
# Vile Piece 7 — animation test.
#
# Usage: ./run-anim-test.sh
#   Runs the real Vile compositor headless with VILE_ANIM_DURATION_MS=6000
#   (slow-motion: every animation takes 6s, so grim can catch it
#   mid-flight) and captures REAL grim screenshots:
#
#   vile-anim-startup-mid.png   ~1.5s in: wallpaper ~25% faded in,
#                               bar half-raised, dock small + faded
#   vile-anim-dock-hover.png    pointer on the dock foot icon: the
#                               icon grown part-way (hover grow)
#   vile-anim-window-mid.png    foot ~40% opacity, slightly smaller
#                               (open fade + scale-up, mid-flight)
#   vile-anim-window-after.png  foot fully in, border focused purple
#   vile-anim-menu-mid.png      system menu ~40% risen + faded in
#   vile-anim-menu-after.png    menu fully open
#   vile-anim-close-mid.png     foot closed with Super+Q: its last
#                               frame fading out over the bare desktop
#   vile-anim-close-after.png   snapshot gone, bare desktop
#   vile-anim-lock-mid.png      Super+L: lock ~40% faded in, clock
#                               drifted up part-way
#   vile-anim-unlock-mid.png    correct password: lock ~60% faded out
#                               over the desktop
#   vile-anim-unlock-after.png  desktop back, lock gone
#
# All proof shots live under screenshots/ — NEVER /tmp. The dock
# hover uses a move-only vptr_test (no click). The lock test uses
# the test user viletest / password vile (VILE_LOCK_USER).
#
# All shots must be checked by eye before calling Piece 7 done.
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
LOG="/tmp/vile-anim-compositor.log"
mkdir -p "$SHOTS"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=150
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export VILE_NO_SETUP=1  # this test is about animations, not first-run
export VILE_ANIM_DURATION_MS=6000  # slow-motion for mid-flight shots
export LC_ALL=C.UTF-8
unset DISPLAY
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"

mkdir -p "$HOME/.config/foot"
cp "$HOME/workspace/vile-os/foot.ini" "$HOME/.config/foot/foot.ini"

export PATH="$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (unlocked, NO startup — bare desktop, slow-mo anims)..."
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

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-anim-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
move_to() { # $1 $2 = x y (no click)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" 2>&1 | tee -a /tmp/vile-anim-grim.log
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-anim-grim.log
}
press_chord() { # vkey_test <keysym>...
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-anim-grim.log
}
type_text() { # $1 = text to type (no Enter)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-anim-grim.log
}
press_key() { # wtype -k args — plain keys only, NEVER chords
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$@" 2>&1 | tee -a /tmp/vile-anim-grim.log
}

# Dock foot icon centre, from the compositor log (the dock widened
# to 4 icons Nov 10, 2026, so the foot centre moved 888 -> 852;
# never hardcode it again). Read after the shell is created.
DOCK_LINE="$(grep -m1 'dock foot icon centre' "$LOG" || true)"
DOCK_FOOT_X="$(echo "$DOCK_LINE" | sed -n 's/.*dock foot icon centre (\([0-9]*\),\([0-9]*\)).*/\1/p')"
DOCK_FOOT_Y="$(echo "$DOCK_LINE" | sed -n 's/.*dock foot icon centre (\([0-9]*\),\([0-9]*\)).*/\2/p')"
DOCK_FOOT_X="${DOCK_FOOT_X:-852}"
DOCK_FOOT_Y="${DOCK_FOOT_Y:-996}"
echo "==> Dock foot icon centre: ($DOCK_FOOT_X,$DOCK_FOOT_Y) [log: $DOCK_LINE]"

echo "==> [1] Startup mid-flight (1.5s in, 6s anim)..."
sleep 1.5
shot "$SHOTS/vile-anim-startup-mid.png"
grep -E "anim: startup" "$LOG" | tail -2

echo "==> [2] Wait for startup to finish, then hover the dock foot icon..."
sleep 5.5
move_to "$DOCK_FOOT_X" "$DOCK_FOOT_Y"
sleep 2.5
shot "$SHOTS/vile-anim-dock-hover.png"

echo "==> [3] Click dock foot icon -> foot opens (fade + scale, 6s)..."
click "$DOCK_FOOT_X" "$DOCK_FOOT_Y"
sleep 2.5
grep -E "anim: window open" "$LOG" | tail -2
shot "$SHOTS/vile-anim-window-mid.png"
sleep 4
shot "$SHOTS/vile-anim-window-after.png"

echo "==> [4] Click status cluster (1880,1064) -> menu rises..."
click 1880 1064
sleep 2.5
grep -m1 'shell menu opened' "$LOG" || echo "WARN: no menu-opened line in log"
shot "$SHOTS/vile-anim-menu-mid.png"
sleep 4
shot "$SHOTS/vile-anim-menu-after.png"

echo "==> [5] Click inside foot (closes menu, focuses foot), then Super+Q..."
click 960 500
sleep 0.5
press_chord Super_L q
sleep 2.5
grep -E "anim: close fade" "$LOG" | tail -2
shot "$SHOTS/vile-anim-close-mid.png"
sleep 4
shot "$SHOTS/vile-anim-close-after.png"

echo "==> [6] Super+L -> lock fades in..."
press_chord Super_L l
sleep 2.5
shot "$SHOTS/vile-anim-lock-mid.png"
sleep 4
echo "==> [6] Type password + Enter -> lock fades out..."
type_text "vile"
press_key -k Return
sleep 2.5
shot "$SHOTS/vile-anim-unlock-mid.png"
grep -E "unlock OK|anim: lock fade-out done" "$LOG" | tail -3
sleep 4
shot "$SHOTS/vile-anim-unlock-after.png"

echo "==> Anim log lines:"
grep -E "Vile: anim:" "$LOG" | head -30
echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Shots:"
ls -lh "$SHOTS"/vile-anim-*.png
