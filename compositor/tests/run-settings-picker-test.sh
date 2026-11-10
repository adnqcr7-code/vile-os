#!/bin/bash
# Vile Settings — file-picker click test (the REAL picker, not --set-picture)
#
# Usage: ./run-settings-picker-test.sh
#   Opens Settings, CLICKS the "Change Picture…" button with the
#   virtual pointer, then uses the keyboard in the GTK file chooser
#   (Ctrl+L for the location bar, type the path, Enter) to pick
#   /tmp/vile-test-avatar.jpg.
#
#   screenshots/vile-settings-picker-open.png  the chooser dialog open
#   vile-settings-picker.png              Settings after picking — the
#                                         avatar must be the NEW picture
#
# Coords are LAYOUT-AWARE (Piece 4 tiling, Piece 5 shell): the click
# points are computed from the compositor log's tile lines, not
# hardcoded to one layout. Settings tiles as window 2 (right half,
# 966,12 942x932 at 1920x1080 on the bottom-bar binary — bar 32px +
# dock reserve space); the "Change Picture…" button centre sits at
# a fixed offset (+326,+324) from the Settings tile origin — measured
# Nov 10 2026 from vile-settings-picker-open.png (button rect
# x 1218..1367, y 315..358, centre 1292,336 on the bottom-bar layout;
# it was y 347..390, centre 1292,368 on the old top-bar layout).
# The old Piece 4 offset (+302,+302) MISSED the button by 1px on the
# Piece 5 layout (click landed on white at y=346, button top edge
# y=347) — that is why the chooser never opened on the 05:21 UTC
# run. The chooser dialog tiles as window 3 (right-bottom, 966,484
# 942x460); its "Open" button centre sits at (+905,+30) from the
# dialog tile origin (GTK headerbar, unchanged from Piece 4). If the
# log lookup fails, the harness falls back to the manually proven
# coords Change (1292,336), Open (1871,514) and says so.
#
# If the click or the chooser navigation fails, the dialog shot shows
# exactly where it stopped — report that honestly, do not claim the
# picker works. The --set-picture path in run-settings-test.sh is the
# fallback proof for save + lock sharing (same save function).
set -u
cd "$(dirname "$0")/.."

OUTD="$HOME/workspace/vile-os/screenshots/vile-settings-picker-open.png"
OUTP="$HOME/workspace/vile-os/screenshots/vile-settings-picker.png"
SETTINGS="$HOME/workspace/vile-os/settings/vile-settings.py"
TEST_AVATAR="/tmp/vile-test-avatar.jpg"
mkdir -p "$(dirname "$OUTP")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=60
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export LC_ALL=C.UTF-8
unset DISPLAY

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"

# Start from the DEFAULT avatar so the picker result is visible.
rm -f "$HOME/.config/vile/profile.png" "$HOME/.face"
[ -s "$TEST_AVATAR" ] || { echo "ERROR: test avatar missing: $TEST_AVATAR"; exit 1; }

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-settings-picker.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

# tile_origin <xdg-title> — prints "X Y" of the last tile logged for
# that window ("Vile: tiling: window N/M '<title>' at X,Y WxH"), or
# nothing if the window never tiled (caller falls back to proven coords).
tile_origin() {
  grep "Vile: tiling: window.*'$1' at " /tmp/vile-settings-picker-compositor.log 2>/dev/null \
    | tail -1 | sed -E "s/.* at ([0-9]+),([0-9]+) [0-9]+x[0-9]+/\1 \2/"
}

echo "==> Starting compositor (unlocked, foot + Settings)..."
rm -f "$VILE_SOCKET_FILE"
./build/vile -s foot > /tmp/vile-settings-picker-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do
  [ -f "$VILE_SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$VILE_SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start."; cat /tmp/vile-settings-picker-compositor.log; exit 1
fi
SOCKET="$(cat "$VILE_SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET"
sleep 3
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$SETTINGS" > /tmp/vile-settings-picker-app.log 2>&1 &
SETTINGS_PID=$!
sleep 4

echo "==> Clicking 'Change Picture…' (Settings tile origin + 326,324 = button centre)"
CHANGE_XY="$(tile_origin "Settings")"
if [ -n "$CHANGE_XY" ]; then
  CHANGE_X=$(( ${CHANGE_XY%% *} + 326 ))
  CHANGE_Y=$(( ${CHANGE_XY##* } + 324 ))
  echo "    Settings tile origin: $CHANGE_XY -> Change at ($CHANGE_X,$CHANGE_Y)"
else
  CHANGE_X=1292; CHANGE_Y=336
  echo "    WARNING: Settings tile not in log, falling back to proven ($CHANGE_X,$CHANGE_Y)"
fi
./tests/vptr_test "$CHANGE_X" "$CHANGE_Y" click 2>&1 | tee -a /tmp/vile-settings-picker.log
sleep 2
echo "==> Dialog shot: $OUTD"
shot "$OUTD"

echo "==> Ctrl+L (location bar), type the path, Enter (select it)"
./tests/vkey_test Control_L l 2>&1 | tee -a /tmp/vile-settings-picker.log
sleep 1
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "/tmp/vile-test-avatar.jpg" 2>&1 | tee -a /tmp/vile-settings-picker.log
sleep 1
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype -k Return 2>&1 | tee -a /tmp/vile-settings-picker.log
sleep 1.5
echo "==> Clicking the dialog 'Open' button (dialog tile origin + 905,30 = button centre)"
echo "    (a second Enter in the location entry did not confirm —"
echo "     the dialog stayed open with the path still in the bar)"
OPEN_XY="$(tile_origin "Choose profile picture")"
if [ -n "$OPEN_XY" ]; then
  OPEN_X=$(( ${OPEN_XY%% *} + 905 ))
  OPEN_Y=$(( ${OPEN_XY##* } + 30 ))
  echo "    Dialog tile origin: $OPEN_XY -> Open at ($OPEN_X,$OPEN_Y)"
else
  OPEN_X=1871; OPEN_Y=514
  echo "    WARNING: dialog tile not in log, falling back to proven ($OPEN_X,$OPEN_Y)"
fi
./tests/vptr_test "$OPEN_X" "$OPEN_Y" click 2>&1 | tee -a /tmp/vile-settings-picker.log
sleep 3
echo "==> Result shot: $OUTP (avatar must be the NEW picture)"
shot "$OUTP"

kill $SETTINGS_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
echo "==> Profile file:"
ls -lh "$HOME/.config/vile/profile.png" 2>&1
echo "==> Settings app log tail:"
tail -5 /tmp/vile-settings-picker-app.log
echo "==> Shots: $OUTD $OUTP"
