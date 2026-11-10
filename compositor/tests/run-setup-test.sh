#!/bin/bash
# Vile Setup — first-run wizard test (Piece 7, Part B).
#
# Usage: ./run-setup-test.sh
#   Runs the real Vile compositor headless TWICE, with a FRESH temp
#   HOME (/tmp/vile-setup-home) so no test state leaks into the real
#   ~/.config/vile/ (the wizard writes the marker, display name,
#   wallpaper hook, timezone and profile picture there):
#
#   RUN 1 — no marker: the compositor must spawn the wizard INSTEAD
#   of the desktop (first thing on screen). The harness drives the
#   whole flow with the virtual pointer + keyboard and captures
#   REAL grim screenshots of every step:
#     vile-setup-01-welcome.png    Welcome — big V, "Welcome to Vile"
#     vile-setup-02-name.png       name step ("Adnan" typed, AD
#                                  picture picked via the real chooser)
#     vile-setup-03-picker-open.png  the GTK file chooser, open
#     vile-setup-04-name-picture.png name step after picking — the
#                                  avatar must be the AD picture
#     vile-setup-05-wallpaper.png  wallpaper step (Purple Arch current)
#     vile-setup-06-wallpaper-nebula.png  Nebula clicked — border +
#                                  "Current" moved, wallpaper behind
#                                  the wizard already swapped
#     vile-setup-07-apps.png       apps step (LibreOffice OFF, the
#                                  rest ON, ScreenFit Included/locked)
#     vile-setup-08-timezone.png   timezone step (America/Toronto)
#     vile-setup-09-done.png       summary (Adnan, picture set,
#                                  Nebula, apps, America/Toronto)
#     vile-setup-10-installing.png installing screen (password row
#                                  hidden in dry-run, log visible)
#     vile-setup-11-installing-done.png  dry-run log — the EXACT
#                                  commands a real Arch run would
#                                  execute, "Open desktop" shown
#     vile-setup-12-desktop.png    bare desktop after Finish — the
#                                  NEBULA wallpaper must be live
#     vile-setup-13-lock.png       Super+L — lock must show "Adnan"
#                                  (display name, not "viletest") and
#                                  the AD avatar
#     vile-setup-14-unlocked.png   after PAM unlock — desktop back
#   RUN 2 — marker exists: the compositor must go STRAIGHT to the
#   bare desktop (no wizard, log must NOT contain the first-run line):
#     vile-setup-15-second-boot.png
#
#   The install itself runs in DRY-RUN mode (VILE_SETUP_DRY_RUN=1):
#   this VM has no pacman, so the real Arch installs are
#   unproven-until-Dell. The dry-run log proves the UI + flow +
#   command construction; nothing is faked as installed.
#
#   Click points are NOT hardcoded: setup-coords.py scans each
#   screenshot for the button/entry/frame and returns its centre
#   (the picker test's coords-from-shot pattern). The chooser's
#   Open button uses the compositor log's tile origin + 905,30,
#   same as run-settings-picker-test.sh.
#
# All shots must be checked by eye before calling Setup done.
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
COORDS="./tests/setup-coords.py"
TEST_AVATAR="/tmp/vile-test-avatar.jpg"
TEST_HOME="/tmp/vile-setup-home"
CLOG="/tmp/vile-setup-compositor.log"
APPLOG="/tmp/vile-setup-app.log"
mkdir -p "$SHOTS"
[ -s "$TEST_AVATAR" ] || { echo "ERROR: test avatar missing: $TEST_AVATAR"; exit 1; }

# The temp HOME is ONLY the runtime home for the compositor + wizard
# (so the marker/profile/wallpaper-hook land there, not in the real
# ~/.config/vile/). Project paths below MUST use the real home —
# exporting HOME first broke them (VILE_WALLPAPER pointed at
# /tmp/vile-setup-home/workspace/... and the compositor refused to
# run black on the first run of this harness).
REAL_HOME="$HOME"
export HOME="$TEST_HOME"
export XDG_RUNTIME_DIR="/tmp/vile-setup-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_SOCKET_FILE="/tmp/vile-setup-wayland-socket.txt"
# NO VILE_WALLPAPER here on purpose (same rule as
# run-wallpaper-test.sh): the env var wins over the hook file, so
# exporting it would pin the desktop to Purple Arch and the
# wizard's Nebula pick would never show. Without it the hook wins:
# the hook is pre-seeded to Purple Arch below, so the compositor
# starts on Purple Arch and the hook takes over the moment the
# wizard writes Nebula into it.
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export VILE_SETUP_DRY_RUN=1  # this VM has no pacman — log, don't run
export LC_ALL=C.UTF-8
unset DISPLAY
unset VILE_NO_SETUP  # run 1 MUST spawn the wizard
export PATH="$REAL_HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

# Fresh temp HOME: no marker, no profile. The wallpaper hook IS
# pre-seeded to Purple Arch: since Nov 10, 2026 the compositor picks
# a RANDOM default (purple Arch or nebula) when no hook exists, and
# this test needs a deterministic start (Purple Arch current at the
# wallpaper step) so the Nebula click is a real swap.
rm -rf "$TEST_HOME"
mkdir -p "$TEST_HOME/.config/foot"
mkdir -p "$TEST_HOME/.config/vile"
echo "$REAL_HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg" > "$TEST_HOME/.config/vile/wallpaper"
cp "$REAL_HOME/workspace/vile-os/foot.ini" "$TEST_HOME/.config/foot/foot.ini"

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-setup-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-setup-grim.log
}
press_chord() { # vkey_test <keysym>...
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-setup-grim.log
}
type_text() { # $1 = text to type (no Enter)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-setup-grim.log
}
press_key() { # wtype -k args — plain keys only, NEVER chords
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$@" 2>&1 | tee -a /tmp/vile-setup-grim.log
}
tile_origin() { # $1 = xdg title -> "X Y" of its last tile in $CLOG
  grep "Vile: tiling: window.*'$1' at " "$CLOG" 2>/dev/null \
    | tail -1 | sed -E "s/.* at ([0-9]+),([0-9]+) [0-9]+x[0-9]+/\1 \2/"
}
start_compositor() { # $1 = extra args (no -s in run 1: wizard instead)
  export VILE_QUIT_AFTER_SEC=300  # safety net; harness kills it sooner
  rm -f "$VILE_SOCKET_FILE"
  ./build/vile $1 > "$CLOG" 2>&1 &
  VILE_PID=$!
  for i in $(seq 1 50); do
    [ -f "$VILE_SOCKET_FILE" ] && break
    sleep 0.1
  done
  if [ ! -f "$VILE_SOCKET_FILE" ]; then
    echo "ERROR: compositor did not start. Log:"; cat "$CLOG"; exit 1
  fi
  SOCKET="$(cat "$VILE_SOCKET_FILE")"
  echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
}
continue_at() { # $1 = screenshot to scan for the Continue button
  XY="$(python3 "$COORDS" continue "$1")" \
    || { echo "FAIL: Continue button not found in $1"; exit 1; }
  [ -n "$XY" ] || { echo "FAIL: empty Continue coords for $1"; exit 1; }
  echo "    Continue button at ($XY)"
  click ${XY% *} ${XY#* }
}

echo "================ RUN 1: first boot, no marker ================"
echo "==> Starting compositor (no startup command — wizard instead)..."
start_compositor ""
sleep 4
grep -m1 "first run — opening Setup wizard" "$CLOG" \
  || echo "WARN: no first-run spawn line in log"
echo "==> [1] Screenshot: vile-setup-01-welcome.png"
shot "$SHOTS/vile-setup-01-welcome.png"

echo "==> [2] Continue -> name step"
continue_at "$SHOTS/vile-setup-01-welcome.png"
sleep 1.5
shot "$SHOTS/vile-setup-02-name.png"
ENTRY="$(python3 "$COORDS" name-entry "$SHOTS/vile-setup-02-name.png")" \
  || { echo "FAIL: name entry not found"; exit 1; }
echo "    Name entry at ($ENTRY)"
click ${ENTRY% *} ${ENTRY#* }
sleep 0.5
press_chord Control_L a   # select the default (root) name
type_text "Adnan"
sleep 0.5

echo "==> [2] Choose picture... -> real GTK file chooser"
CHOOSE="$(python3 "$COORDS" choose-picture "$SHOTS/vile-setup-02-name.png")" \
  || { echo "FAIL: Choose picture button not found"; exit 1; }
echo "    Choose picture button at ($CHOOSE)"
click ${CHOOSE% *} ${CHOOSE#* }
sleep 2
echo "==> [2] Screenshot: vile-setup-03-picker-open.png"
shot "$SHOTS/vile-setup-03-picker-open.png"
echo "==> [2] Ctrl+L, type the path, Enter, click Open"
press_chord Control_L l
sleep 1
type_text "$TEST_AVATAR"
sleep 1
press_key -k Return
sleep 1.5
OPEN_XY="$(tile_origin "Choose profile picture")"
if [ -n "$OPEN_XY" ]; then
  OPEN_X=$(( ${OPEN_XY%% *} + 905 )); OPEN_Y=$(( ${OPEN_XY##* } + 30 ))
  echo "    Dialog tile origin: $OPEN_XY -> Open at ($OPEN_X,$OPEN_Y)"
else
  OPEN_X=1871; OPEN_Y=514
  echo "    WARNING: dialog tile not in log, falling back to proven ($OPEN_X,$OPEN_Y)"
fi
click "$OPEN_X" "$OPEN_Y"
sleep 3
echo "==> [2] Screenshot: vile-setup-04-name-picture.png (avatar must be the AD picture)"
shot "$SHOTS/vile-setup-04-name-picture.png"

echo "==> [3] Continue -> wallpaper step"
continue_at "$SHOTS/vile-setup-04-name-picture.png"
sleep 1.5
echo "==> [3] Screenshot: vile-setup-05-wallpaper.png (Purple Arch current)"
shot "$SHOTS/vile-setup-05-wallpaper.png"
NEBULA="$(python3 "$COORDS" wallpaper-nebula "$SHOTS/vile-setup-05-wallpaper.png")" \
  || { echo "FAIL: Nebula row not found"; exit 1; }
echo "    Nebula row at ($NEBULA)"
click ${NEBULA% *} ${NEBULA#* }
sleep 2  # compositor live-reloads the wallpaper behind the wizard
echo "==> [3] Screenshot: vile-setup-06-wallpaper-nebula.png (Nebula current, wallpaper swapped)"
shot "$SHOTS/vile-setup-06-wallpaper-nebula.png"

echo "==> [4] Continue -> apps step"
continue_at "$SHOTS/vile-setup-06-wallpaper-nebula.png"
sleep 1.5
echo "==> [4] Screenshot: vile-setup-07-apps.png (LibreOffice OFF, rest ON, ScreenFit Included)"
shot "$SHOTS/vile-setup-07-apps.png"

echo "==> [5] Continue -> timezone step"
continue_at "$SHOTS/vile-setup-07-apps.png"
sleep 1.5
echo "==> [5] Screenshot: vile-setup-08-timezone.png (America/Toronto)"
shot "$SHOTS/vile-setup-08-timezone.png"

echo "==> [6] Continue -> done step"
continue_at "$SHOTS/vile-setup-08-timezone.png"
sleep 1.5
echo "==> [6] Screenshot: vile-setup-09-done.png (summary)"
shot "$SHOTS/vile-setup-09-done.png"

echo "==> [6] Finish -> installing screen (dry-run)"
continue_at "$SHOTS/vile-setup-09-done.png"
sleep 1.5
echo "==> [6] Screenshot: vile-setup-10-installing.png"
shot "$SHOTS/vile-setup-10-installing.png"
echo "==> [6] Install (dry-run logs the exact commands)"
continue_at "$SHOTS/vile-setup-10-installing.png"
sleep 3
echo "==> [6] Screenshot: vile-setup-11-installing-done.png"
shot "$SHOTS/vile-setup-11-installing-done.png"
echo "==> [6] Open desktop -> wizard closes, bare desktop"
continue_at "$SHOTS/vile-setup-11-installing-done.png"
sleep 2
echo "==> [6] Screenshot: vile-setup-12-desktop.png (nebula wallpaper must be live)"
shot "$SHOTS/vile-setup-12-desktop.png"

echo "==> Files the wizard wrote (temp HOME $TEST_HOME):"
for f in .config/vile/setup-done .config/vile/display-name \
         .config/vile/wallpaper .config/vile/timezone; do
  echo "--- $f:"; cat "$TEST_HOME/$f" 2>&1 | head -5
done
for f in .config/vile/profile.png .face; do
  echo "--- $f:"; file "$TEST_HOME/$f" 2>&1
done

echo "==> [7] Super+L -> lock (must show 'Adnan' + AD avatar, not 'viletest')"
press_chord Super_L l
sleep 2
shot "$SHOTS/vile-setup-13-lock.png"
echo "==> [7] Unlock with the PAM password"
type_text "vile"
press_key -k Return
sleep 2
shot "$SHOTS/vile-setup-14-unlocked.png"

echo "==> Stopping compositor (run 1 done)"
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true

echo "================ RUN 2: marker exists = straight to desktop ================"
echo "==> Starting compositor again (same temp HOME, marker present)..."
start_compositor ""
sleep 4
if grep -q "first run — opening Setup wizard" "$CLOG"; then
  echo "FAIL: wizard spawned on second boot despite the marker!"
else
  echo "OK: no first-run spawn line on second boot"
fi
echo "==> Screenshot: vile-setup-15-second-boot.png (bare desktop, NO wizard)"
shot "$SHOTS/vile-setup-15-second-boot.png"
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true

echo "==> Shots:"
ls -lh "$SHOTS"/vile-setup-*.png
echo "==> Compositor log (setup lines):"
grep -E "Vile: (first run|wallpaper reloaded|lock|unlock)" "$CLOG" | head -20
