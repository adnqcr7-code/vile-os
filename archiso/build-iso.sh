#!/bin/bash
# Vile OS — build the bootable ISO from this repo (on Arch Linux).
#
#   sudo pacman -S archiso
#   cd archiso
#   sudo ./build-iso.sh
#
# Writes the ISO to /tmp/vile-iso-out/. Needs sudo, ~10 GB free disk,
# and a network connection (pacstrap downloads the base packages).
#
# NOTE: this script is derived from the exact steps that built the proven
# 0.5.3 ISO (Nov 10, 2026), but it has NOT been run end-to-end from this
# file yet — the 0.5.3 ISO was built with the equivalent steps in the
# build-env/ workspace. If it fails, the build log is in /tmp/vile-iso-build.log.
set -e
PROFILE_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$PROFILE_DIR/.." && pwd)"
WORK_DIR="${VILE_ISO_WORK:-/tmp/vile-iso-work}"
OUT_DIR="${VILE_ISO_OUT:-/tmp/vile-iso-out}"
BUILD_LOG=/tmp/vile-iso-build.log

command -v mkarchiso >/dev/null 2>&1 || {
  echo "mkarchiso not found — run: sudo pacman -S archiso"
  exit 1
}
[ "$(id -u)" -eq 0 ] || { echo "Run this as root (sudo ./build-iso.sh)."; exit 1; }

echo "== Vile ISO build ==" | tee "$BUILD_LOG"
echo "Profile: $PROFILE_DIR" | tee -a "$BUILD_LOG"
echo "Repo:    $REPO_DIR" | tee -a "$BUILD_LOG"

# --- Stage the repo tree into the profile's airootfs -----------------------
# mkarchiso copies profile/airootfs into the live image. The live system
# runs `vile` from /root/vile-os, which is a copy of this repo (minus the
# archiso profile itself and build artefacts).
STAGE="$PROFILE_DIR/airootfs/root/vile-os"
rm -rf "$STAGE"
mkdir -p "$STAGE"
for d in compositor settings filemanager setup screenfit assets wallpapers docs; do
  [ -d "$REPO_DIR/$d" ] && cp -r "$REPO_DIR/$d" "$STAGE/"
done
for f in foot.ini README.md LICENSE vile-install.sh; do
  [ -f "$REPO_DIR/$f" ] && cp "$REPO_DIR/$f" "$STAGE/"
done
# Never ship build dirs or caches into the image.
rm -rf "$STAGE/compositor/build" "$STAGE"/**/__pycache__ 2>/dev/null || true
find "$STAGE" -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null || true
chmod +x "$STAGE/compositor/vile-foot.sh" "$STAGE/settings/vile-settings" \
  "$STAGE/filemanager/vile-files" "$STAGE/setup/vile-setup" 2>/dev/null || true
# Stage the foot icons + default wallpaper into the airootfs from the repo's
# assets/ and wallpapers/ (they live once in the repo, not duplicated).
mkdir -p "$PROFILE_DIR/airootfs/usr/share/icons/vile" \
  "$PROFILE_DIR/airootfs/root/.config/vile/wallpapers"
cp "$REPO_DIR"/assets/foot-logo*.png \
  "$PROFILE_DIR/airootfs/usr/share/icons/vile/" 2>/dev/null || true
cp "$REPO_DIR/wallpapers/vile-purple-arch.jpg" \
  "$PROFILE_DIR/airootfs/root/.config/vile/wallpapers/" 2>/dev/null || true

# --- Build wlroots 0.19.3 into the airootfs /usr/local ----------------------
# The live system builds the compositor on first boot (see
# airootfs/usr/local/bin/vile) and needs wlroots 0.19 headers + lib in
# /usr/local. Arch only ships wlroots 0.20, so build 0.19.3 from source
# and stage it with DESTDIR.
WLROOTS_SRC=/tmp/wlroots-0.19.3
if [ ! -d "$WLROOTS_SRC" ]; then
  curl -L -o /tmp/wlroots-0.19.3.tar.gz \
    https://gitlab.freedesktop.org/wlroots/wlroots/-/archive/0.19.3/wlroots-0.19.3.tar.gz
  tar xzf /tmp/wlroots-0.19.3.tar.gz -C /tmp
fi
# Build deps for wlroots on the BUILD machine (Arch).
pacman -Sy --needed --noconfirm base-devel meson ninja pkgconf wayland \
  wayland-protocols libinput libxkbcommon pixman libseat libdrm mesa \
  libdisplay-info >>"$BUILD_LOG" 2>&1
cd "$WLROOTS_SRC"
meson setup build --prefix=/usr/local -Dexamples=false -Drenderers=gles2 \
  >>"$BUILD_LOG" 2>&1
ninja -C build >>"$BUILD_LOG" 2>&1
DESTDIR="$PROFILE_DIR/airootfs" ninja -C build install >>"$BUILD_LOG" 2>&1
ldconfig -r "$PROFILE_DIR/airootfs" 2>/dev/null || true

# --- Build the ISO ----------------------------------------------------------
mkdir -p "$OUT_DIR"
mkarchiso -v -w "$WORK_DIR" -o "$OUT_DIR" "$PROFILE_DIR" 2>&1 | tee -a "$BUILD_LOG"
echo "" | tee -a "$BUILD_LOG"
echo "ISO written to $OUT_DIR" | tee -a "$BUILD_LOG"
ls -la "$OUT_DIR" | tee -a "$BUILD_LOG"
