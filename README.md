# Vile OS

A custom Arch-based OS with a **from-scratch wlroots compositor** — no Hyprland,
no prebuilt desktop. Dark, minimal, purple. Built piece by piece, each piece
visually tested before the next one started.

- **v0.5.6** (this repo): light/dark theme (asked in Setup, switchable in
  Settings), System monitor in Settings (CPU / memory / disk / network /
  GPU), draggable + resizable windows, working minimize/maximize,
  frosted glass dock (hold to reorder, right-click to unpin), purple
  cursor (Adnan's arrow, the default pointer), KDE-style launch jump on
  the dock (the icon bounces while its app loads), no Arch logo stamped
  on wallpapers, Vile in the LightDM session list, Log Out in the
  system menu.
- **v0.5.3**: proven — boots in QEMU, first-run Setup wizard, frosted
  bottom bar, floating dock, Settings, Files, lock screen with real PAM
  unlock, live wallpaper switching, random default wallpaper.

## Install on Arch (the Dell)

```sh
git clone https://github.com/adnqcr7-code/vile-os.git ~/vile-os
cd ~/vile-os
bash vile-install.sh
```

The script installs the Arch packages, builds wlroots 0.19.3 from source
(Arch dropped the unversioned `wlroots` package in Nov 2026 — only
wlroots 0.20 remains, and the compositor needs the 0.19 API), builds the
compositor, and installs the launchers, foot config, PAM service and
ScreenFit. Takes ~15–20 minutes, mostly the wlroots build.

Then switch to a text console (**Ctrl+Alt+F2** — on the Dell Latitude 7400
that's **Ctrl+Alt+Fn+F2**), log in, and run:

```sh
vile
```

First run opens the Setup wizard: name, profile picture, wallpaper, theme,
apps, timezone. Give it your sudo password once for the app installs.

The install also adds **Vile to the login screen**: at the LightDM greeter,
pick "Vile" from the session list to log into Vile, or pick Cinnamon /
Hyprland to go back to your other desktop. Inside Vile, Log Out in the
system menu (bottom bar, right side) takes you back to the greeter.

## Build the bootable ISO

On an Arch machine with `archiso` installed:

```sh
cd archiso
sudo bash ./build-iso.sh
```

Writes the ISO to `/tmp/vile-iso-out/`. Write it to USB
(`sudo dd if=vile-os-0.5.6-x86_64.iso of=/dev/sdX bs=4M status=progress`)
and boot it.

## Keys

- **Windows+Enter** new terminal · **Windows+Q** close
- **Windows+J / K** move focus · **Windows+F** fullscreen
- **Windows+Escape** clear focus · **Windows+L** lock
- Drag a window by its title bar (or hold **Windows** and drag anywhere on
  it); drag an edge/corner to resize. Minimize/maximize buttons work;
  minimized windows come back from the dock.
- Dock: hold an icon to drag it left/right and reorder; right-click an icon
  to unpin it.

## Layout

- `compositor/` — the OS itself (C, wlroots 0.19.3): bar, dock, tiling,
  lock screen, animations, wallpaper + theme hooks
- `settings/` — Vile Settings (Python + GTK3): profile picture, wallpaper,
  System monitor, About
- `filemanager/` — Vile Files: grid/list, sidebar, This PC, zip/unzip
- `setup/` — first-run Setup wizard
- `screenfit/` — ScreenFit display-fit tool (bundled core app)
- `wallpapers/` — the wallpapers (purple Arch, nebula, dunes)
- `assets/` — logos, icons, foot desktop file
- `archiso/` — the bootable-ISO profile + `build-iso.sh`
- `docs/` — build log, design research, judge audits

## Known issues

- **First-boot dock clicks are fixed in 0.5.5** (the cause was a 400ms hold
  timer eating slow clicks; dock clicks now work in the first session in
  QEMU). One open item from QEMU: a foot dock click needed a second try
  once (iso9b) — worth checking on the first Dell run of 0.5.6.
- The wizard's real Arch app installs are unproven until a Dell run
  (only the dry-run command log is proven).
- Volume slider is visual-only headless; menu blur is a frosted look,
  not true live blur (wlroots has no blur primitive).

## License

MIT — see `LICENSE`.
