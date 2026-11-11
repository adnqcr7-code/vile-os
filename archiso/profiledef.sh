# Vile OS archiso profile — v2 plan (Nov 10, 2026)
#
# Pure Arch, kernel-first: linux + base + systemd + pacman. No prebuilt
# Arch system (no EndeavourOS/CachyOS/Garuda). The desktop is the VILE
# COMPOSITOR (compositor/, our own wlroots compositor built from source),
# NOT the hyprland package.
#
# STATUS: profile updated for the wlroots plan, NOT yet built or booted.
# Piece 1 (compositor + wallpaper) is proven headless on Ubuntu; the ISO
# build (mkarchiso) + QEMU boot test is Piece 6.
#
# How to build it (on an Arch machine with archiso installed):
#   sudo pacman -S archiso
#   cp -r archiso /tmp/vile-iso && cd /tmp/vile-iso
#   sudo mkarchiso -v -w /tmp/vile-iso-work -o /tmp/vile-iso-out .
#
# If you build it, update this file and BUILD_LOG.md with the result.

iso_name="vile-os"
iso_label="VILE_OS"
iso_publisher="Vile OS <https://example.com/vile-os>"
iso_application="Vile OS live/install medium"
iso_version="0.5.6"
install_dir="arch"
bootmodes=('bios.syslinux' 'uefi.grub')
arch="x86_64"
pacman_conf="pacman.conf"
airootfs_image_type="squashfs"
# zstd level 3 + 2 processors, NOT zstd-15/4: the first zstd-15 build
# OOM-died mid-squashfs (27%, no error) on this 1.1GB-available-RAM cell
# (Nov 10, 2026). zstd-3 uses a far smaller window per thread. The .sfs
# is a bit bigger; the ISO still fits the quota. Revisit on a bigger box.
airootfs_image_tool_options=('-comp' 'zstd' '-Xcompression-level' '3' '-b' '1M' '-processors' '2')
file_permissions=(
  ["/etc/shadow"]="0:0:400"
  ["/etc/passwd"]="0:0:644"
  ["/root"]="0:0:750"
  ["/usr/local/bin/vile"]="0:0:755"
  ["/root/vile-os/settings/vile-settings"]="0:0:755"
  ["/root/vile-os/setup/vile-setup"]="0:0:755"
  ["/root/vile-os/compositor/vile-foot.sh"]="0:0:755"
  ["/usr/local/bin/screenfit"]="0:0:755"
  ["/root/vile-os/filemanager/vile-files"]="0:0:755"
)
