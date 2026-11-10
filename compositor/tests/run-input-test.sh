#!/bin/bash
# Vile Piece 3 — input test (keyboard typing, keybinds, pointer, Super+L)
#
# Usage: ./run-input-test.sh
#   Runs the real Vile compositor headless (wlroots headless backend +
#   pixman renderer), drives it with wtype (virtual keyboard) and
#   vptr_test (virtual pointer), and captures REAL grim screenshots:
#
#   vile-input-01.png  typed `echo vile` visible in the first foot,
#                      cursor moved + clicked inside foot first
#   vile-input-02.png  after Super+Enter (new foot, `echo second`)
#                      then Super+Q (that window closed) — back to the
#                      first foot with `echo vile` still there, then
#                      focus cleared by clicking the wallpaper frame
#                      (`echo nofocus` must NOT appear), focus
#                      restored by clicking inside foot (`echo yesfocus`
#                      visible), then Super+Escape clears focus again
#                      (`echo escaped` must NOT appear) and clicking
#                      inside foot once more restores it
#                      (`echo backagain` visible)
#   vile-input-03.png  after Super+L — clean v2 lock screen, no terminal
#   vile-input-03-unlocked.png (bonus) after typing the PAM password
#                      + Enter — overlay gone, foot back
#
# Focus-clear under tiling (Piece 4): a single tiled window covers
# the whole screen except the 12px outer frame (8px of it wallpaper)
# and the 4px wallpaper strips inside the inter-window gaps, so the
# old click at (200,200) lands INSIDE foot. The gap click here uses
# (5,540) — the outer-left wallpaper frame, mid-height. Super+Escape
# is the keybind path (same result, no click needed).
# All shots must be checked by eye before calling Piece 3 done.
# Intermediate shot: screenshots/vile-input-02-open.png (two feet, top one
#   showing `echo second` — proves Super+Enter opened a NEW window
#   and it got keyboard focus).
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-input-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-input-02.png"
OUT2OPEN="$HOME/workspace/vile-os/screenshots/vile-input-02-open.png"
OUT3="$HOME/workspace/vile-os/screenshots/vile-input-03.png"
OUT3U="$HOME/workspace/vile-os/screenshots/vile-input-03-unlocked.png"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=75
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
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

echo "==> Starting Vile compositor (unlocked, foot startup)..."
./build/vile -s foot > /tmp/vile-input-compositor.log 2>&1 &
VILE_PID=$!

for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-input-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3  # foot maps, gets focus

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-input-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
type_text() { # $1 = text to type (no Enter)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-input-grim.log
}
press_chord() { # vkey_test <keysym>... — holds all keys, releases in reverse
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-input-grim.log
}
press_key() { # wtype -k args — plain keys only, NEVER chords (wtype releases each -k key before the next)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$@" 2>&1 | tee -a /tmp/vile-input-grim.log
}

echo "==> [1] Pointer: move to (700,300) inside foot + click to focus"
./tests/vptr_test 700 300 click 2>&1 | tee -a /tmp/vile-input-grim.log
sleep 1
echo "==> [1] Typing 'echo vile' + Enter into foot"
type_text "echo vile"
press_key -k Return
sleep 1.5
echo "==> [1] Screenshot: $OUT1"
shot "$OUT1"

echo "==> [2] Keybind Super+Enter (open second foot) — via vkey_test chord"
press_chord Super_L Return
sleep 2
echo "==> [2] Typing 'echo second' + Enter into the NEW window"
type_text "echo second"
press_key -k Return
sleep 1.5
echo "==> [2] Intermediate shot (two feet, top shows 'echo second'): $OUT2OPEN"
shot "$OUT2OPEN"

echo "==> [2] Keybind Super+Q (close focused window) — via vkey_test chord"
press_chord Super_L q
sleep 1.5
echo "==> [2] Intermediate shot after close (back to first foot): /home/hatch/workspace/vile-os/screenshots/vile-input-02-closed.png"
shot "$HOME/workspace/vile-os/screenshots/vile-input-02-closed.png"

echo "==> [2] Pointer: click (5,540) on the outer wallpaper frame — must CLEAR focus"
echo "    (tiling covers the screen; only the 12px outer frame is clickable wallpaper)"
./tests/vptr_test 5 540 click 2>&1 | tee -a /tmp/vile-input-grim.log
sleep 1
echo "==> [2] Typing 'echo nofocus' — must NOT appear anywhere (no focus)"
type_text "echo nofocus"
press_key -k Return
sleep 1.5
echo "==> [2] Intermediate shot (no 'nofocus' line): /home/hatch/workspace/vile-os/screenshots/vile-input-02-nofocus.png"
shot "/home/hatch/workspace/vile-os/screenshots/vile-input-02-nofocus.png"

echo "==> [2] Pointer: click (700,300) back inside foot — must RESTORE focus"
./tests/vptr_test 700 300 click 2>&1 | tee -a /tmp/vile-input-grim.log
sleep 1
echo "==> [2] Typing 'echo yesfocus' — must appear in foot"
type_text "echo yesfocus"
press_key -k Return
sleep 1.5
echo "==> [2] Screenshot (focus restored, 'yesfocus' visible, no 'nofocus'): $OUT2"
shot "$OUT2"

echo "==> [2] Keybind Super+Escape (clear focus without a click) — via vkey_test chord"
press_chord Super_L Escape
sleep 1
echo "==> [2] Typing 'echo escaped' — must NOT appear anywhere (focus cleared by keybind)"
type_text "echo escaped"
press_key -k Return
sleep 1.5
echo "==> [2] Intermediate shot (no 'escaped' line; focused border must be dim): /home/hatch/workspace/vile-os/screenshots/vile-input-02-escaped.png"
shot "/home/hatch/workspace/vile-os/screenshots/vile-input-02-escaped.png"

echo "==> [2] Pointer: click (700,300) inside foot once more — focus must come back"
./tests/vptr_test 700 300 click 2>&1 | tee -a /tmp/vile-input-grim.log
sleep 1
echo "==> [2] Typing 'echo backagain' — must appear in foot"
type_text "echo backagain"
press_key -k Return
sleep 1.5
echo "==> [2] Intermediate shot ('backagain' visible, no 'escaped'/'nofocus'): /home/hatch/workspace/vile-os/screenshots/vile-input-02-refocus.png"
shot "/home/hatch/workspace/vile-os/screenshots/vile-input-02-refocus.png"

echo "==> [3] Keybind Super+L (lock now) — via vkey_test chord"
press_chord Super_L l
sleep 2
echo "==> [3] Screenshot (clean v2 lock, no terminal): $OUT3"
shot "$OUT3"

echo "==> [3] Unlocking the Super+L lock with the PAM password"
type_text "vile"
press_key -k Return
sleep 2
echo "==> [3] Type 'echo unlocked' so the post-unlock shot is NOT byte-identical to refocus (audit #2 must-fix #5)"
./tests/vptr_test 700 300 click 2>&1 | tee -a /tmp/vile-input-grim.log
sleep 1
type_text "echo unlocked"
press_key -k Return
sleep 1.5
echo "==> [3] Bonus shot after unlock (foot back, 'echo unlocked' visible): $OUT3U"
shot "$OUT3U"

wait $VILE_PID 2>/dev/null
echo "==> Key log lines:"
grep -E "Vile: (keybind|lock|unlock|window mapped|keyboard focus|virtual keyboard|pointer)" /tmp/vile-input-compositor.log || tail -30 /tmp/vile-input-compositor.log
echo "==> Shots:"
ls -lh "$OUT1" "$OUT2" "$OUT3" "$OUT3U" "$OUT2OPEN" /home/hatch/workspace/vile-os/screenshots/vile-input-02-closed.png /home/hatch/workspace/vile-os/screenshots/vile-input-02-nofocus.png /home/hatch/workspace/vile-os/screenshots/vile-input-02-escaped.png /home/hatch/workspace/vile-os/screenshots/vile-input-02-refocus.png 2>&1
