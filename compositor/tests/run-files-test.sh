#!/bin/bash
# Vile Files — test harness (v1)
#
# Usage: ./run-files-test.sh
#   Runs the real Vile compositor headless (snapshot binary, so a
#   Piece 7 rebuild mid-test cannot break the run), opens the real
#   Files window (Python + GTK3, Wayland native), and captures REAL
#   grim screenshots:
#
#   vile-files-01.png  real home listing, grid view
#   vile-files-02.png  inside Projects (double-clicked in from the
#                      scratch root) — path bar must show it
#   vile-files-03.png  search filter active ("note" typed)
#   vile-files-04-dialog.png  New Folder dialog open
#   vile-files-04.png  the new folder actually created in the listing
#   vile-files-05.png  list view
#
# Creation/deletion tests run ONLY in the scratch tree
# /tmp/vile-files-test — never the real home.
#
# Click coordinates are measured from the probe shots (see BUILD_LOG
# File Manager section); the double-click is two fast vptr clicks,
# which GTK sees as one double-click.
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
APP="$HOME/workspace/vile-os/filemanager/vile-files.py"
SCRATCH="/tmp/vile-files-test"
mkdir -p "$SHOTS"

# Reset the scratch tree every run so the New Folder step creates
# Drafts fresh — a previous run's Drafts trips the duplicate-name
# guard instead (that guard itself is proven by the status message
# in the 14:59 run's vile-files-04.png).
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH/Projects" "$SCRATCH/Photos"
printf 'Vile Files test note.\nSecond line here.\n' > "$SCRATCH/Projects/notes.txt"
printf -- '- ship the file manager\n- test the trash\n' > "$SCRATCH/Projects/todo.md"
printf 'name,size\nnotes,128\ntodo,64\n' > "$SCRATCH/Projects/data.csv"
printf 'Vile Files — scratch root readme.\n' > "$SCRATCH/readme.txt"
printf 'item,cost\nkeyboard,49\nmouse,29\n' > "$SCRATCH/budget.csv"
printf 'PK\x03\x04dummy zip contents\n' > "$SCRATCH/archive.zip"
printf 'hidden on purpose\n' > "$SCRATCH/.hidden-note.txt"
python3 -c "
from PIL import Image, ImageDraw
im = Image.new('RGB', (320, 200), (109, 74, 255))
d = ImageDraw.Draw(im)
d.ellipse([90, 40, 230, 180], fill=(255, 190, 80))
im.save('$SCRATCH/Photos/sunset.png')"

export XDG_RUNTIME_DIR="/tmp/vile-files-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg"
export VILE_SOCKET_FILE="/tmp/vile-files-socket.txt"
export VILE_QUIT_AFTER_SEC=240
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export LC_ALL=C.UTF-8
unset DISPLAY

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
VILE_BIN="/tmp/vile-files-vile"
[ -x "$VILE_BIN" ] || cp ./build/vile "$VILE_BIN"

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-files-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

start_compositor() {
  rm -f "$VILE_SOCKET_FILE"
  "$VILE_BIN" > /tmp/vile-files-compositor.log 2>&1 &
  VILE_PID=$!
  for i in $(seq 1 50); do
    [ -f "$VILE_SOCKET_FILE" ] && break
    sleep 0.1
  done
  if [ ! -f "$VILE_SOCKET_FILE" ]; then
    echo "ERROR: compositor did not start. Log:"
    cat /tmp/vile-files-compositor.log
    kill $VILE_PID 2>/dev/null || true
    exit 1
  fi
  SOCKET="$(cat "$VILE_SOCKET_FILE")"
  echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
}

launch_app() { # $1 = extra args
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    GDK_BACKEND=wayland python3 "$APP" $1 > /tmp/vile-files-app.log 2>&1 &
  APP_PID=$!
  sleep 4
}

stop_app() {
  kill $APP_PID 2>/dev/null || true
  sleep 1
}

# Click coords, measured from the probe shots (single tiled window
# 1896x932 at (12,12); see BUILD_LOG File Manager section).
# Projects grid cell centre in the scratch-root view:
PROJ_X=${PROJ_X:-536}
PROJ_Y=${PROJ_Y:-176}
# List/Grid toggle button centre (headerbar, right side):
VIEW_X=${VIEW_X:-1705}
VIEW_Y=${VIEW_Y:-36}
# Search-entry clear (x) icon centre — clears the live filter:
CLEAR_X=${CLEAR_X:-1875}
CLEAR_Y=${CLEAR_Y:-92}
# New Folder dialog OK button centre (dialog tiles as window 2,
# right half 966,12 942x932; OK at the bottom-right of the dialog):
OK_X=${OK_X:-1851}
OK_Y=${OK_Y:-910}

echo "==> [A] Starting compositor (bare desktop, no startup foot)..."
start_compositor
sleep 3

echo "==> [A] Opening Files at the real home..."
launch_app ""
echo "==> Shot 1: $SHOTS/vile-files-01.png (home, grid view)"
shot "$SHOTS/vile-files-01.png"
stop_app

echo "==> [B] Opening Files at the scratch root..."
launch_app "--path $SCRATCH"
sleep 1
echo "==> [B] Double-clicking Projects ($PROJ_X,$PROJ_Y)..."
./tests/vptr_test "$PROJ_X" "$PROJ_Y" click 2>&1 | tee -a /tmp/vile-files-grim.log
./tests/vptr_test "$PROJ_X" "$PROJ_Y" click 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 2
echo "==> Shot 2: $SHOTS/vile-files-02.png (inside Projects)"
shot "$SHOTS/vile-files-02.png"

echo "==> [C] Ctrl+F, type 'note' (search filter)..."
./tests/vkey_test Control_L f 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 1
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "note" 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 2
echo "==> Shot 3: $SHOTS/vile-files-03.png (filtered listing)"
shot "$SHOTS/vile-files-03.png"

echo "==> [C] Clicking the search clear (x) icon ($CLEAR_X,$CLEAR_Y)..."
# wtype -k Delete does not reliably clear a GTK entry here (same
# quirk the picker test hit with Return) — the entry's own clear
# button is the real user action.
./tests/vptr_test "$CLEAR_X" "$CLEAR_Y" click 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 1.5

echo "==> [D] Ctrl+Shift+N (New Folder dialog)..."
./tests/vkey_test Control_L Shift_L n 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 2
echo "==> Shot 4-dialog: $SHOTS/vile-files-04-dialog.png"
shot "$SHOTS/vile-files-04-dialog.png"
echo "==> [D] Typing 'Drafts', clicking OK ($OK_X,$OK_Y)..."
# wtype -k Return does not confirm GTK dialogs in this harness
# (the picker test hit the same quirk) — click the real OK button.
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "Drafts" 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 0.5
./tests/vptr_test "$OK_X" "$OK_Y" click 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 2
echo "==> Shot 4: $SHOTS/vile-files-04.png (Drafts must be listed)"
shot "$SHOTS/vile-files-04.png"
ls -ld "$SCRATCH/Projects/Drafts" 2>&1 || { echo "FAIL: Drafts was not created"; exit 1; }

echo "==> [E] Clicking the List/Grid toggle ($VIEW_X,$VIEW_Y)..."
./tests/vptr_test "$VIEW_X" "$VIEW_Y" click 2>&1 | tee -a /tmp/vile-files-grim.log
sleep 2
echo "==> Shot 5: $SHOTS/vile-files-05.png (list view)"
shot "$SHOTS/vile-files-05.png"

stop_app
wait $VILE_PID 2>/dev/null || true
echo "==> App log tail:"
tail -5 /tmp/vile-files-app.log
echo "==> Scratch tree now:"
find "$SCRATCH" | sort
echo "==> Shots:"
ls -lh "$SHOTS"/vile-files-0*.png 2>&1
