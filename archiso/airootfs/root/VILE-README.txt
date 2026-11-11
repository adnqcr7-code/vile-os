Vile OS 0.5.6 — live ISO
========================

This is a live Arch system. The desktop is the Vile compositor
(our own wlroots compositor, built from source — not Hyprland).

To start Vile, type at this prompt:
    vile

First it builds the compositor (about a minute, first run only),
then the first-run Setup wizard appears: name, profile picture,
wallpaper, theme (dark / light), apps, timezone. After that you
land on the bare desktop: a wallpaper (Purple Arch or Nebula,
picked randomly on first boot), a frosted bottom bar, and the
frosted glass dock — macOS-style, the wallpaper faintly visible
through it.

The mouse cursor is a purple arrow (Vile's own). Click a dock
icon and it jumps while the app loads — KDE-style launch
feedback, with a little purple spinner on the icon until the
window opens.

The bar (bottom, 40px, frosted): a bold clock + date in the
centre, status icons on the right (click them — the menu rises
UP, with Lock, Settings, Log Out, Power Off). The dock holds
Terminal, Settings, ScreenFit (display fit for overscanning
TVs), and Files. Windows are draggable — grab a title bar and
move it; drag a window edge to resize it.

  Windows key+Enter  new terminal (foot, dark title bar,
                      prints the Vile banner first)
  Windows key+Q      close window
  Windows key+J / K  move focus (purple border follows)
  Windows key+F      fullscreen
  Windows key+Escape clear focus
  Windows key+L      lock — type YOUR password + Enter to unlock

Before locking, set a password — the live root account has none:
    passwd

Settings (dock V, or from the system menu): Profile Picture
first, then Theme (dark / light, switches live), Wallpaper —
Purple Arch, Nebula, Dunes (click one and the desktop switches
live, within a second), System (CPU / RAM / disk / network /
GPU usage), About.

Files (dock folder icon): real file manager — sidebar with
Home / Desktop / Documents / Downloads / Pictures / Music /
Videos, This PC with drive usage, search, grid/list, right-click
Compress to ZIP and Extract Here / Extract to…

What is where:
  /root/vile-os/compositor   compositor source (meson + ninja build)
  /root/vile-os/settings     Settings app (profile picture, theme,
                             wallpaper, system monitor)
  /root/vile-os/setup        first-run Setup wizard
  /root/vile-os/filemanager  Files app (zip/unzip)
  /root/vile-os/wallpapers  purple Arch + nebula + dunes
  /root/vile-os/foot.ini    foot config (dark title bar — do not lose it)
  /root/vile-os/vile-install.sh  installer for a real Arch system
  /usr/share/screenfit      ScreenFit app (display settings)
  /etc/pam.d/vile           PAM service for the lock screen

Real hardware is the real test: the purple cursor and the dock
jump were proven in QEMU, but DRM on the Intel GPU, the lock
with a real password, and Reboot / Power Off still need the
Dell run.
