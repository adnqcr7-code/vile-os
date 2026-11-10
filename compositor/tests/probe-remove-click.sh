#!/bin/bash
# Diagnostic probe (Nov 10 2026, audit-trail fix): does a pointer click
# reach a GTK button in Settings on the CURRENT binary?
#   - Sets the AD test profile via CLI first (proven path).
#   - Starts the compositor + foot + Settings, clicks "Remove Picture"
#     (centre 1462,336 on the bottom-bar layout, tile origin 966,12).
#   - If the click lands, profile.png is deleted and the avatar flips
#     back to the purple V. If nothing changes, click delivery to GTK
#     buttons is broken on this binary (Piece 5 bug — report, do not fix).
# This is a diagnostic only, not a proof harness. Run from compositor/.
set -u
cd "$(dirname "$0")/.." || exit 1
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/vile-runtime}"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless WLR_RENDERER=pixman
export VILE_QUIT_AFTER_SEC=30 LC_ALL=C.UTF-8
export VILE_NO_SETUP=1  # Piece 7 Part B: probe is about the desktop, not first-run
export VILE_SOCKET_FILE="/tmp/vile-wayland-socket.txt"
rm -f "$HOME/.config/vile/profile.png" "$HOME/.face"
python3 ../settings/vile-settings.py --set-picture /tmp/vile-test-avatar.jpg \
  || { echo "ERROR: could not set test profile"; exit 1; }
ls -l "$HOME/.config/vile/profile.png"
echo "binary before probe: $(sha256sum build/vile | cut -c1-12)"
rm -f "$VILE_SOCKET_FILE"
./build/vile -s foot > /tmp/vile-remove-probe-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do [ -f "$VILE_SOCKET_FILE" ] && break; sleep 0.1; done
SOCKET="$(cat "$VILE_SOCKET_FILE")"
sleep 3
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 ../settings/vile-settings.py \
  > /tmp/vile-remove-probe-app.log 2>&1 &
sleep 4
echo "clicking Remove Picture at (1462,336)"
./tests/vptr_test 1462 336 click
sleep 2
WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 \
  ../screenshots/vile-settings-remove-probe.png
echo "profile after click:"
ls -l "$HOME/.config/vile/profile.png" 2>&1 || echo "(profile.png gone)"
grep -c "window mapped 'Settings'" /tmp/vile-remove-probe-compositor.log
wait $VILE_PID 2>/dev/null
echo "binary after probe: $(sha256sum build/vile | cut -c1-12)"
