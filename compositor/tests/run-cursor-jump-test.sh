#!/bin/bash
# Vile cursor + launch jump test (0.5.6)
#
# Usage: ./run-cursor-jump-test.sh [OUT_W OUT_H]
#   Default 1920x1080; pass 1280 800 for the QEMU resolution.
#   Runs the real Vile compositor headless with a scratch HOME
#   (no setup-done marker, so the Setup icon shows) and proves:
#
#   1. CURSOR: the default pointer is the PURPLE arrow (#C574FE),
#      not the old white arrow — measured by counting pixels in
#      the exact cursor purple in a box at the pointer, before
#      (baseline, no cursor there) vs after moving the pointer.
#      NOTE: the headless backend's set_cursor is a no-op, so the
#      cursor never reaches a grim frame unless the software
#      cursor is forced — this harness sets
#      WLR_NO_HARDWARE_CURSORS=1 (test env only; the Dell uses
#      the DRM backend, where the cursor is real either way).
#   2. JUMP: clicking the Files dock icon starts the KDE-style
#      bounce — the icon's bright-pixel CENTROID (pill box, dot
#      row excluded) moves up >= 8px in at least one mid-flight
#      shot. Files is used because it is a slow GTK launch; the
#      test slows Python startup by 2s (a PATH wrapper, test
#      only) so the ~2.9s launch is observable with grim's
#      ~0.75s shots at the shipped 500ms bounce period. Foot
#      maps too fast (~0.2s) to catch with grim.
#   3. DONE: the jump stops when the Files window maps ("launch
#      done 'files'" in the log) and the icon returns to rest.
#   4. GUARD: a second click on Settings while it is still
#      launching spawns it only ONCE (no duplicate
#      "dock: open Settings").
#   5. FALLBACK: with XCURSOR_PATH pointing at an empty dir, the
#      compositor logs the missing-theme fallback and still runs
#      (built-in default cursor).
#
# The 5s timeout path is proven in the QEMU boot test instead
# (Settings under TCG takes ~37s to open; the icon must be back
# at rest after ~7s with no window yet).
set -u
cd "$(dirname "$0")/.."

OUT_W="${1:-1920}"
OUT_H="${2:-1080}"
SHOTS="$HOME/workspace/vile-os/screenshots"
TEST_HOME="/tmp/vile-cursor-jump-home"
mkdir -p "$SHOTS" "$TEST_HOME/.config/vile"
rm -f "$TEST_HOME/.config/vile/setup-done" \
  "$TEST_HOME/.config/vile/dock-pinned" \
  "$TEST_HOME/.config/vile/dock-setup-hidden"

export HOME="$TEST_HOME"
export XDG_RUNTIME_DIR="/tmp/vile-cursor-jump-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
# Headless set_cursor is a no-op: force the software cursor so the
# scene renderer draws it into the frame grim captures.
export WLR_NO_HARDWARE_CURSORS=1
export VILE_WALLPAPER="/home/hatch/workspace/vile-os/wallpapers/vile-nebula.jpg"
export VILE_SOCKET_FILE="/tmp/vile-cursor-jump-socket.txt"
export VILE_QUIT_AFTER_SEC=180
export VILE_NO_SETUP=1
export VILE_LOCK_USER=viletest
export VILE_OUT_W="$OUT_W"
export VILE_OUT_H="$OUT_H"
export VPTR_W="$OUT_W"
export VPTR_H="$OUT_H"
# Slow the bounce for the test (grim takes ~0.75s per shot).
# Use the SHIPPED default 500ms: with the slow-Python wrapper
# below, the launch lasts ~2.9s, so the 500ms bounce is caught
# mid-flight by several of the 0.75s-spaced shots.
export VILE_LAUNCH_JUMP_MS=500
export LC_ALL=C.UTF-8
unset DISPLAY
# TEST-ONLY slow launch: a python3 wrapper that sleeps 2s before
# exec'ing the real python3. The dock's Files/Settings launchers
# call `python3` from PATH, so their windows take ~2.9s to map
# and the jump is observable with grim's ~0.75s shots. The
# wrapper is prepended to PATH ONLY for the compositor process
# (below), so the harness shell's own python3 calls are unaffected
# and no shipped file is touched.
mkdir -p /tmp/vile-slow-python
cat > /tmp/vile-slow-python/python3 <<'PYEOF'
#!/bin/sh
# Test-only: slow Python startup so the launch jump is observable.
sleep 2
exec /usr/bin/python3 "$@"
PYEOF
chmod +x /tmp/vile-slow-python/python3
# The theme lives at /usr/share/icons/vile (installed by
# vile-install.sh / staged in the ISO). This harness must NOT
# depend on it being in a scratch HOME.
unset XCURSOR_PATH

REAL_HOME="/home/hatch"
export PATH="$REAL_HOME/.local/bin:/usr/local/bin:/usr/bin:/bin"
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

LOG="/tmp/vile-cursor-jump-compositor.log"
LOG2="/tmp/vile-cursor-jump-fallback.log"
PASS=0; FAIL=0
check() { # $1 = name, $2 = 1/0
  if [ "$2" = "1" ]; then PASS=$((PASS+1)); echo "PASS $1";
  else FAIL=$((FAIL+1)); echo "FAIL $1"; fi
}
has() { grep -q "$1" "${2:-$LOG}" 2>/dev/null && echo 1 || echo 0; }
count() { grep -c "$1" "${2:-$LOG}" 2>/dev/null || true; }

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-cursor-jump-grim.log
}
click() { ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-cursor-jump-grim.log; }
move() { ./tests/vptr_test "$1" "$2" 2>&1 | tee -a /tmp/vile-cursor-jump-grim.log; }

mkdir -p "$TEST_HOME/.config/foot"
cp "$REAL_HOME/workspace/vile-os/compositor/foot.ini" "$TEST_HOME/.config/foot/foot.ini" 2>/dev/null || true

echo "==> Building compositor (no-op if current)..."
ninja -C build 2>&1 | tail -1
echo "==> Starting Vile compositor (${OUT_W}x${OUT_H}, nebula)..."
rm -f "$VILE_SOCKET_FILE"
PATH="/tmp/vile-slow-python:$PATH" ./build/vile > "$LOG" 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do [ -f "$VILE_SOCKET_FILE" ] && break; sleep 0.1; done
[ -f "$VILE_SOCKET_FILE" ] || { echo "ERROR: compositor did not start"; cat "$LOG"; exit 1; }
SOCKET="$(cat "$VILE_SOCKET_FILE")"
export WAYLAND_DISPLAY="$SOCKET"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
sleep 3

centres_line() {
  grep "dock foot icon centre" "$LOG" | tail -1
}
centre() { # $1 = log line, $2 = full token
  echo "$1" | sed -n "s/.*$2 (\\([0-9]*\\),\\([0-9]*\\)).*/\\1 \\2/p"
}
parse_centres() {
  local line
  line="$(centres_line)"
  FOOT_C="$(centre "$line" "dock foot icon centre")"
  SETTINGS_C="$(centre "$line" "settings centre")"
  FILES_C="$(centre "$line" "files centre")"
  FOOT_X="${FOOT_C%% *}"; FOOT_Y="${FOOT_C##* }"
  SETTINGS_X="${SETTINGS_C%% *}"; SETTINGS_Y="${SETTINGS_C##* }"
  FILES_X="${FILES_C%% *}"; FILES_Y="${FILES_C##* }"
}
parse_centres
echo "==> Dock centres: foot ($FOOT_X,$FOOT_Y) settings ($SETTINGS_X,$SETTINGS_Y) files ($FILES_X,$FILES_Y)"

# Icon centroid: mean y of bright pixels in the pill box around an
# icon centre (x cx+-28, y cy-40..cy+28 — the icon's full range at
# rest plus the 14px jump above it; the running-dot row below the
# icon is excluded). The frosted wallpaper behind the pill is dim
# enough that the icon glyph + spinner dominate the count.
icon_centroid() { # $1 = png, $2 = centre x, $3 = centre y
  python3 - "$1" "$2" "$3" <<'PYEOF'
import sys
from PIL import Image
import numpy as np
im = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(int)
cx, cy = int(sys.argv[2]), int(sys.argv[3])
box = im[max(0,cy-40):cy+28, cx-28:cx+28]
bright = box.max(axis=2) > 180
ys, _ = np.where(bright)
if len(ys) == 0:
    print("-1")
else:
    print(f"{ys.mean() + max(0,cy-40):.1f}")
PYEOF
}

# ---------------- Phase 1: purple cursor ------------------------
# The compositor starts with the cursor at screen centre. Move it
# to a dark patch of nebula and count pixels in the exact cursor
# purple #C574FE (197,116,254) +/-15 in a 30x30 box at the pointer.
# Baseline: the same box BEFORE the move (cursor is at centre).
shot "$SHOTS/vile-cursor-00-bare.png"
BASE_PURPLE="$(python3 - "$SHOTS/vile-cursor-00-bare.png" 400 300 <<'PYEOF'
import sys
from PIL import Image
import numpy as np
im = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(int)
x, y = int(sys.argv[2]), int(sys.argv[3])
box = im[y:y+30, x:x+30]
near = ((abs(box[...,0]-197)<=15) & (abs(box[...,1]-116)<=15)
        & (abs(box[...,2]-254)<=15)).sum()
print(int(near))
PYEOF
)"
move 400 300; sleep 1
shot "$SHOTS/vile-cursor-01-moved.png"
MOVE_PURPLE="$(python3 - "$SHOTS/vile-cursor-01-moved.png" 400 300 <<'PYEOF'
import sys
from PIL import Image
import numpy as np
im = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(int)
x, y = int(sys.argv[2]), int(sys.argv[3])
box = im[y:y+30, x:x+30]
near = ((abs(box[...,0]-197)<=15) & (abs(box[...,1]-116)<=15)
        & (abs(box[...,2]-254)<=15)).sum()
print(int(near))
PYEOF
)"
echo "    cursor-purple pixels: baseline=$BASE_PURPLE moved=$MOVE_PURPLE"
check "1 cursor theme loaded (purple arrow log)" "$(has "cursor theme 'vile' (purple arrow)")"
check "1b purple cursor visible at pointer" "$([ "$MOVE_PURPLE" -ge 50 ] && [ "$MOVE_PURPLE" -gt "$BASE_PURPLE" ] && echo 1 || echo 0)"

# ---------------- Phase 2: launch jump (Files) ------------------
# Hover the Files icon first (rest pose WITH hover grow, so the
# displacement measured is the jump only, not the hover), then
# click and take 5 shots — at least one must catch the icon
# centroid >= 8px above its rest centroid.
move "$FILES_X" "$FILES_Y"; sleep 1
shot "$SHOTS/vile-jump-00-rest.png"
REST_C="$(icon_centroid "$SHOTS/vile-jump-00-rest.png" "$FILES_X" "$FILES_Y")"
echo "    Files icon rest centroid y=$REST_C"
click "$FILES_X" "$FILES_Y"
BEST_DISP=0
for i in 1 2 3 4 5; do
  shot "$SHOTS/vile-jump-0$i-mid$i.png"
  C="$(icon_centroid "$SHOTS/vile-jump-0$i-mid$i.png" "$FILES_X" "$FILES_Y")"
  echo "    mid$i centroid y=$C"
  D="$(python3 -c "print(f'{float('$REST_C')-float('$C'):.1f}')" 2>/dev/null || echo 0)"
  BIG="$(python3 -c "print(1 if float('$D') > $BEST_DISP else 0)")"
  [ "$BIG" = "1" ] && BEST_DISP="$D"
done
echo "    best upward displacement: ${BEST_DISP}px"
check "2 jump starts on dock click" "$(has "launch jump start 'files'")"
check "2b icon displaced upward >= 8px mid-flight" "$(python3 -c "print(1 if float('$BEST_DISP') >= 8 else 0)")"

# ---------------- Phase 3: jump stops on window open ------------
sleep 3
shot "$SHOTS/vile-jump-06-open.png"
OPEN_C="$(icon_centroid "$SHOTS/vile-jump-06-open.png" "$FILES_X" "$FILES_Y")"
echo "    Files icon centroid after open: $OPEN_C (rest was $REST_C)"
check "3 jump stops when window maps" "$(has "launch done 'files'")"
check "3b icon back at rest after open" "$(python3 -c "print(1 if abs(float('$OPEN_C')-float('$REST_C')) <= 3 else 0)")"
check "3c Files window open (spawn log)" "$(has "dock: open Files")"

# ---------------- Phase 4: re-click guard (Settings) ------------
# Settings is not open yet. Click it twice in quick succession
# (the second lands while it is still launching) — it must spawn
# ONCE.
echo "==> Phase 4: double-click Settings while launching..."
S_BEFORE="$(count "dock: open Settings")"
click "$SETTINGS_X" "$SETTINGS_Y"; sleep 0.25
click "$SETTINGS_X" "$SETTINGS_Y"; sleep 2
S_AFTER="$(count "dock: open Settings")"
echo "    open-Settings lines before=$S_BEFORE after=$S_AFTER"
check "4 second click while launching does not re-spawn" "$([ "$((S_AFTER - S_BEFORE))" -eq 1 ] && echo 1 || echo 0)"
shot "$SHOTS/vile-jump-07-settings.png"

kill $VILE_PID 2>/dev/null || true
sleep 1

# ---------------- Phase 5: cursor fallback ----------------------
# XCURSOR_PATH at an empty dir: the theme cannot be found, so the
# compositor must log the fallback and still start (built-in
# default cursor). Short run, no screenshots needed. Uses its OWN
# log file — the main log must survive for the checks above.
echo "==> Phase 5: cursor theme missing (fallback)..."
mkdir -p /tmp/vile-empty-cursors
export XCURSOR_PATH="/tmp/vile-empty-cursors"
export VILE_SOCKET_FILE="/tmp/vile-cursor-fallback-socket.txt"
rm -f "$VILE_SOCKET_FILE"
./build/vile > "$LOG2" 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do [ -f "$VILE_SOCKET_FILE" ] && break; sleep 0.1; done
if [ -f "$VILE_SOCKET_FILE" ]; then
  sleep 2
  check "5 missing theme falls back (log line)" "$(has "falling back to system default cursor" "$LOG2")"
  check "5b compositor still runs without the theme" "1"
else
  echo "ERROR: fallback-run compositor did not start"; tail -20 "$LOG2"
  check "5 missing theme falls back (log line)" "0"
  check "5b compositor still runs without the theme" "0"
fi
kill $VILE_PID 2>/dev/null || true
sleep 1
export HOME="$REAL_HOME"
unset XCURSOR_PATH
echo ""
if [ "$FAIL" = "0" ]; then echo "==> CURSOR-JUMP TEST (${OUT_W}x${OUT_H}): ALL $PASS PASS";
else echo "==> CURSOR-JUMP TEST (${OUT_W}x${OUT_H}): $FAIL FAILURES (pass $PASS)"; fi
echo "==> Shots: $SHOTS/vile-cursor-*.png $SHOTS/vile-jump-*.png"
exit "$FAIL"
