#!/bin/bash
# Verify 0.5.6 .sfs contents: extract inside the chroot (/tmp/extract12),
# then check from the host path build-env/root.x86_64/tmp/extract12.
R=/home/hatch/workspace/vile-os/build-env/root.x86_64
W=/tmp/vile-iso-work/iso/arch/x86_64
E=$R/tmp/extract12
rm -rf "$E"
chroot "$R" unsquashfs -d /tmp/extract12 -f "$W/airootfs.sfs" \
  root/vile-os/compositor/src/main.c root/vile-os/compositor/vile-foot.sh \
  root/vile-os/compositor/meson.build root/vile-os/settings/vile-settings.py \
  root/vile-os/setup/vile-setup.py root/vile-os/setup/vile-setup \
  root/vile-os/filemanager/vile-files.py root/vile-os/filemanager/vile-files \
  root/vile-os/assets/foot-logo.png root/vile-os/assets/foot-logo-dark.png \
  root/vile-os/assets/files-icon.png root/vile-os/assets/files-icon-bare.png \
  root/vile-os/assets/vile-logo.png root/vile-os/assets/vile-logo-dark.png \
  root/vile-os/assets/vile-logo-512.png root/vile-os/assets/vile-logo-256.png \
  root/vile-os/assets/arch-logo.png root/vile-os/wallpapers/vile-purple-arch-blur-light.jpg \
  root/vile-os/wallpapers/vile-nebula.jpg root/vile-os/wallpapers/vile-purple-arch.jpg \
  root/vile-os/assets/cursor-theme/vile/cursors/default \
  root/vile-os/assets/cursor-theme/vile/cursors/left_ptr \
  root/vile-os/assets/cursor-theme/vile/cursors/pointer \
  root/vile-os/assets/cursor-theme/vile/index.theme \
  root/vile-os/assets/build-cursor-theme.py \
  usr/share/icons/vile/cursors/default usr/share/icons/vile/cursors/left_ptr \
  usr/share/icons/vile/cursors/pointer usr/share/icons/vile/index.theme \
  root/.config/foot/foot.ini \
  root/VILE-README.txt root/vile-os/README.md root/vile-os/vile-install.sh \
  root/vile-os/docs/BUILD_LOG.md usr/share/screenfit/screenfit.py \
  usr/local/bin/screenfit etc/pam.d/vile etc/ssl/certs/ca-certificates.crt 2>&1 | tail -1
echo "--- md5s (sfs vs workspace source):"
md5sum "$E/root/vile-os/compositor/src/main.c" "$E/root/vile-os/filemanager/vile-files.py" "$E/root/vile-os/setup/vile-setup.py" "$E/root/vile-os/settings/vile-settings.py" "$E/usr/share/screenfit/screenfit.py"
md5sum /home/hatch/workspace/vile-os/compositor/src/main.c /home/hatch/workspace/vile-os/filemanager/vile-files.py /home/hatch/workspace/vile-os/setup/vile-setup.py /home/hatch/workspace/vile-os/settings/vile-settings.py /home/hatch/workspace/intel-display-center/src/screenfit.py
echo "--- foot logos in .sfs (dark must start b0a9a3ec, master e5a2f29d):"
sha256sum "$E/root/vile-os/assets/foot-logo-dark.png" "$E/root/vile-os/assets/foot-logo.png" | cut -c1-16
echo "--- banner / versions (must say 0.5.6; Settings 0.3.0):"
grep "v0.5.6" "$E/root/vile-os/compositor/vile-foot.sh"
grep -h "0.5.6" "$E/root/vile-os/compositor/meson.build" "$E/root/vile-os/settings/vile-settings.py"
grep 'VERSION = "0.3.0"' "$E/root/vile-os/settings/vile-settings.py"
head -1 "$E/root/VILE-README.txt"
echo "--- vile logos in .sfs (master 95c3211d, dark 4f3be1cc):"
sha256sum "$E/root/vile-os/assets/vile-logo.png" "$E/root/vile-os/assets/vile-logo-dark.png" | cut -c1-16
echo "--- 0.5.5 assets still there (bare icon f30b26d1, blur-light 63b41b88):"
sha256sum "$E/root/vile-os/assets/files-icon-bare.png" "$E/root/vile-os/wallpapers/vile-purple-arch-blur-light.jpg" | cut -c1-16
echo "--- overlay deleted (must be 0) / frosted dock + hold fix + Log Out present:"
grep -c "vile_load_logo\|vile_fit_logo" "$E/root/vile-os/compositor/src/main.c" || true
grep -c "No logo overlay" "$E/root/vile-os/compositor/src/main.c"
grep -c "Log Out" "$E/root/vile-os/compositor/src/main.c"
grep -c "vile_shell_build_dock_frost" "$E/root/vile-os/compositor/src/main.c"
grep -c "Frosted dock" "$E/root/vile-os/compositor/src/main.c"
grep -c "dropped in place" "$E/root/vile-os/compositor/src/main.c"
echo "--- wizard Enter fix + --step fix (must be >0):"
grep -c "grab_focus" "$E/root/vile-os/setup/vile-setup.py"
grep -c "Re-assert the initial step" "$E/root/vile-os/setup/vile-setup.py"
grep -c 'VERSION = "0.5.6"' "$E/root/vile-os/setup/vile-setup.py"
echo "--- ScreenFit dark (CSS_DARK must be >0):"
grep -c "CSS_DARK" "$E/usr/share/screenfit/screenfit.py"
echo "--- NEW 0.5.6: cursor theme (must be 83baed44, magic 'Xcur'):"
sha256sum "$E/usr/share/icons/vile/cursors/default" "$E/root/vile-os/assets/cursor-theme/vile/cursors/default" | cut -c1-16
head -c 4 "$E/usr/share/icons/vile/cursors/default"; echo
head -c 4 "$E/usr/share/icons/vile/cursors/left_ptr"; echo
head -c 4 "$E/usr/share/icons/vile/cursors/pointer"; echo
ls -l "$E/usr/share/icons/vile/cursors/" "$E/usr/share/icons/vile/index.theme"
echo "--- NEW 0.5.6: launch jump + cursor loading in main.c (all must be >0):"
grep -c "VILE_LAUNCH_JUMP_MS" "$E/root/vile-os/compositor/src/main.c"
grep -c "vile_launch_jump_ms" "$E/root/vile-os/compositor/src/main.c"
grep -c "launch jump start" "$E/root/vile-os/compositor/src/main.c"
grep -c "launch done" "$E/root/vile-os/compositor/src/main.c"
grep -c "launch timeout" "$E/root/vile-os/compositor/src/main.c"
grep -c "wlr_xcursor_manager_create" "$E/root/vile-os/compositor/src/main.c"
grep -c "cursor theme 'vile'" "$E/root/vile-os/compositor/src/main.c"
grep -c "vile_cursor_theme_available" "$E/root/vile-os/compositor/src/main.c"
grep -c "shell_spin_frames" "$E/root/vile-os/compositor/src/main.c"
echo "--- NEW 0.5.6: vile-install.sh installs the cursor theme:"
grep -c "cursor-theme" "$E/root/vile-os/vile-install.sh"
grep -c "assets/cursor-theme/vile/cursors" "$E/root/vile-os/vile-install.sh"
echo "--- foot.ini / pam / launchers / files 1.2.0 / install script / docs:"
grep -c "\[colors-dark\]" "$E/root/.config/foot/foot.ini"
ls -l "$E/etc/pam.d/vile" "$E/root/vile-os/filemanager/vile-files" "$E/root/vile-os/setup/vile-setup" "$E/usr/local/bin/screenfit" "$E/root/vile-os/vile-install.sh" "$E/root/vile-os/README.md" "$E/root/vile-os/docs/BUILD_LOG.md"
grep -c 'VERSION = "1.2.0"' "$E/root/vile-os/filemanager/vile-files.py"
grep -c "wayland-sessions/vile.desktop" "$E/root/vile-os/vile-install.sh"
echo "--- NO Hatch certs in ISO trust store (must be 0):"
rm -f /tmp/iso-cert-*; openssl crl2pkcs7 -nocrl -certfile "$E/etc/ssl/certs/ca-certificates.crt" 2>/dev/null | openssl pkcs7 -print_certs -out /tmp/iso-bundle-certs.pem 2>/dev/null; csplit -s -f /tmp/iso-cert- /tmp/iso-bundle-certs.pem '/-----BEGIN CERTIFICATE-----/' '{*}' 2>/dev/null; HATCH=0; for f in /tmp/iso-cert-*; do s=$(openssl x509 -in $f -noout -subject 2>/dev/null); case "$s" in *Hatch*) HATCH=$((HATCH+1));; esac; done; echo "Hatch certs in ISO bundle: $HATCH"
