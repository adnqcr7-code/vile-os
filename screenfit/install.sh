#!/bin/sh
# ScreenFit - user install script (no root needed).
# Installs the app to ~/.local/share/screenfit/, a launcher to
# ~/.local/bin/screenfit, and a menu entry to
# ~/.local/share/applications/. Run:  ./install.sh
set -e

REPO_DIR="$(dirname "$(readlink -f "$0")")"
APP_DIR="$HOME/.local/share/screenfit"
BIN_DIR="$HOME/.local/bin"
DESKTOP_DIR="$HOME/.local/share/applications"

echo "ScreenFit - checking dependencies..."

fail=0
command -v python3 >/dev/null 2>&1 || { echo "MISSING: python3"; fail=1; }
command -v xrandr >/dev/null 2>&1 || { echo "MISSING: xrandr (sudo pacman -S xorg-xrandr)"; fail=1; }
python3 -c "import gi; gi.require_version('Gtk', '3.0'); from gi.repository import Gtk" 2>/dev/null \
    || { echo "MISSING: python-gobject / gtk3 (sudo pacman -S python-gobject gtk3)"; fail=1; }
python3 -c "import cairo" 2>/dev/null \
    || echo "NOTE: python-cairo not found - the preview gradient will use a solid colour instead (sudo pacman -S python-cairo)"

if [ "$fail" -ne 0 ]; then
    echo "Install the missing packages first, then re-run ./install.sh"
    exit 1
fi

echo "Installing to $APP_DIR ..."
mkdir -p "$APP_DIR" "$BIN_DIR" "$DESKTOP_DIR"
cp "$REPO_DIR/src/screenfit.py" "$REPO_DIR/src/xrandr.py" "$APP_DIR/"
cp "$REPO_DIR/src/screenfit" "$BIN_DIR/screenfit"
cp "$REPO_DIR/src/screenfit.desktop" "$DESKTOP_DIR/screenfit.desktop"
chmod +x "$APP_DIR/screenfit.py" "$BIN_DIR/screenfit"

echo "Done."
echo "Run it with:  screenfit"
echo "  (make sure ~/.local/bin is on your PATH; if the command is not"
echo "   found, log out and back in, or run:  export PATH=\"\$HOME/.local/bin:\$PATH\")"
echo "It should also appear in the Cinnamon menu as 'ScreenFit'."
