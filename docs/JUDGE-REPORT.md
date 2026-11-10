# Vile Judge — Full Audit Report #1

**Judge run:** Nov 10, 2026 ~00:10–00:25 EST (overnight shift, Adnan asleep)
**Judge:** Vile Judge agent (`~/workspace/custom-skills/vile-judge/SKILL.md`, rules 1–9 incl. the edit-worthy bar)
**Scope:** Pieces 1–4, Lock v1/v2, Settings (+ header fix), foot logo v2, design research, BUILD_LOG reconstruction. Piece 5 (top bar / clock / dock) is running in parallel and is NOT judged here.
**Method:** every claimed screenshot checked for existence + `sha256sum` vs the claimed sha, then READ with my own eyes; every behaviour claim checked against code (`compositor/src/main.c`, `settings/vile-settings.py`, harness scripts) and/or visible screenshot evidence; pixel measurements with PIL where a claim was numeric. No evidence = UNPROVEN, not PASS. Nothing was built, fixed, or re-rendered by the judge.

## Verdicts at a glance

| Piece | Verdict | Edit-worthy? |
|---|---|---|
| 1 — Screen | **PASS** | NO — gorgeous wallpaper, but it's just a wallpaper; nothing happening, no desktop, no moment |
| 2 — First window | **PASS WITH NOTES** | NO — empty black terminal + white title bar. Boring |
| 3 — Input | **PASS WITH NOTES** | NO — same terminal with 3 echo lines |
| Lock v2 | **PASS** | **YES** — the one frame that could carry an edit: huge clock, blurred purple glow, clean card |
| Settings (+ header) | **PASS WITH NOTES** | NO — a settings page with a dead empty bottom half; nobody edits a settings page |
| Foot logo v2 | **PASS** | NO as a scene — clean logo asset, not a moment |
| Design research | **PASS** | N/A — a doc, not a look |
| 4 — Tiling | **PASS WITH NOTES** | NO — three empty black terminals with white title bars; purple borders help, white bars dominate |
| BUILD_LOG reconstruction | **PASS WITH NOTES** | N/A — facts preserved, sha record rotted |
| **System as it stands** | **PASS WITH NOTES** | **NO — one great frame (the lock screen), everything else is empty terminals. Piece 5 has to change this.** |

## 1. Piece 1 — Screen — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Boots to Adnan's exact purple Arch screen | `screenshots/vile-screen-01.png`, sha `e585a996…7425e5a` — matches log | PASS | Purple glowing Arch logo centred on dark bg, full-screen, no stock wallpaper, no welcome dialog |
| Pixel diff 0.00 vs wallpaper | PIL check: mean max-diff **0.005**, max **2** over sampled pixels (JPEG noise only) | PASS | Numbers confirm the claim |

**Edit-worthy? NO** — it's a beautiful wallpaper and nothing else. A first-time viewer saves it as a wallpaper; nobody screen-records a static wallpaper to edit over it.
**Flaws:** none. This is the cleanest piece in the system.

## 2. Piece 2 — First window — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Foot maps, prompt visible | `screenshots/vile-screen-02.png`, sha `09e3225d…` — matches Piece 4 regression table | PASS | Foot tiled full-screen (1896x1056), purple border, prompt `root@htch-runtime:~/workspace/vile-os/compositor#`, block cursor |
| Sha changed from `19f985d7…` | File on disk is `09e3225d…`; log explains: tiling replaced the centred 960x600 layout | PASS (expected change) | Layout change is real and explained, not a silent regression |
| Old "centred" claim was exact | Log admits it: window was 26px too low (visible 266..865, not 240..839); Piece 4 fixed the placement | PASS (honestly corrected) | The original claim was wrong and the log says so — good |

**Edit-worthy? NO** — a mostly-empty black terminal filling the screen. The white title bar is the first thing your eye hits.
**Flaws:**
- Foot's **white title bar** ("foot" + min/max/close) — the single biggest look break in the whole system, appears in every desktop shot.
- Locale warning line in yellow at the top: `warning: 'C' is not a UTF-8 locale, using 'C.UTF-8' instead` — environment noise, shows on most first launches (but NOT in tiling-01, so it's inconsistent run to run).

## 3. Piece 3 — Input — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Typing into focused window works | `vile-input-01.png` (sha `52a07059…` — log's `224ad063…` is STALE, file was re-shot in the Piece 4 regression) | PASS | `echo vile` + output `vile` visible, purple border, cursor after prompt |
| Focus follows typing; no leaked `q`, no `nofocus` | `vile-input-02.png` (sha `ae3f94c8…` — log's `860dc3d0…` is STALE) | PASS | `echo vile`/`vile` + `echo yesfocus`/`yesfocus`, NO `nofocus` line, NO leaked `q` |
| Super+Enter opens foot, Super+Q closes | Code: `main.c:3131` (`keybind Super+Enter -> open foot`), `:3138` (Super+Q); tiling-02 shows 2 windows, tiling-03 shows 3 | PASS | Code + visible multi-window states agree |
| Super+L locks, PAM unlock returns to foot | `vile-input-03.png` (sha `d1408d8d…` — log's `4b6c50f6…` is STALE): clean lock 04:44; `vile-input-03-unlocked.png`: foot back with `vile`/`yesfocus`/`backagain` | PASS | Lock is clean (blurred wallpaper only, no terminal); unlock returns to the same terminal |
| Super+Escape clears focus | Code exists: `main.c:3190` + `vile_focus_toplevel(server, NULL)`; harness `run-input-test.sh` clicks (5,540) frame + Super+Escape chord | **UNPROVEN** | The intermediate proof shots (`/tmp/vile-input-02-nofocus.png`, `-escaped.png`, `-refocus.png`) are GONE — /tmp was wiped. Final input-02 does not show the cleared-focus state. Code + harness exist; the visual proof does not survive |
| Super+M / Super+Shift+Q clean exit | Code: `main.c:3146`, `:3156` | **UNPROVEN** | In code only, never screenshot-tested (log admits this) |
| Mouse arrow visible | — | **UNPROVEN** (known gap) | No arrow in any headless grim shot; headless backend never renders one. Needs real DRM on the Dell |

**Edit-worthy? NO** — same empty terminal, three lines of echo output. Nothing to edit over.
**Flaws:**
- `vile-input-03-unlocked.png` is **byte-identical to the refocus shot** (sha `cdcd729d…` in both the log and on disk). The unlock screenshot is not independent evidence — the unlock claim rests on the log line, not the pixels.
- Log shas for input-01/02/03 are all stale (see §9).

## 4. Lock v2 — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Locked: blurred wallpaper ONLY, no windows behind | `vile-lock-10.png` (sha `fc845953…` — log's `685b2c22…` is STALE, re-shot 04:49) | PASS | Huge `04:49`, `Tuesday, November 10`, user card (teal AD avatar — see note), `viletest` + LOCKED + empty field, gear bottom-right, `EN • Vile OS` bottom-centre. No terminal, no title bar, no window edges anywhere |
| Correct password unlocks (real PAM) | `vile-lock-11.png` (sha `faee11df…` — log's `50da49de…` STALE): foot back, tiled, purple border | PASS | Overlay gone, terminal back. Code: `pam_start("vile", …)` + `pam_authenticate` at `main.c:2103–2105`; `pam.d/vile` says no hardcoded password; no hardcoded password found in `main.c` |
| Wrong password stays locked + error | `vile-lock-12.png` (sha `76cf8d2e…` — log's `f307ef1c…` STALE) | PASS | Red field border + red `Wrong password — try again`, still locked |
| Settings panel (gear click) | `vile-lock-13.png` (sha `b205de44…` — matches log) | PASS | Panel on the right: SETTINGS, Hide password toggle ON (purple), Reveal delay 300 ms, SYSTEM: Reboot / Power Off, X close. No terminal behind |
| Windows hidden while locked (not just dimmed) | Code: `wlr_scene_node_set_enabled(..., false)` on toplevel nodes when locking (`main.c:1099`); lock-10/12 show zero bleed | PASS | v1's bleed is gone — compare `vile-lock-03.png` (v1): foot's white title bar visibly cuts across the top behind the `03:33` clock. v2 has none of that |
| Settings is post-login only; lock gear is lock-only | Lock-13 panel contains only Hide password / Reveal delay / Reboot / Power Off — no full Settings | PASS | Rule held |

**Edit-worthy? YES** — the one frame in the whole system that could carry a crazy edit. Huge light clock, blurred purple glow, clean centred card — it looks like a phone lock screen shot, the kind of frame an editor would actually grab. The only thing holding it back is the test username/avatar in the proof shots (`viletest`, AD test picture) instead of Adnan's.
**Flaws / notes:**
- Lock-10/12 show the **AD test avatar** (re-run happened while the test profile was set), not the shipped default purple V. The default-V lock IS proven separately by input-03 (04:44, purple V) — so this is a note, not a fail.
- `Reveal delay 300 ms` is label-only, not a working setting (log admits it).
- Reboot / Power Off were never clicked (deliberately) — unproven on real hardware.

## 5. Settings — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Profile Picture FIRST at top | `vile-settings-01.png` (sha `a623d9ab…` — matches Piece 4 regression table; header table's `6b6fa892…` is STALE) | PASS | Settings right half, dark purple header, Profile Picture first, big purple V avatar, username `root`, Change Picture… (purple, readable) / Remove Picture (dark, readable), then Wallpaper, then About |
| Changing the picture works + live refresh | `vile-settings-02.png` (sha `a78ec386…` — matches regression table) | PASS | Same layout, avatar is now the teal AD circle (orange rim is the test image's own border, not a bug). No restart |
| Lock avatar shares the same file | `vile-settings-03.png` (sha `57ebab0a…` — matches regression table): lock 04:51, avatar IS the AD picture, no terminal behind | PASS | Settings ↔ lock share `~/.config/vile/profile.png` + `~/.face` — proven end to end. Code: `vile_load_profile_avatar` at `main.c:1599`; `set_profile_picture()` in `vile-settings.py:60` writes both paths |
| Picker (click path) opens the real chooser and saves | `vile-settings-picker.png` (sha `29592c4f…` — matches): AD avatar + status `Profile picture updated.` | PASS (save) / **UNPROVEN** (dialog open) | The surviving shot shows Settings AFTER the pick — the chooser itself is not visible. The dialog proof (`/tmp/vile-settings-picker-dialog.png`) is GONE. The layout-aware harness (`tile_origin` helper in `run-settings-picker-test.sh`) exists and the log records computed coords + exit 0, but the visual proof of the dialog opening does not survive |
| Test profile removed after testing; shipped state is default V | `~/.config/vile/` is EMPTY on disk | PASS | Confirmed — no leftover test profile |
| Header fix: dark purple #241640, not white | `vile-settings-04.png` (sha `0bc6a638…` — matches); CSS `headerbar, .titlebar { background-color: #241640; … }` at `vile-settings.py:160–169` | PASS | Top bar is dark purple across the whole strip, `Settings` title in light lavender, dark X. Pixel-measured #241640. `main.c` untouched by this fix (CSS-only) — scope clean |

**Edit-worthy? NO** — it's a settings page. The bottom half of the window is dead empty dark space, and the foot window next to it has a white title bar. Nobody makes a crazy edit over a settings page. The header fix makes it correct, not exciting.
**Flaws:**
- **settings-04 window placement does not match the claimed tile.** Pixels: headerbar `#241640` from **y=0** (touches the screen top edge — no 8px wallpaper frame, no top purple border); window bg from **x=0** (no left frame/border); right edge is a **~28px purple gradient ramp** (x≈1884–1911) and bottom edge a **~28px purple gradient ramp** (y≈1044–1071), not a clean 4px border; wallpaper visible only at x≥1912 and y≥1072. The log claims 1896x1056 at (12,12) with 4px borders. The same app in settings-01 (half tile) IS clean (wallpaper 0–7, short border ramp 8–11, headerbar from 12, bottom ramp 1067–1071). Likely a GTK shadow/maximize interaction with the in-progress Piece 4 binary the header test ran on (the log hints at this) — but as photographed, the header proof shot is not a clean tile. Investigate, don't hand-wave.
- Settings About says `Compositor v0.3.0` while `meson.build` is `0.4.0` — version string drift.
- Username mismatch across shots: Settings shows `root`, lock shows `viletest` (test environment; on Adnan's laptop both must say `adnan` — verify then).
- Settings is not in any launcher/dock yet — launches from a terminal inside Vile (Piece 5 territory).
- Wallpaper section is info-only (`Wallpaper switching comes next`) — compositor hook exists, UI doesn't.

## 6. Foot logo v2 — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Transparent master, no blob, no white bg | `assets/foot-logo.png`, sha `3c65eaeb…89c0b1` — matches | PASS | Crisp blue footprint + cream `>_` + cream `foot`, transparent bg, no blob, no halo |
| Dark tile | `assets/foot-logo-dark.png`, sha `c4a0ae49…278b51` — matches | PASS | Same logo on dark rounded-square #0F0B1A tile, rounded corners, reads clearly |
| 512 / 256 sizes | `foot-logo-512.png` sha `13a2cea1…`, `foot-logo-256.png` sha `c8790690…` — both match | PASS | At 256px the footprint, `>_` and `foot` are all still legible |
| v1 blob version kept for history | `foot-logo-blob-v1.png`, `foot-logo-dark-blob-v1.png` present; `foot-logo-clear-source.jpg` is the untouched new source | PASS | History kept, live files are the clear version |
| `foot.desktop` points at the right icon | `Icon=/usr/share/icons/vile/foot-logo.png`, unchanged | PASS | Checked by hand (no `desktop-file-validate` on this VM — log admits this) |

**Edit-worthy? NO as a scene** — it's a clean logo asset, not a moment. As a logo it's good: crisp, legible at 256px, on-token colours.
**Flaws:** none in the files. Not yet seen in a running foot title bar or dock (Piece 5).

## 7. Design research — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| 6 OSes researched, concrete steals for Piece 5, do-not-copy list | `design-research.md`, 154 lines, read in full | PASS | macOS Tahoe, Windows 11, GNOME, KDE Plasma 6, Hyprland rices, ChromeOS — each with specific, usable patterns; top 5 steals and do-not-copy list are concrete and on-brief (slim 3-zone bar, floating pill dock, huge desktop clock, sidebar Settings, one-screen lock) |
| Sources honest | Sources section lists article URLs; video section admits "from search results and their published descriptions/transcripts. No video was played in a browser" | PASS | Honest about its own limits. No invented stats, no fake data |
| No mockups built from it | No HTML/preview in the doc; doc is text only | PASS | True for the doc itself (see stray-file note in §9) |

**Edit-worthy? N/A** — a research doc, not a look.
**Flaws:** video claims are second-hand by the doc's own admission — fine for research, but Piece 5 shouldn't treat a transcript summary as a design fact without a second look.

## 8. Piece 4 — Tiling — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| 1 window fills tile, 4px purple border | `vile-tiling-01.png`, sha `9bda037a…` — matches | PASS | 1 foot, prompt, purple border. Pixel-measured col x=1000: wallpaper 0–7, purple (110,74,255) 8–11, title 12–37, content 38–1067, purple 1068–1071, wallpaper 1072–1079 — exactly 4px on all four sides |
| 2 windows side by side, focused purple / unfocused dim | `vile-tiling-02.png`, sha `c1960cd2…` — matches | PASS | Left dim border, right purple border with `echo second`/`second`, no overlap, wallpaper glow in the middle gap |
| 3 windows, master left + right stack, focus moves with Super+J/K | `vile-tiling-03.png`, sha `df9b9fcb…` — matches | PASS | Master left full height (dim), right-top focused purple (`echo second` + `echo focus2`), right-bottom `echo third`. No overlap. Focus followed the keybind |
| 26px shift bug fixed | Pixel runs above (tiling-01) + log's before/after measurement | PASS | Confirmed by measurement, not by eyeballing |
| Fullscreen toggle + restore + re-tile on close | Log claims `/tmp/vile-tiling-fullscreen.png` (`77426d1d…`), `-restored.png` (byte-identical to tiling-03), `-retile.png` (`710daeed…`) | **UNPROVEN** | All three files are GONE from /tmp. Code for the fullscreen branch + re-enable fix exists in `main.c` (`:734–736`, `:786–793`), but no surviving screenshot proves fullscreen, restore, or re-tile |
| Wallpaper hook (`~/.config/vile/wallpaper`, `VILE_WALLPAPER` wins) | Log claims `/tmp/vile-tiling-wallpaper.png` (`ee1a3568…`), corner pixels match nebula | **UNPROVEN** | File is GONE from /tmp. Hook code exists in `main.c` (header comment + loader); the nebula proof does not survive |
| Super+Escape + frame-click focus-clear (the two fixed FAILs) | Code: `main.c:3190` + harness `run-input-test.sh` now clicks (5,540); picker harness is layout-aware (`tile_origin` helper) | **UNPROVEN** (visual) | Intermediate shots gone (see §3). The layout-aware picker harness code is real and the picker save is proven (§5); the focus-clear visual proof is not |
| Full regression, all exit 0 | Regression table in BUILD_LOG (Piece 4 section) — its shas for settings-01/02/03 (`a623d9ab…`/`a78ec386…`/`57ebab0a…`) MATCH the files on disk | PASS (as recorded) | The table is internally consistent with the current files; the earlier sections are not (see §9) |
| 4+ window layout | — | **UNPROVEN** | No screenshot. The master-stack rule covers it in theory; nothing proves it |

**Edit-worthy? NO** — three empty black terminals with white title bars. The purple focus borders are the only Vile thing visible, and the white title bars drown them out. This is the least edit-worthy set in the audit.
**Flaws:**
- White title bars on all three windows (same §2 flaw, tripled).
- The middle gap in tiling-02 shows a thin bright vertical sliver (wallpaper glow through the 12px gap) — by design, but it reads as a stray light line at a glance.

## 9. BUILD_LOG.md — reconstruction audit — PASS WITH NOTES

The file is 668 lines with the RECONSTRUCTION NOTE at the top (844 → 177 → rebuilt to 668). Facts, timestamps and screenshot paths survived. But the **sha record rotted**: re-shot screenshots were never re-recorded in the middle sections, so the log now contradicts the files on disk.

| Log claim | File on disk | Result |
|---|---|---|
| Lock v2: lock-10 `685b2c22…`, lock-11 `50da49de…`, lock-12 `f307ef1c…` | `fc845953…`, `faee11df…`, `76cf8d2e…` | **STALE** — files re-shot in the Piece 4 regression (04:49), log section never updated |
| Piece 3: input-01 `224ad063…`, input-02 `860dc3d0…`, input-03 `4b6c50f6…` | `52a07059…`, `ae3f94c8…`, `d1408d8d…` | **STALE** — re-shot 04:43–04:44 |
| Header verification table: input-01 `011f0208…`, settings-01 `6b6fa892…`, settings-02 `801d5d74…`, settings-03 `cd1a5409…` | `52a07059…`, `a623d9ab…`, `a78ec386…`, `57ebab0a…` | **STALE** — superseded by the Piece 4 regression table, which DOES match |
| Piece 1 `e585a996…`, tiling-01/02/03, settings-04 `0bc6a638…`, picker `29592c4f…`, foot logo v2 (all 4 shas) | All match | PASS |
| Foot logo v1 section shas `da1f36db…` / `8b96b57d…` | Files overwritten by v2 (`3c65eaeb…` / `c4a0ae49…`) | PASS (historical — log explains the rework) |
| `main.c` ~2680 lines, meson 0.4.0 | Actual 2695 lines, meson `0.4.0` | PASS |

**Stray file (not used as proof, but it must not be mistaken for one):** `render_preview.py` + `vile-desktop-preview.png` (Nov 10 00:42) in the project root — an HTML/Playwright **mockup** of the v1 desktop (Waybar + eww + nwg-dock). It is not referenced in BUILD_LOG as evidence for any wlroots piece, but a mockup sitting in the project root next to real grim screenshots is a standing-rules violation waiting to happen. Move it out of `~/workspace/vile-os/` or label it clearly as a mockup. (The `verification/` folder's v1 sway screenshots are a different track — fine, just don't confuse them with wlroots proof.)

## Flaws found (complete list)

1. **Foot's white title bar** — every desktop shot (screen-02, input-01/02/unlocked, lock-11, settings-01/02/picker, settings-03-unlocked, tiling-01/02/03). The #1 look killer and the #1 reason nothing desktop-side is edit-worthy.
2. **BUILD_LOG sha rot** — stale shas for lock-10/11/12, input-01/02/03, settings-01/02/03 (header table). The log is no longer a reliable sha source of truth.
3. **/tmp evidence loss** — Super+Escape states, fullscreen/restore/retile, nebula wallpaper hook, picker dialog: all intermediate proof lived in /tmp and is gone. Those claims are now UNPROVEN.
4. **settings-04 placement anomaly** — headerbar touches screen top (y=0), no top/left border, ~28px purple gradient ramps on right/bottom; claimed tile not what pixels show.
5. **Mouse arrow unproven** — no arrow in any headless shot; whole system headless-only so far.
6. **Locale warning line** in foot on most first launches (inconsistent — present in screen-02/input/settings shots, absent in tiling-01).
7. **Settings About version drift** — `Compositor v0.3.0` vs meson `0.4.0`.
8. **Username mismatch** — Settings `root` vs lock `viletest` (test env; must be `adnan` on his laptop).
9. **input-03-unlocked byte-identical to the refocus shot** — unlock screenshot is not independent evidence.
10. **Reveal delay 300 ms** is label-only, not a working setting.
11. **Reboot / Power Off never clicked** — unproven (deliberately, but still unproven).
12. **Super+M / Super+Shift+Q** in code only, never screenshot-tested.
13. **No 4+ window layout screenshot.**
14. **Settings wallpaper section is info-only** — hook exists, switching UI doesn't.
15. **Lock v2 proof shots show the AD test avatar**, not the shipped default V (default-V lock proven separately by input-03).
16. **Stray mockup** `render_preview.py` / `vile-desktop-preview.png` in the project root.

## Must-fix before shipping (in order)

1. **Foot's white title bar.** Patch foot's theme/config or add server-side decorations. Without this, no desktop screenshot is edit-worthy and the whole "premium dark OS" claim falls apart the second a window opens. This is the look blocker above all others.
2. **Repair the audit trail.** Update BUILD_LOG's stale shas (lock-10/11/12, input-01/02/03, settings-01/02/03 header table) to the current on-disk values, and make it a rule: proof screenshots live under `screenshots/`, never only in /tmp. A log that contradicts its own files can't be trusted by the next judge.
3. **Re-prove the lost claims with durable screenshots.** Super+Escape focus-clear (nofocus/escaped/refocus states), fullscreen + restore + re-tile, nebula wallpaper hook, picker dialog mid-open. Save them under `screenshots/`. Until then they are UNPROVEN, and the next judge will say so again.
4. **Investigate the settings-04 placement anomaly.** Headerbar at y=0, no top/left border, ~28px purple gradient ramps on right/bottom. Find whether it's the GTK shadow, a maximize flag, or the tiling code — and re-shoot the header proof on the fixed binary. Don't ship a proof shot whose geometry contradicts its own caption.
5. **Run it on real hardware.** Mouse arrow, DRM/GLES2, real keyboard/mouse, Reboot/Power Off, the Dell 7400 + Westinghouse TV — none of it is proven anywhere in this audit. The headless proof is strong, but Adnan's laptop is the machine that matters, and the v1 hyprlock freeze is exactly the kind of thing headless testing can't catch.

## Nice-to-have

- Set `LANG=C.UTF-8` (or equivalent) in the startup path so the locale warning never appears in a screenshot again.
- Fix the Settings About version string to `0.4.0` (or read it from one source of truth).
- Make `Reveal delay 300 ms` a real setting or drop the row.
- Wallpaper switching UI in Settings (the compositor hook is already there).
- A 4+ window tiling screenshot.
- Move `render_preview.py` / `vile-desktop-preview.png` out of the project root.
- On the Dell run, confirm both Settings and the lock show `adnan`, not `root`/`viletest`.

## Still not done

- **Piece 5** (top bar, huge centred desktop clock, dock) — running in parallel, NOT judged here. This is the piece that has to make the desktop edit-worthy; as of this audit the desktop is not.
- **Piece 6** (pure Arch ISO + QEMU boot screenshot).
- Settings launcher/dock wiring (opens from a terminal today).
- More Settings sections (Wi-Fi, display/ScreenFit, sound, lock settings).
- Real-hardware gate for everything (see must-fix #5).
- Adnan's own profile photo (lock avatar is the purple V placeholder until he sets one in Settings, post-login).

## Judge's bottom line

The engineering is real: a from-scratch wlroots compositor that boots to the right screen, hosts windows, takes input, tiles, locks with real PAM, and shares a profile picture between Settings and the lock — almost all of it proven by real grim screenshots I read myself, with pixel measurements backing the numeric claims. The two original FAILs from the header verification run (focus-clear, picker coords) were genuinely fixed in code, and the log is honest about what it doesn't prove.

But two things are not good enough yet. **The look:** only the lock screen is edit-worthy; the desktop is empty black terminals with white title bars, and Piece 5 has to fix that or the "so good an editor makes a crazy edit on it" bar fails. **The evidence trail:** the log's shas rotted when files were re-shot, and every intermediate proof shot lived in /tmp and is now gone — so four claims I would otherwise pass are UNPROVEN on the surviving evidence. Fix the title bar, fix the log, re-prove the lost claims with durable screenshots, and then the system is shippable for a first Dell run.
