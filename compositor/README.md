# Vile compositor

A **new Wayland compositor** for Vile OS, written from scratch in C on
**wlroots 0.19**. Not a Hyprland fork, not the Hyprland package — our own
code, built the way Hyprland was originally built (on wlroots).

## Status — Piece 2 DONE (Nov 10, 2026, ~03:24 EST)

- [x] **Piece 1:** backend + renderer + output + wallpaper. Boots to
  Adnan's purple Arch screen, full-screen 1920x1080.
  Proof: [../screenshots/vile-screen-01.png](../screenshots/vile-screen-01.png)
  — captured with `grim` from the running compositor, visually checked,
  pixel-checked (mean diff vs wallpaper 0.00/255, max 1).
- [x] **Piece 2:** first window (xdg-shell) — `foot` launches, renders
  centred 960x600 on top of the wallpaper, real prompt + cursor.
  Proof: [../screenshots/vile-screen-02.png](../screenshots/vile-screen-02.png)
  — captured with `grim` from the running compositor, visually checked.
  Needed a minimal `wl_seat` global first (foot refuses to open a
  window without one); no input handling yet.
- [x] Piece 3: input — keyboard + mouse, focus, close window with keybind.
  Proven Nov 10 with real grim screenshots (`vile-input-01/02/03.png`):
  typing into the focused window, Super+Enter opens a second foot,
  Super+Q closes it, click on empty wallpaper clears focus, click
  back in foot restores it, Super+L locks (clean v2 screen) and the
  PAM password unlocks it. See BUILD_LOG.md "Piece 3 — input".
  One open item: the visible mouse arrow never appears in headless
  grim shots — the headless backend claims the cursor as hardware
  and renders nothing, so the arrow needs a real-hardware check.
- [x] Piece 4: window management — tiling / placement + wallpaper stays behind.
  Done Nov 10, 2026 ~04:52 EST. Master-stack, gap 12px, border 4px.
  Proof: screenshots/vile-tiling-01/02/03.png. See BUILD_LOG.md "Piece 4".
- [x] Piece 5: Vile look — purple `#6D4AFF`, 14px rounded corners, top bar,
  huge centered clock, dock. Done Nov 10, 2026 ~05:30 EST.
  Proof: screenshots/vile-look-01/02/03.png + vile-look-menu-final.png.
  See BUILD_LOG.md "Piece 5".
- [ ] Piece 6: package into pure-Arch archiso ISO, boot-test in QEMU/VM.
- [x] Lock screen v2 (Nov 10) — video-style: blurred wallpaper ONLY behind,
  huge clock + date, user card (avatar + name + LOCKED + password field),
  settings panel (gear), real PAM unlock. No windows bleed through.
  Proof: [../screenshots/vile-lock-10.png](../screenshots/vile-lock-10.png)
  (locked, clean), [../screenshots/vile-lock-11.png](../screenshots/vile-lock-11.png)
  (unlocked after correct password),
  [../screenshots/vile-lock-12.png](../screenshots/vile-lock-12.png)
  (wrong password stays locked),
  [../screenshots/vile-lock-13.png](../screenshots/vile-lock-13.png)
  (settings panel open). See `../BUILD_LOG.md`.

## What it is

`src/main.c` (~750 lines, Pieces 1–2) does six things:

1. **Backend** — `wlr_backend_autocreate()`. DRM on real hardware,
   headless/X11 when `WLR_BACKENDS` says so.
2. **Renderer** — `wlr_renderer_autocreate()`. GLES2 on hardware,
   pixman headless.
3. **Output** — one output. Headless forces a custom 1920x1080 mode so
   the test screenshot matches the wallpaper 1:1; real hardware uses
   the monitor's preferred mode.
4. **Wallpaper** — Adnan's default screen (`wallpapers/vile-purple-arch.jpg`,
   purple Arch logo, dark bg) is decoded with stb_image, painted into a
   wlroots buffer, and put in the scene graph as the bottom-most node.
   The compositor refuses to run if the wallpaper fails to load — the
   screen is never black, never a stock wallpaper.

Also enabled already: screencopy (`wlr_screencopy_manager_v1`, so `grim`
can screenshot) and xdg-output (so clients learn output names/sizes).

5. **xdg-shell** — application windows. New toplevels get a scene tree,
   are configured 960x600, centred on map, raised above the wallpaper,
   and activated. Popups are hosted in the parent's scene tree.
6. **Startup command** — `-s <cmd>` / `VILE_STARTUP`, forked once the
   backend is up. Piece 2 runs `foot`. A minimal `wl_seat` global is
   advertised (foot will not open a window without one) — no input
   handling yet, that is Piece 3.

**Lock screen** — `-l` / `VILE_LOCK=1` starts locked. While locked, ALL
windows are hidden entirely (their scene nodes are disabled) and the
only thing behind the UI is the wallpaper, pre-blurred + darkened
(`wallpapers/vile-purple-arch-blur.jpg`), as a full-screen node in the
lock tree. The UI floats on it — no box: huge clock, date, user card
(avatar + username + LOCKED + password field with masked dots), settings
gear bottom-right opening a panel (Hide password toggle, Reveal delay,
System Reboot / Power Off). Enter runs a real PAM check (`pam_unix`,
service `vile` — install `pam.d/vile` as `/etc/pam.d/vile`); correct
password destroys the overlay and brings the windows back, wrong
password shows "Wrong password" and stays locked. Minimal keyboard
handling (physical + virtual keyboards) lives here; full input is
Piece 3 (done, screenshot-proven Nov 10).

## Build

Needs wlroots 0.19, wayland-server ≥ 1.23, pixman ≥ 0.43, libdrm.

**On Arch (Adnan's laptop):**

```
sudo pacman -S --needed wlroots wayland wayland-protocols meson ninja gcc pkg-config libdrm pixman libinput libxkbcommon libseat foot grim
cd ~/vile-os/compositor
meson setup build
ninja -C build
```

**On Ubuntu 24.04 (this VM):** the system wayland (1.22) and pixman
(0.42) are too old for wlroots 0.19.3, so wayland 1.24, pixman 0.46.4
and wlroots 0.19.3 were built from source into `/usr/local`
(see `../BUILD_LOG.md`). Then:

```
export PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
meson setup build && ninja -C build
```

## Run

**Headless test (screenshot proof):**

```
./run-headless.sh [seconds] [screenshot-path]
# Piece 1: ./run-headless.sh 6 ../screenshots/vile-screen-01.png
# Piece 2: VILE_STARTUP="foot" ./run-headless.sh 12 ../screenshots/vile-screen-02.png
```

Runs the real compositor on the headless backend + pixman renderer,
waits for frames, captures with `grim -o HEADLESS-1`, checks the PNG
is non-empty and 1920x1080.

**On real hardware (Arch laptop, from a TTY):**

```
./build/vile -s foot          # desktop with a terminal
./build/vile -l -s foot       # locked first — type your password, Enter
```

Needs `pam.d/vile` installed as `/etc/pam.d/vile` for the lock screen,
and packages `pango cairo pam` in addition to the Piece 1 list.

Uses the DRM backend and your monitor's preferred mode. Quit with
Ctrl+C, or set `VILE_QUIT_AFTER_SEC=10` to auto-exit.

## Env vars

| Var | Default | Meaning |
|---|---|---|
| `VILE_WALLPAPER` | `../wallpapers/vile-purple-arch.jpg` | Wallpaper JPEG to draw |
| `VILE_QUIT_AFTER_SEC` | `0` (forever) | Auto-exit after N seconds |
| `VILE_SOCKET_FILE` | — | Write the Wayland socket name here |
| `VILE_STARTUP` | — | Command to run once up (e.g. `foot`); same as `-s` |
| `VILE_LOCK` | — | `1` = start with the lock screen on (same as `-l`) |
| `VILE_LOCK_USER` | your username | User the lock screen checks (test only) |
| `WLR_BACKENDS` | wlroots default | `headless` / `drm` / `x11` |
| `WLR_RENDERER` | wlroots default | `pixman` / `gles2` / `vulkan` |
| `WLR_HEADLESS_OUTPUTS` | — | Number of headless outputs |

## Files

- `src/main.c` — the compositor (Pieces 1–2 + lock screen + profile
  picture). The lock avatar reads `~/.config/vile/profile.png` —
  see Settings below.
- `../settings/vile-settings.py` — Vile Settings (Python + GTK3).
  Profile Picture is the FIRST section: big circular preview,
  username, Change Picture… (GTK file picker, JPG/PNG) / Remove
  Picture. Saving centre-square-crops to 256×256 and writes
  `~/.config/vile/profile.png` + `~/.face`; Settings live-refreshes
  every second, and the lock avatar reads the same file, so a change
  shows on the next lock. CLI: `--set-picture <file>`,
  `--remove-picture`. Tests: `tests/run-settings-test.sh`
  (shots 01/02/03) and `tests/run-settings-picker-test.sh` (real
  picker click + Ctrl+L path + Open-button click).
- `src/stb_image.h` — public-domain JPEG/PNG decoder (Sean Barrett).
  Only used to decode the wallpaper.
- `meson.build` — build file. Generates `xdg-shell-protocol.h` from
  wayland-protocols with `wayland-scanner` (a source-built wlroots does
  not install it).
- `pam.d/vile` — PAM service for the lock screen; install as
  `/etc/pam.d/vile` on the target machine.
- `tests/pam_test.c` — standalone PAM check (proves PAM works before
  the compositor uses it). `tests/run-lock-test.sh` — headless lock
  test: locked shot, type password with `wtype`, after shot.
  `tests/run-lock-redesign-test.sh correct|wrong` — lock v2 test
  (locked clean shot, after shot). `tests/run-lock-settings-test.sh`
  — gear-click / settings-panel test. `tests/vptr_test.c` — minimal
  virtual-pointer client (move + click by absolute coords) used by
  the settings test and Piece 3 input tests.
- `run-headless.sh` — headless run + grim screenshot test.
