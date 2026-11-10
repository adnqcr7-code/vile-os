#!/bin/bash
# Vile — Settings wallpaper switching test (UI polish round)
#
# Usage: ./run-wallpaper-test.sh
#   Runs the real Vile compositor headless, opens the real Settings
#   window ALONE in the full tile (Python + GTK3, Wayland native),
#   and captures REAL grim screenshots:
#
#   vile-wallpaper-01.png  Settings open — the Wallpaper section
#                          lists the three wallpapers (Purple Arch,
#                          Nebula, Dunes) with thumbnails; Purple
#                          Arch is marked Current
#   vile-wallpaper-02.png  after CLICKING the Nebula row — Settings
#                          shows Nebula marked Current
#   vile-wallpaper-03.png  Settings closed (Super+Q) — the bare
#                          desktop with the NEBULA wallpaper actually
#                          on screen (live reload, no restart)
#
# The Nebula row click comes from the compositor log's Settings tile
# origin + a measured offset (NEBULA_DX / NEBULA_DY below, measured
# from vile-wallpaper-01.png); the fallback is the manually proven
# centre. The live reload is the 1-second hook watcher in main.c:
# Settings writes ~/.config/vile/wallpaper, the compositor stats it
# and swaps the wallpaper in one frame. The log must show
# "Vile: wallpaper reloaded live: ...nebula...".
#
# Starts from the DEFAULT wallpaper (hook file removed) and restores
# it at the end, so other harnesses are not affected.
#
# All proof shots live under screenshots/ — NEVER /tmp. Logs stay in
# /tmp; they are not proof. All shots must be checked by eye.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-wallpaper-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-wallpaper-02.png"
OUT3="$HOME/workspace/vile-os/screenshots/vile-wallpaper-03.png"
SETTINGS="$HOME/workspace/vile-os/settings/vile-settings.py"
LOG="/tmp/vile-wallpaper-compositor.log"
mkdir -p "$(dirname "$OUT1")"

# Nebula row centre, as an offset from the Settings tile origin.
# MEASURED from vile-look-settings.png (Settings alone, full tile
# 12,12 1896x932): row borders at y 663..665 / 820..821, centre 742.
NEBULA_DX=948
NEBULA_DY=730

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_QUIT_AFTER_SEC=90
export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
export LC_ALL=C.UTF-8
unset DISPLAY  # GTK must use Wayland, never X
rm -f "$VILE_SOCKET_FILE"
# NO VILE_WALLPAPER here on purpose: the env var wins over the hook
# file in vile_wallpaper_path(), so exporting it would make the live
# reload a no-op (the compositor would keep re-resolving purple arch
# and never see nebula). With it unset, the hook wins when Settings
# writes it. The hook is pinned to Purple Arch for this test: since
# Nov 10, 2026 the compositor picks a RANDOM default (purple Arch or
# nebula) when no hook exists, and this test needs a deterministic
# start (Purple Arch current) so the Nebula click is a real swap.
unset VILE_WALLPAPER
mkdir -p "$HOME/.config/vile"
echo "$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg" > "$HOME/.config/vile/wallpaper"

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-wallpaper-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}
click() { # $1 $2 = x y
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-wallpaper-grim.log
}
press_chord() { # vkey_test <keysym>...
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-wallpaper-grim.log
}

echo "==> Starting Vile compositor (unlocked, NO startup)..."
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

echo "==> Opening Settings (alone in the full tile)..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$SETTINGS" > /tmp/vile-wallpaper-app.log 2>&1 &
SETTINGS_PID=$!
sleep 4
echo "==> Shot 01: Settings with the wallpaper list..."
shot "$OUT1"

# Nebula row centre, from the Settings tile origin in the log.
TILE_XY="$(grep "Vile: tiling: window.*'Settings' at " "$LOG" 2>/dev/null | tail -1 | sed -E "s/.* at ([0-9]+),([0-9]+) [0-9]+x[0-9]+/\1 \2/")"
if [ -n "$TILE_XY" ]; then
  NEB_X=$(( ${TILE_XY%% *} + NEBULA_DX ))
  NEB_Y=$(( ${TILE_XY##* } + NEBULA_DY ))
  echo "    Settings tile origin: $TILE_XY -> Nebula row at ($NEB_X,$NEB_Y)"
else
  NEB_X=960; NEB_Y=742
  echo "    WARNING: Settings tile not in log, falling back to proven ($NEB_X,$NEB_Y)"
fi
echo "==> Clicking the Nebula wallpaper row ($NEB_X,$NEB_Y)..."
click "$NEB_X" "$NEB_Y"
sleep 3  # Settings writes the hook; the compositor reloads within a second
echo "==> Hook file now:"
cat "$HOME/.config/vile/wallpaper" 2>&1
grep -m1 'wallpaper reloaded live' "$LOG" || echo "WARN: no live-reload line in log yet"
echo "==> Shot 02: Settings with Nebula marked Current..."
shot "$OUT2"

echo "==> Closing Settings (Super+Q) — the desktop must show nebula..."
press_chord Super_L q
sleep 2
echo "==> Shot 03: bare desktop with the nebula wallpaper..."
shot "$OUT3"
grep -m1 'wallpaper reloaded live' "$LOG" || echo "WARN: no live-reload line in log"

echo "==> Waiting for compositor to quit (VILE_QUIT_AFTER_SEC=$VILE_QUIT_AFTER_SEC)..."
wait $VILE_PID 2>/dev/null
echo "==> Restoring the default wallpaper for other harnesses..."
rm -f "$HOME/.config/vile/wallpaper"
echo "==> Compositor exited. Log tail:"
tail -5 "$LOG"
echo "==> Shots: $OUT1 $OUT2 $OUT3"
ls -lh "$OUT1" "$OUT2" "$OUT3"
