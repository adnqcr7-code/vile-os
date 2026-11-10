Vile OS 0.5.3 — live ISO
========================

This is a live Arch system. The desktop is the Vile compositor
(our own wlroots compositor, built from source — not Hyprland).

To start Vile, type at this prompt:
    vile

First it builds the compositor (about a minute, first run only),
then the first-run Setup wizard appears: name, profile picture,
wallpaper, apps, timezone. After that you land on the bare
desktop: your purple Arch wallpaper, a frosted bottom bar, and
the floating dock just above it.

The bar (bottom, 40px, frosted): Vile logo on the left, a bold
clock + date in the centre, status icons on the right (click
them — the menu rises UP). The dock holds Terminal, Settings,
ScreenFit (display fit for overscanning TVs), and Files.

  Windows key+Enter  new terminal (foot, dark title bar,
                      prints the Vile banner first)
  Windows key+Q      close window
  Windows key+J / K  move focus (purple border follows)
  Windows key+F      fullscreen
  Windows key+Escape clear focus
  Windows key+L      lock — type YOUR password + Enter to unlock

Before locking, set a password — the live root account has none:
    passwd

Settings (dock purple V, or from the system menu): Profile
Picture first, then Wallpaper — Purple Arch, Nebula, Dunes.
Click one and the desktop switches live, within a second.

Files (dock folder icon): real file manager — sidebar with
Home / Desktop / Documents / Downloads / Pictures / Music /
Videos, This PC with drive usage, search, grid/list, right-click
Compress to ZIP and Extract Here / Extract to…

What is where:
  /root/vile-os/compositor   compositor source (meson + ninja build)
  /root/vile-os/settings     Settings app (profile picture, wallpaper)
  /root/vile-os/setup        first-run Setup wizard
  /root/vile-os/filemanager  Files app (zip/unzip)
  /root/vile-os/wallpapers  purple Arch + nebula + dunes
  /root/vile-os/foot.ini    foot config (dark title bar — do not lose it)
  /usr/share/screenfit      ScreenFit app (display settings)
  /etc/pam.d/vile           PAM service for the lock screen

Real hardware is the real test: the mouse arrow was proven in
QEMU, but DRM on the Intel GPU, the lock with a real password,
and Reboot / Power Off still need the Dell run. If the mouse
arrow does not appear on your screen, that is the one open item
to report.
