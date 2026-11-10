#!/bin/bash
# Vile OS — install on Arch Linux (Dell Latitude 7400, user adnan).
#
# Run FROM the cloned repo:
#   git clone https://github.com/adnqcr7-code/vile-os.git ~/vile-os
#   cd ~/vile-os
#   ./vile-install.sh
#
# What it does:
#   1. Installs the Arch packages (build tools, wlroots build deps, apps).
#   2. Builds wlroots 0.19.3 from source into /usr/local — Arch dropped the
#      unversioned `wlroots` package in Nov 2026 (only wlroots 0.20 remains)
#      and the compositor needs the 0.19 API.
#   3. Builds the Vile compositor.
#   4. Installs the `vile` launcher, app launchers, foot config, PAM
#      service, desktop files, and ScreenFit.
#
# Takes ~15-20 minutes, mostly the wlroots build. When it finishes, switch
# to a text console (Ctrl+Alt+F2 — on the Dell Latitude 7400 that's
# Ctrl+Alt+Fn+F2), log in, and run:  vile
set -e
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "== Vile OS install =="
echo "Repo: $REPO_DIR"
if [ ! -f /etc/arch-release ]; then
  echo "WARNING: this does not look like Arch Linux — continuing anyway."
fi

# wlroots 0.19.3 lives in /usr/local, and Arch's pkgconf does NOT search
# /usr/local/lib/pkgconfig by default (nor is /usr/local/lib in the
# default ld path). Both need to be set for the build AND at runtime.
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "== 1/5 Arch packages =="
sudo pacman -Syu --needed --noconfirm \
  base-devel meson ninja pkgconf git curl \
  wayland wayland-protocols libinput libxkbcommon pixman seatd libdrm mesa \
  libdisplay-info xorg-xrandr \
  gtk3 python-gobject python-pillow python-cairo foot xdg-utils

echo "== 2/5 wlroots 0.19.3 (from source) =="
if pkg-config --exists wlroots-0.19; then
  echo "wlroots 0.19 already installed — skipping build."
else
  cd /tmp
  if [ ! -d wlroots-0.19.3 ]; then
    curl -LO https://gitlab.freedesktop.org/wlroots/wlroots/-/archive/0.19.3/wlroots-0.19.3.tar.gz
    tar xzf wlroots-0.19.3.tar.gz
  fi
  cd wlroots-0.19.3
  # GLES2 renderer only (Intel UHD 620 on the Dell); no examples, no Vulkan.
  meson setup build --prefix=/usr/local -Dexamples=false -Drenderers=gles2
  ninja -C build
  sudo ninja -C build install
  sudo ldconfig
  echo "wlroots 0.19.3 installed to /usr/local."
fi

echo "== 3/5 Vile compositor =="
cd "$REPO_DIR/compositor"
meson setup build --wipe >/tmp/vile-build.log 2>&1
ninja -C build >>/tmp/vile-build.log 2>&1
echo "Compositor built."

echo "== 4/5 Launchers, config, PAM =="
chmod +x "$REPO_DIR/compositor/vile-foot.sh" \
  "$REPO_DIR/settings/vile-settings" \
  "$REPO_DIR/filemanager/vile-files" \
  "$REPO_DIR/setup/vile-setup"
sudo tee /usr/local/bin/vile > /dev/null <<'VILELAUNCHER'
#!/bin/bash
# Vile OS launcher — starts the compositor from ~/vile-os.
# Run from a text console (Ctrl+Alt+F2), NOT from inside another desktop.
set -e
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$HOME/vile-os/compositor"
if [ ! -x build/vile ]; then
  echo "Vile: first run — building the compositor (about a minute)..."
  meson setup build >/tmp/vile-build.log 2>&1
  ninja -C build >>/tmp/vile-build.log 2>&1
  echo "Vile: build done."
fi
echo "Vile: starting — bottom bar, dock, wallpaper."
exec ./build/vile "$@"
VILELAUNCHER
sudo chmod 755 /usr/local/bin/vile

mkdir -p "$HOME/.config/foot" "$HOME/.config/vile"
cp "$REPO_DIR/foot.ini" "$HOME/.config/foot/foot.ini"
sudo cp "$REPO_DIR/compositor/pam.d/vile" /etc/pam.d/vile

if [ -f "$REPO_DIR/assets/foot.desktop" ]; then
  sudo cp "$REPO_DIR/assets/foot.desktop" /usr/share/applications/foot.desktop
fi
if ls "$REPO_DIR"/assets/foot-logo*.png >/dev/null 2>&1; then
  sudo mkdir -p /usr/share/icons/vile
  sudo cp "$REPO_DIR"/assets/foot-logo*.png /usr/share/icons/vile/
else
  echo "NOTE: foot icons not in the repo yet — the dock will use a fallback."
fi

echo "== 5/5 ScreenFit =="
# NOTE: files pushed to GitHub via the API lose their executable bit, so
# make the scripts executable here instead of assuming the clone has them.
chmod +x "$REPO_DIR/screenfit/install.sh" "$REPO_DIR/archiso/build-iso.sh"
(cd "$REPO_DIR/screenfit" && ./install.sh)
# Make sure the compositor can find it on PATH even if ~/.local/bin is not.
sudo ln -sf "$HOME/.local/bin/screenfit" /usr/local/bin/screenfit

echo ""
echo "== Vile OS installed =="
echo "To run it: switch to a text console (Ctrl+Alt+F2 — on the Dell"
echo "Ctrl+Alt+Fn+F2), log in as $USER, and run:  vile"
echo "First run opens the Setup wizard. Give it your sudo password once"
echo "for the app installs."
echo "Keys: Windows+Enter terminal, Windows+Q close, Windows+J/K focus,"
echo "Windows+F fullscreen, Windows+Escape unfocus, Windows+L lock."
