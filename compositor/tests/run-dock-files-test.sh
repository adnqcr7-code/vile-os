#!/bin/bash
# Vile dock — Files icon test
#
# Usage: ./run-dock-files-test.sh
#   Runs the real Vile compositor headless (no startup command) and
#   captures REAL grim screenshots proving the fourth dock icon:
#
#   vile-dock-files-01.png  bare desktop — 4-icon dock (foot,
#                     Settings, ScreenFit, Files) centred above the
#                     bottom bar, pill 296x80 at (812,956)
#   vile-dock-files-02.png  Files open from a REAL click on the dock
#                     Files icon — tiled, purple running dot under
#                     the Files icon, log: "Vile: dock: open Files"
#   vile-dock-files-03-hover.png (bonus)
#                     pointer moved OFF the dock (bar) so the icon
#                     shrinks back, then ONTO the Files icon: the
#                     icon grows (hover 1.14x) while its neighbours
#                     stay 56px — Piece 7 hover machinery on the new
#                     icon. The off/on pair is what proves it: a
#                     shot taken with the pointer already on the icon
#                     (e.g. right after the dock click) is grown in
#                     both frames and proves nothing.
#   vile-dock-files-04-1280x800.png (bonus)
#                     bare desktop at 1280x800 — 4-icon dock centred,
#                     no overlap, no clipping
#
# Dock icon centres come from the compositor log ("shell created:
# ... files centre (x,y) ..."); the hardcoded values are fallbacks
# only. All proof shots live under screenshots/ — NEVER /tmp.
# The Files window is identified by client cmdline (vile-files.py),
# not its title — the title is the folder name.
set -u
cd "$(dirname "$0")/.."

SHOTS="$HOME/workspace/vile-os/screenshots"
OUT1="$SHOTS/vile-dock-files-01.png"
OUT2="$SHOTS/vile-dock-files-02.png"
OUT3="$SHOTS/vile-dock-files-03-hover.png"
LOG="/tmp/vile-dock-files-compositor.log"
mkdir -p "$SHOTS"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-dock-files-socket.txt"
export VILE_QUIT_AFTER_SEC=60
export VILE_NO_SETUP=1  # these tests are about the dock, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"  # default wallpaper for these shots

mkdir -p "$HOME/.config/foot"
cp "$HOME/workspace/vile-os/foot.ini" "$HOME/.config/foot/foot.ini"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

echo "==> Starting Vile compositor (bare desktop)..."
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
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-dock-files-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-dock-files-grim.log
}
move_to() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" move 2>&1 | tee -a /tmp/vile-dock-files-grim.log
}

# Dock Files icon centre, from the compositor log (fallback: design coords).
sleep 1
DOCK_LINE="$(grep -m1 'dock foot icon centre' "$LOG" || true)"
FILES_X="$(echo "$DOCK_LINE" | sed -n 's/.*files centre (\([0-9]*\),\([0-9]*\)).*/\1/p')"
FILES_Y="$(echo "$DOCK_LINE" | sed -n 's/.*files centre (\([0-9]*\),\([0-9]*\)).*/\2/p')"
FILES_X="${FILES_X:-1068}"
FILES_Y="${FILES_Y:-996}"
echo "==> Dock files icon centre: ($FILES_X,$FILES_Y) [log: $DOCK_LINE]"
grep -m1 'dock files icon loaded' "$LOG" || echo "WARN: files icon asset not loaded (fallback glyph)"

echo "==> Shot 01: bare desktop, 4-icon dock..."
shot "$OUT1"
python3 - "$OUT1" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
# Dock pill 296x80 at (812,956): pill fill is translucent dark over the
# wallpaper, so check the icon cells are NOT wallpaper: the foot tile
# centre (852,996) is the logo tile, the Files tile centre (1068,996)
# is the files-icon tile (dark tile + purple folder). The pill row
# y=960..1030 must differ from the wallpaper band above it.
def col(x, y):
    return im.getpixel((x, y))
print('dock-01: foot icon centre (852,996):', col(852, 996))
print('dock-01: settings centre (924,996):', col(924, 996))
print('dock-01: screenfit centre (996,996):', col(996, 996))
print('dock-01: files icon centre (1068,996):', col(1068, 996))
print('dock-01: pill left edge (812,996):', col(812, 996))
print('dock-01: pill right edge (1107,996):', col(1107, 996))
print('dock-01: just left of pill (800,996, wallpaper):', col(800, 996))
print('dock-01: just right of pill (1120,996, wallpaper):', col(1120, 996))
EOF

echo "==> Click dock Files icon ($FILES_X,$FILES_Y) -> Files opens..."
click "$FILES_X" "$FILES_Y"
sleep 3
grep -E "dock: open Files|window mapped" "$LOG" | tail -3
echo "==> Shot 02: Files tiled, running dot under Files icon..."
shot "$OUT2"
python3 - "$OUT2" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
def col(x, y):
    return im.getpixel((x, y))
# Running dot under the Files icon: centre (1068,1029), purple
# (110,74,255) core with a halo. Foot/Settings/ScreenFit have no dot.
print('dock-02: files running dot (1068,1029):', col(1068, 1029))
print('dock-02: files dot halo (1068,1024):', col(1068, 1024))
print('dock-02: foot dot spot (852,1029, expect no dot):', col(852, 1029))
print('dock-02: files tile headerbar (960,16):', col(960, 16))
EOF

echo "==> Move pointer OFF the dock (bottom bar) so the icon shrinks back..."
move_to 200 1064
sleep 1.5
shot "$SHOTS/vile-dock-files-03-ungrown.png"
echo "==> Hover the Files icon (settled grow)..."
move_to "$FILES_X" "$FILES_Y"
sleep 2
shot "$OUT3"
python3 - "$SHOTS/vile-dock-files-03-ungrown.png" "$OUT3" <<'EOF'
import sys
from PIL import Image
def purple_bbox(path):
    im = Image.open(path).convert('RGB')
    pts = []
    for y in range(950, 1020):  # above the running dot (y~1029)
        for x in range(1020, 1120):
            r, g, b = im.getpixel((x, y))
            if b > 150 and 60 < r < 170 and g < 130:
                pts.append((x, y))
    xs = [p[0] for p in pts]; ys = [p[1] for p in pts]
    return (min(xs), max(xs), min(ys), max(ys), len(pts))
print('dock-03-ungrown: files purple bbox (x0,x1,y0,y1,n):', purple_bbox(sys.argv[1]))
print('dock-03-hover:   files purple bbox (x0,x1,y0,y1,n):', purple_bbox(sys.argv[2]))
print('dock-03: expect grown width ~1.14x ungrown (35 -> ~40) and more purple px')
EOF

echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Compositor exited. Log tail:"
tail -5 "$LOG"
echo "==> Shots: $OUT1 $OUT2 $SHOTS/vile-dock-files-03-ungrown.png $OUT3"
ls -lh "$OUT1" "$OUT2" "$SHOTS/vile-dock-files-03-ungrown.png" "$OUT3"
