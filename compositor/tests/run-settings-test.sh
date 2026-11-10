#!/bin/bash
# Vile Settings — Profile Picture test (Settings first setting, editable)
#
# Usage: ./run-settings-test.sh
#   Runs the real Vile compositor headless, opens the real Settings
#   window (Python + GTK3, Wayland native), and captures REAL grim
#   screenshots:
#
#   vile-settings-01.png  Settings open, Profile Picture FIRST at the
#                         top, purple V placeholder avatar (default)
#   vile-settings-02.png  after changing the picture (via the SAME
#                         save function the picker calls), Settings
#                         shows the NEW picture (live refresh, no restart)
#   vile-settings-03.png  lock screen after the change — the lock avatar
#                         must be the NEW picture, not the purple V
#   vile-settings-03-unlocked.png (bonus) after PAM unlock — foot back
#
# The file-picker click path is tested separately in
# run-settings-picker-test.sh (it needs the button coordinates from
# vile-settings-01.png). The --set-picture path here uses the exact
# same set_profile_picture() the picker calls, so the save + lock
# sharing is proven either way; the picker test proves the click.
#
# All shots must be checked by eye before calling Settings done.
set -u
cd "$(dirname "$0")/.."

OUT1="$HOME/workspace/vile-os/screenshots/vile-settings-01.png"
OUT2="$HOME/workspace/vile-os/screenshots/vile-settings-02.png"
OUT3="$HOME/workspace/vile-os/screenshots/vile-settings-03.png"
OUT3U="$HOME/workspace/vile-os/screenshots/vile-settings-03-unlocked.png"
SETTINGS="$HOME/workspace/vile-os/settings/vile-settings.py"
TEST_AVATAR="/tmp/vile-test-avatar.jpg"
mkdir -p "$(dirname "$OUT1")"

export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="${VILE_WALLPAPER:-$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg}"
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
unset DISPLAY  # GTK must use Wayland, never X

export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if [ ! -x ./build/vile ]; then
  echo "Building Vile compositor..."
  meson setup build --prefix=/usr/local 2>&1 | tail -2 && ninja -C build 2>&1 | tail -2
fi

# Start from the DEFAULT avatar: no profile picture anywhere.
rm -f "$HOME/.config/vile/profile.png" "$HOME/.face"
[ -s "$TEST_AVATAR" ] || { echo "ERROR: test avatar missing: $TEST_AVATAR"; exit 1; }

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-settings-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

start_compositor() { # $1 = extra args, $2 = quit seconds
  export VILE_QUIT_AFTER_SEC="$2"
  export VILE_NO_SETUP=1  # Piece 7 Part B: these tests are about the desktop, not first-run
  export LC_ALL=C.UTF-8 # kills foot's "not a UTF-8 locale" warning in shots
  rm -f "$VILE_SOCKET_FILE"
  ./build/vile $1 > /tmp/vile-settings-compositor.log 2>&1 &
  VILE_PID=$!
  for i in $(seq 1 50); do
    [ -f "$VILE_SOCKET_FILE" ] && break
    sleep 0.1
  done
  if [ ! -f "$VILE_SOCKET_FILE" ]; then
    echo "ERROR: compositor did not start. Log:"
    cat /tmp/vile-settings-compositor.log
    kill $VILE_PID 2>/dev/null || true
    exit 1
  fi
  SOCKET="$(cat "$VILE_SOCKET_FILE")"
  echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
}

echo "==> [A] Starting compositor (unlocked, foot + Settings)..."
start_compositor "-s foot" 90
sleep 3
echo "==> [A] Opening Settings..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$SETTINGS" > /tmp/vile-settings-app.log 2>&1 &
SETTINGS_PID=$!
sleep 4
echo "==> [A] Screenshot: $OUT1 (Profile Picture first, V placeholder)"
shot "$OUT1"

echo "==> [B] Changing the picture via the shared save function"
echo "    (same set_profile_picture() the picker calls)"
python3 "$SETTINGS" --set-picture "$TEST_AVATAR"
sleep 3  # Settings live-refreshes once a second from the file
echo "==> [B] Screenshot: $OUT2 (Settings must show the NEW picture)"
shot "$OUT2"

echo "==> [B] Stopping Settings + compositor for the lock test"
kill $SETTINGS_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true

echo "==> [C] Restarting compositor LOCKED (foot behind, hidden)"
start_compositor "-l -s foot" 60
sleep 4
echo "==> [C] Screenshot: $OUT3 (lock avatar must be the NEW picture)"
shot "$OUT3"

echo "==> [C] Unlocking with the PAM password"
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "vile" 2>&1 | tee -a /tmp/vile-settings-grim.log
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype -k Return 2>&1 | tee -a /tmp/vile-settings-grim.log
sleep 2
echo "==> [C] Bonus shot after unlock: $OUT3U"
shot "$OUT3U"

wait $VILE_PID 2>/dev/null || true
echo "==> Profile file:"
ls -lh "$HOME/.config/vile/profile.png" "$HOME/.face" 2>&1
echo "==> Compositor log (profile lines):"
grep -E "Vile: (profile|lock|unlock)" /tmp/vile-settings-compositor.log | head -20
echo "==> Shots:"
ls -lh "$OUT1" "$OUT2" "$OUT3" "$OUT3U" 2>&1
