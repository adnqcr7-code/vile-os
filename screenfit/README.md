# ScreenFit

A simple display settings GUI for Linux — a community clone of the
Intel Graphics Command Center display page. Built because a 32" TV
over HDMI overscans and crops the taskbar, and the fix (an `xrandr`
transform) is painful to type by hand.

**Not affiliated with Intel.** This is an independent project that
recreates the *idea* (a GUI for display settings) on Linux. It does
not use Intel software, branding assets, or drivers.

## What it does (v0.4.0)

- **Detects your displays** by parsing `xrandr` output — name (e.g.
  `HDMI-1`), current resolution, refresh rate, and physical size.
- **Resolution dropdown** — pick any mode your display supports and
  Apply it with `xrandr`.
- **Fit to screen sliders — the key feature.** Separate **Horizontal**
  and **Vertical** sliders, each from **-10% to +10%**. Positive %
  shrinks the picture so the whole desktop fits (the usual fix for
  TV overscan, which crops the taskbar). Negative % expands / zooms
  the picture in, to fill the screen if there is a black border
  around it. Defaults to +5% on both axes. **The sliders do NOT
  change the real screen while you drag** — they update the Preview
  live, and the real display only changes when you click **Apply**
  (or **Reset**). That was a deliberate change in v0.4.0: the
  live-apply in v0.3.0 kept fighting the user while dragging.
- **Preview — a miniature of your real desktop** in the middle of the
  window. It has the **aspect ratio of the selected resolution**
  (1920x1080 shows as 16:9; pick another resolution and the box
  changes shape), a blue desktop background in Cinnamon's wallpaper
  tone, and a **taskbar along the bottom** with a menu button and a
  few app icons, at the same proportions as a real Cinnamon panel
  (about 40px tall at 1080p). The whole mini-desktop scales with the
  sliders and stays centred: shrink it and dark border shows around
  it; expand it and its edges — taskbar included — get clipped by
  the mini-screen, exactly like the real display. So you can see
  whether the taskbar will be visible **before** you click Apply.
  No image file is loaded; it is drawn in code.
- **Refresh rate** shown read-only.
- **Apply at login** — saves the current fit-to-screen settings to a
  Cinnamon autostart file
  (`~/.config/autostart/screenfit-overscan.desktop`) so it comes back
  every boot. It is only written by **Apply** or by ticking the
  checkbox — never by moving a slider.
- **No root needed.** It runs as your user; `xrandr` works as a user.

## The fit-to-screen math

For a screen of width W and height H with horizontal shrink factor
`sx` and vertical shrink factor `sy` (1.0 = no change, 0.95 = 5%
shrink, 1.10 = 10% expand), the transform is:

```
xrandr --output <display> --transform sx,0,W*(1-sx)/2,0,sy,H*(1-sy)/2,0,0,1
```

Example — 1920x1080 at +5% shrink (0.95) on both axes:

```
xrandr --output HDMI-1 --transform 0.95,0,48,0,0.95,27,0,0,1
```

Offsets are 48 horizontal and 27 vertical: `1920*0.05/2 = 48`,
`1080*0.05/2 = 27`. Negative percentages flip the offsets
(expand): 1920x1080 at -10% (1.10) gives `-96` and `-54`.

## Install

### Option A — Arch package (recommended)

```sh
cd intel-display-center
makepkg -si
```

Then run `screenfit` from a terminal, or find "ScreenFit"
in the Cinnamon menu.

### Option B — user install, no root

```sh
cd intel-display-center
./install.sh
```

This copies the app to `~/.local/share/screenfit/`, a launcher
to `~/.local/bin/screenfit`, and a menu entry to
`~/.local/share/applications/`. Run it with `screenfit`.

### Dependencies

- `python` (3.x)
- `python-gobject` + `gtk3` (the GUI)
- `python-cairo` (the preview gradient; the app falls back to a
  solid colour if it is missing)
- `xorg-xrandr` (reading and changing display settings)

Install them on Arch with:

```sh
sudo pacman -S python python-gobject python-cairo gtk3 xorg-xrandr
```

## Usage

1. Open **ScreenFit**.
2. Pick your display (e.g. `HDMI-1`) if more than one is connected.
3. Pick a resolution, or leave the current one.
4. Move the **Horizontal** and **Vertical** Fit to screen sliders
   until the mini-desktop in the **Preview** looks right — the
   taskbar fully visible inside the box, with as little dark border
   as possible (start at +5% on both; use negative values if there
   is a black border to fill in). The Preview updates live as you
   drag, but the real screen does not change yet.
5. Click **Apply** to change the real screen to what the Preview
   shows. If it looks right, tick **Apply this overscan
   fix at login** so it survives a reboot.
6. **Reset (no shrink)** removes the transform entirely.

## Files

```
intel-display-center/
  README.md                  this file
  LICENSE                    MIT
  PKGBUILD                   Arch package build
  install.sh                 user install (no root)
  src/
    screenfit.py             GTK 3 GUI
    xrandr.py                xrandr parsing + transform math (no GTK)
    screenfit                launcher script
    screenfit.desktop        menu entry
  tests/
    test_xrandr.py           unit tests for parsing + transform math
```

## History

- v0.1.0 — first version: one uniform shrink slider (0–10%).
- v0.2.0 — separate Horizontal and Vertical sliders.
- v0.3.0 — sliders now run -10% to +10% (expand as well as shrink),
  the app was renamed from Display Center to **ScreenFit**, the
  sliders apply **live** (no Apply click needed for the shrink),
  and a **Preview** picture shows the effect in the window.
- v0.4.0 — the live-apply is **removed** (it fought the user while
  dragging): sliders now only update the Preview, and the real
  screen changes on **Apply** / **Reset**. The Preview is now a
  **miniature of the real desktop** — aspect ratio of the selected
  resolution, blue desktop, and a taskbar along the bottom at real
  Cinnamon panel proportions — so you can see whether the taskbar
  will be visible before you click Apply.

## What was verified

- `python3 -m py_compile` passes on all `.py` files.
- `tests/test_xrandr.py` passes: it parses a real `xrandr` output
  sample (HDMI-1, 1920x1080 @ 60 Hz, 708mm x 398mm) and checks the
  transform math, including 1920x1080 at 0.95 = offsets 48 and 27,
  per-axis 0.95/0.90 = 48 and 54, and expand 1.10 = offsets -96
  and -54 — plus the preview math (`percent_to_scale`,
  `preview_rect`, and the v0.4.0 mini-desktop geometry: `fit_box`,
  `scaled_rect`, `panel_rect`).
- The GUI was **not** visually tested (no display in this environment).
  Run it on a real desktop to confirm the window renders.

## Known limits

- Refresh rate is display-only; changing it is not in v0.4.0.
- Multi-monitor arrangement (positioning displays side by side) is not
  in v0.4.0 — use Cinnamon's Display settings for that.
- The autostart file only saves the fit-to-screen transform, not a
  custom resolution.
