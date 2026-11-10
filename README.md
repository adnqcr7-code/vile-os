# Vile OS

A custom Arch-based OS with a **from-scratch wlroots compositor** — no Hyprland,
no prebuilt desktop. Dark, minimal, purple. Built piece by piece, each piece
visually tested before the next one started.

- **v0.5.3** (this repo): proven — boots in QEMU, first-run Setup wizard,
  frosted bottom bar, floating dock, Settings, Files, lock screen with real
  PAM unlock, live wallpaper switching, random default wallpaper, Arch logo
  overlay.
- **v0.5.4** (in progress): light/dark theme + System monitor in Settings.

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

First run opens the Setup wizard: name, profile picture, wallpaper, apps,
timezone. Give it your sudo password once for the app installs.

## Build the bootable ISO

On an Arch machine with `archiso` installed:

```sh
cd archiso
sudo bash ./build-iso.sh
```

Writes the ISO to `/tmp/vile-iso-out/`. Write it to USB
(`sudo dd if=vile-os-0.5.3-x86_64.iso of=/dev/sdX bs=4M status=progress`)
and boot it.

## Keys

- **Windows+Enter** new terminal · **Windows+Q** close
- **Windows+J / K** move focus · **Windows+F** fullscreen
- **Windows+Escape** clear focus · **Windows+L** lock

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

- **First-boot dock/bar clicks may not respond in the first session**
  (seen twice in QEMU; worked after restarting the desktop with
  `pkill -x vile && vile`). If it happens on real hardware, that's the
  bug to fix first — check it before anything else on the Dell run.
- The wizard's real Arch app installs are unproven until a Dell run
  (only the dry-run command log is proven).
- Volume slider is visual-only headless; menu blur is a frosted look,
  not true live blur (wlroots has no blur primitive).

## License

MIT — see `LICENSE`.
