#!/bin/bash
# Vile Piece 4 — tiling test (master-stack layout, focus borders,
# focus keybinds, fullscreen toggle, re-tile on close, wallpaper hook)
#
# Usage: ./run-tiling-test.sh
#   Runs the real Vile compositor headless, drives it with vkey_test
#   (key chords) and wtype (typing), captures REAL grim screenshots:
#
#   vile-tiling-01.png  1 window (foot) — fills its tile (1896x1056),
#                       NOT the old centred 960x600
#   vile-tiling-02.png  2 windows tiled side by side, both visible,
#                       focused one with the purple border
#   vile-tiling-03.png  3 windows tiled (master left, stack right x2),
#                       focus moved with Super+J, 'echo focus2' typed
#                       into the middle window
#   screenshots/vile-tiling-fullscreen.png
#                       Super+F on the focused window — fills the whole
#                       output, other windows hidden
#   screenshots/vile-tiling-retile.png
#                       after Super+F off + Super+Q (close focused) —
#                       the remaining 2 windows re-tiled to halves
#   screenshots/vile-tiling-wallpaper-nebula.png (bonus)
#                       wallpaper hook: ~/.config/vile/wallpaper ->
#                       vile-nebula.jpg, 1 window on the nebula
#
# All shots must be checked by eye before calling Piece 4 done.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-tiling-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-tiling-02.png"
OUT3="$HOME/workspace/vile-os/screenshots/vile-tiling-03.png"
OUTFS="$HOME/workspace/vile-os/screenshots/vile-tiling-fullscreen.png"
OUTRE="$HOME/workspace/vile-os/screenshots/vile-tiling-retile.png"
OUTWP="$HOME/workspace/vile-os/screenshots/vile-tiling-wallpaper-nebula.png"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=120
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_STARTUP="foot"
export LC_ALL=C.UTF-8
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"  # default wallpaper for shots 1-3

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (unlocked, foot startup)..."
./build/vile -s foot > /tmp/vile-tiling-compositor.log 2>&1 &
VILE_PID=$!

for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-tiling-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3  # foot maps, gets tiled

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-tiling-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
type_text() { # $1 = text to type (no Enter)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-tiling-grim.log
}
press_chord() { # vkey_test <keysym>... — holds all keys, releases in reverse
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-tiling-grim.log
}
press_key() { # wtype -k args — plain keys only, NEVER chords
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$@" 2>&1 | tee -a /tmp/vile-tiling-grim.log
}

echo "==> [1] Screenshot, 1 window tiled full-area: $OUT1"
shot "$OUT1"

echo "==> [2] Super+Enter (second foot), type 'echo second'"
press_chord Super_L Return
sleep 2
type_text "echo second"
press_key -k Return
sleep 1.5
echo "==> [2] Screenshot, 2 windows side by side: $OUT2"
shot "$OUT2"

echo "==> [3] Super+Enter (third foot), type 'echo third'"
press_chord Super_L Return
sleep 2
type_text "echo third"
press_key -k Return
sleep 1.5
echo "==> [3] Super+J (focus previous = middle window), type 'echo focus2'"
press_chord Super_L j
sleep 1
type_text "echo focus2"
press_key -k Return
sleep 1.5
echo "==> [3] Screenshot, 3 windows tiled, focus in middle: $OUT3"
shot "$OUT3"

echo "==> [F] Super+F (fullscreen the focused middle window)"
press_chord Super_L f
sleep 2
echo "==> [F] Screenshot, fullscreen: $OUTFS"
shot "$OUTFS"

echo "==> [F] Super+F again (restore tiling)"
press_chord Super_L f
sleep 2
echo "==> [F] Intermediate shot after restore: /home/hatch/workspace/vile-os/screenshots/vile-tiling-restored.png"
shot "/home/hatch/workspace/vile-os/screenshots/vile-tiling-restored.png"

echo "==> [R] Super+Q (close the focused middle window) — rest must re-tile"
press_chord Super_L q
sleep 2
echo "==> [R] Screenshot, 2 windows re-tiled to halves: $OUTRE"
shot "$OUTRE"

wait $VILE_PID 2>/dev/null
echo "==> Tiling log lines:"
grep -E "Vile: (tiling|keybind|window mapped|window unmapped|keyboard focus)" /tmp/vile-tiling-compositor.log || tail -30 /tmp/vile-tiling-compositor.log
echo "==> Shots:"
ls -lh "$OUT1" "$OUT2" "$OUT3" "$OUTFS" "$OUTRE" /home/hatch/workspace/vile-os/screenshots/vile-tiling-restored.png 2>&1

# --- Wallpaper hook bonus: separate short run with the hook file set ---
echo "==> [W] Wallpaper hook: ~/.config/vile/wallpaper -> vile-nebula.jpg"
mkdir -p "$HOME/.config/vile"
echo "$HOME/workspace/vile-os/wallpapers/vile-nebula.jpg" > "$HOME/.config/vile/wallpaper"
export VILE_QUIT_AFTER_SEC=12
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
unset VILE_WALLPAPER
rm -f "$VILE_SOCKET_FILE"
./build/vile -s foot > /tmp/vile-tiling-wp-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ -f "$VILE_SOCKET_FILE" ]; then
  SOCKET="$(cat "$VILE_SOCKET_FILE")"
  sleep 3
  echo "==> [W] Screenshot, nebula wallpaper behind tiled foot: $OUTWP"
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$OUTWP" 2>&1 | tee -a /tmp/vile-tiling-grim.log
  python3 -c "from PIL import Image; im=Image.open('$OUTWP'); print('shot:', '$OUTWP', im.size)" 2>&1 || file "$OUTWP"
  wait $VILE_PID 2>/dev/null
  grep -E "wallpaper" /tmp/vile-tiling-wp-compositor.log | head -5
else
  echo "ERROR: wallpaper-hook run did not start"
  cat /tmp/vile-tiling-wp-compositor.log
fi
rm -f "$HOME/.config/vile/wallpaper"  # restore default for later runs
echo "==> Wallpaper hook file removed again (default purple Arch restored)"
ls -lh "$OUTWP" 2>&1
