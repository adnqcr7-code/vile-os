#!/bin/bash
# Vile Piece 5 — look test (bottom bar, dock — the huge desktop
# clock was removed Nov 10, 2026; the bar centre carries the small
# clock now)
#
# Usage: ./run-look-test.sh
#   Runs the real Vile compositor headless (no startup command), drives
#   it with vptr_test (pointer clicks) and vkey_test (key chords), and
#   captures REAL grim screenshots:
#
#   vile-look-01.png  desktop with NO windows — bottom bar + dock
#                     above it over the purple wallpaper (the poster
#                     frame: must clear the edit-worthy bar)
#   vile-look-02.png  one tiled foot window open (via a real click on
#                     the dock's foot icon) — bottom bar + dock still
#                     visible, foot's title bar DARK purple (foot.ini,
#                     not white)
#   vile-look-03.png  dock interaction proven — foot closed with
#                     Super+Q, then re-opened by clicking the dock's
#                     foot icon again (log: "Vile: dock: open foot"),
#                     with `echo reopened` typed in the reopened foot
#                     so the shot is NOT byte-identical to look-02
#                     (audit #2 must-fix #5 — the old look-03 was)
#   vile-look-menu.png (bonus)
#                     click the bottom-bar status cluster -> the one
#                     system menu opens UPWARDS from the bar (Wi-Fi /
#                     Volume / Battery / Lock / Settings / Power Off)
#   vile-look-settings.png (bonus)
#                     click the menu's Settings row -> Settings opens
#                     (tiled next to foot)
#   vile-look-settings-alone.png (bonus — judge item 2)
#                     foot closed, Settings alone in the full tile:
#                     re-proves the settings-04 placement question
#                     (headerbar must start at the tile's y, with
#                     4px borders — not at y=0)
#
# All proof shots live under screenshots/ — NEVER /tmp (audit #1 set
# this rule; this harness broke it for the three bonus shots and
# audit #2 flagged it again). Logs stay in /tmp; they are not proof.
#
# Dock/menu click coords come from the compositor log, not hardcoded
# geometry ("shell created: ... dock foot icon centre (x,y) ..." and
# "shell menu opened: ... Settings (x,y wxh) ..."); the hardcoded
# values are fallbacks only.
#
# All shots must be checked by eye before calling Piece 5 done.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-look-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-look-02.png"
OUT3="$HOME/workspace/vile-os/screenshots/vile-look-03.png"
OUTMENU="$HOME/workspace/vile-os/screenshots/vile-look-menu.png"
OUTSET="$HOME/workspace/vile-os/screenshots/vile-look-settings.png"
OUTALONE="$HOME/workspace/vile-os/screenshots/vile-look-settings-alone.png"
LOG="/tmp/vile-look-compositor.log"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=150
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"
rm -f "$HOME/.config/vile/wallpaper"  # default wallpaper for these shots

# Foot's dark title bar (Piece 5 judge fix): foot reads
# ~/.config/foot/foot.ini. Install the shipped theme so every foot
# launched here — startup, keybind, or dock click — gets it.
mkdir -p "$HOME/.config/foot"
cp "$HOME/workspace/vile-os/foot.ini" "$HOME/.config/foot/foot.ini"

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
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-look-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-look-grim.log
}
press_chord() { # vkey_test <keysym>...
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-look-grim.log
}
type_text() { # $1 = text to type (no Enter)
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-look-grim.log
}
press_key() { # wtype -k args — plain keys only, NEVER chords
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$@" 2>&1 | tee -a /tmp/vile-look-grim.log
}

# Dock foot icon centre, from the compositor log (fallback: design coords).
sleep 1
DOCK_LINE="$(grep -m1 'dock foot icon centre' "$LOG" || true)"
DOCK_FOOT_X="$(echo "$DOCK_LINE" | sed -n 's/.*dock foot icon centre (\([0-9]*\),\([0-9]*\)).*/\1/p')"
DOCK_FOOT_Y="$(echo "$DOCK_LINE" | sed -n 's/.*dock foot icon centre (\([0-9]*\),\([0-9]*\)).*/\2/p')"
DOCK_FOOT_X="${DOCK_FOOT_X:-888}"
DOCK_FOOT_Y="${DOCK_FOOT_Y:-996}"
echo "==> Dock foot icon centre: ($DOCK_FOOT_X,$DOCK_FOOT_Y) [log: $DOCK_LINE]"

echo "==> Shot 01: bare desktop (no windows)..."
shot "$OUT1"

echo "==> Click dock foot icon ($DOCK_FOOT_X,$DOCK_FOOT_Y) -> foot opens..."
click "$DOCK_FOOT_X" "$DOCK_FOOT_Y"
sleep 3
grep -E "dock: open foot|window mapped 'foot'" "$LOG" | tail -3
echo "==> Shot 02: one tiled foot (dark title bar)..."
shot "$OUT2"

echo "==> Super+Q closes foot; click dock foot icon again..."
press_chord Super_L q
sleep 2
click "$DOCK_FOOT_X" "$DOCK_FOOT_Y"
sleep 3
grep -E "dock: open foot|window mapped 'foot'" "$LOG" | tail -3
echo "==> Type 'echo reopened' in the reopened foot (look-03 must differ from look-02)..."
click 960 500   # inside the tiled foot (12,12 1896x924) — focus it
sleep 1
type_text "echo reopened"
press_key -k Return
sleep 1.5
echo "==> Shot 03: dock re-open proven (with 'echo reopened' visible)..."
shot "$OUT3"

echo "==> Bonus: click status cluster (1880,1064) -> system menu..."
click 1880 1064
sleep 1
grep -m1 'shell menu opened' "$LOG" || echo "WARN: no menu-opened line in log"
shot "$OUTMENU"

# Settings row centre, from the menu-opened log line (fallback: design).
MENU_LINE="$(grep -m1 'shell menu opened' "$LOG" || true)"
SET_RECT="$(echo "$MENU_LINE" | sed -n 's/.*Settings (\([0-9]*\),\([0-9]*\) \([0-9]*\)x\([0-9]*\)).*/\1 \2 \3 \4/p')"
if [ -n "$SET_RECT" ]; then
  SET_X="$(echo "$SET_RECT" | cut -d' ' -f1)"
  SET_Y="$(echo "$SET_RECT" | cut -d' ' -f2)"
  SET_W="$(echo "$SET_RECT" | cut -d' ' -f3)"
  SET_H="$(echo "$SET_RECT" | cut -d' ' -f4)"
  SET_CX=$((SET_X + SET_W / 2))
  SET_CY=$((SET_Y + SET_H / 2))
else
  SET_CX=1770; SET_CY=968
  echo "WARN: menu Settings rect not in log, fallback ($SET_CX,$SET_CY)"
fi
echo "==> Click menu Settings row ($SET_CX,$SET_CY)..."
click "$SET_CX" "$SET_CY"
sleep 3
grep -E "shell menu: Settings clicked|shell: opening Settings" "$LOG" | tail -2
shot "$OUTSET"

echo "==> Bonus (judge item 2): close foot, Settings alone in full tile..."
press_chord Super_L j   # focus previous tile (foot)
sleep 1
press_chord Super_L q   # close foot, Settings re-tiles to the full tile
sleep 2
shot "$OUTALONE"
python3 - "$OUTALONE" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert('RGB')
# Bottom-bar layout: full tile is (12,12,1896,924). The 4px border
# sits OUTSIDE the tile (y=8..11 on top, x=8..11 on the left); the
# headerbar starts at the tile's own y=12. Wallpaper visible at y<8.
# Border pixels read blended purple (~(93,63,216) top, ~(86,58,200)
# left), not pure (110,74,255) — the border is drawn semi-transparent
# over the wallpaper; that is the render, not a failure.
def col(x, y):
    return im.getpixel((x, y))
print('settings-alone: y=6 x=960 (wallpaper gap, expect ~(43,45,58)):', col(960, 6))
print('settings-alone: y=9 x=960 (border, expect blended purple ~(93,63,216)):', col(960, 9))
print('settings-alone: y=16 x=960 (headerbar, expect (36,22,64)):', col(960, 16))
print('settings-alone: y=100 x=5 (wallpaper strip, expect ~(43,45,58)):', col(5, 100))
print('settings-alone: y=100 x=9 (border, expect ~(110,74,255)):', col(9, 100))
print('settings-alone: y=100 x=12 (headerbar/content edge):', col(12, 100))
# first y where the headerbar colour appears at x=960
for y in range(0, 80):
    if col(960, y) == (36, 22, 64):
        print('settings-alone: headerbar first appears at y =', y)
        break
EOF

echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Compositor exited. Log tail:"
tail -5 "$LOG"
echo "==> Shots: $OUT1 $OUT2 $OUT3 $OUTMENU $OUTSET $OUTALONE"
ls -lh "$OUT1" "$OUT2" "$OUT3" "$OUTMENU" "$OUTSET" "$OUTALONE"
