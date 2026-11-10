# Vile Judge — Audit Report #2

**Judge run:** Nov 10, 2026 ~00:50–01:05 EST (overnight shift, Adnan asleep)
**Judge:** Vile Judge agent (`~/workspace/custom-skills/vile-judge/SKILL.md`, rules 1–9 incl. the edit-worthy bar)
**Scope:** NEW since audit #1 — Piece 5 (top bar / huge desktop clock / dock / system menu, compositor v0.5.0), the dock fix, the audit-trail repair, re-check of audit #1 must-fix items 1, 2, 4, and regressions on Pieces 1–4, Lock v2, Settings.
**Method:** every claimed screenshot checked for existence + `sha256sum` vs the claimed sha, then READ with my own eyes; behaviour claims checked against code (`compositor/src/main.c`, `settings/vile-settings.py`, `foot.ini`, harness scripts) and/or visible evidence; pixel measurements with PIL where a claim was numeric. No evidence = UNPROVEN, not PASS. Nothing was built, fixed, or re-rendered by the judge.

## Verdicts at a glance

| Piece | Verdict | Edit-worthy? |
|---|---|---|
| Piece 5 — Vile look | **PASS WITH NOTES** | **YES** — the bare desktop (look-01) is a poster: huge glowing clock over the glowing logo, slim bar, floating dock. First desktop frame that clears the bar |
| Dock fix | **PASS** | N/A as a standalone piece (one-line fix) — but it COMPLETED the poster frame; without it look-01 had no dock |
| Audit-trail repair | **PASS WITH NOTES** | N/A — a doc fix, not a look. The repair itself was real; the rot pattern repeated anyway (see §3) |
| Must-fix #1 re-check (white title bar) | **FIXED** | — |
| Must-fix #2 re-check (audit trail) | **PARTIALLY FIXED** | — |
| Must-fix #4 re-check (settings-04 anomaly) | **RESOLVED** | — |
| Regressions (Pieces 1–4, Lock, Settings) | **PASS** | — |
| **System as it stands** | **PASS WITH NOTES** | **YES — narrowly. Two edit-worthy frames now (lock + bare desktop). The tiled-terminal state is still not edit-worthy on its own, but it is no longer the first thing you see.** |

## 1. Piece 5 — Vile look — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Top bar: 32px, translucent dark, 3 zones, never white | `vile-look-01.png` sha `4dfa790e…b39b42c` — matches dock-fix section | PASS | Slim dark bar: purple V + "Vile" + pills 1 (filled) / 2 / 3 left; "Tue 10 Nov 05:43" centre; wifi/volume/battery/power icons right. Legible, no white |
| Huge desktop clock, same component as lock clock, no box, behind windows | look-01 | PASS | "05:43" ~140px Inter Light with soft purple halo, "Tuesday, November 10" below, glowing Arch logo under it. Clock hidden behind the tiled foot in look-02 — correct layering |
| Dock: floating pill, 152x80 at (884,988), foot + purple V, running dots, click to open/focus | look-01 (no dots — nothing running) + look-02 (dot under foot) + dock-fix pixel check, re-measured by me: dock region (45.3,39.3,80.2) vs wallpaper band (43,45,58); V icon (110,74,255) = #6D4AFF; foot icon (248,243,238) cream | PASS | Floating dark pill, foot tile + purple V, no dots on the bare desktop, dot under foot with a window open. Visible in every current shot |
| System menu: one dark card, real Wi-Fi/battery, Lock/Settings real, Power Off never clicked | `vile-look-menu-final.png` sha `299703c9…24ecc` — matches | PASS | Flush-right dark card: SYSTEM, Wi-Fi Unavailable, Volume slider 60% (purple fill + white knob), Battery No battery, Lock / Settings / Power Off. No bleed — brightest pixel in the top strip (54,54,66). Foot close button does not peek out |
| Dock click opens foot (real click, log line) | look-02 sha `59c5b6a4…c8377` — matches dock-fix section; log claimed `Vile: dock: open foot` | PASS | One tiled foot, dark title bar, purple border, bar + dock visible, running dot under foot |
| Dock close/reopen (Super+Q, dock click again) | look-03 sha `59c5b6a4…` — byte-identical to look-02 | **PASS (log only)** | look-03 is NOT independent evidence — same empty foot, same minute, identical bytes. The cycle is proven by the log line, not the pixels |
| Tiling area 1896x932 at (12,44) | tiling-01 (new) — measured: border y40–43, title y44–69 | PASS | Geometry change is real and consistent across all current shots |
| Edit-worthy? (Adnan's bar) | look-01, read by me | **YES** | Huge glowing clock + logo, one accent, no clutter, no white, slim expensive bar, floating dock. A frame someone would screen-record and edit over. Nits: date line touches the logo apex (builder admitted it); the dock is the quietest element |

**Flaws:**
- Date line just touches the logo's apex in look-01 (the "N" of November sits on the purple). Reads as a deliberate poster overlap, but it is a touch, not a gap.
- Volume slider is visual-only (not wired to audio); workspace pills are visual-only (switching not built). Log admits both.
- `vile-look-menu-final.png` still shows the locale warning line in yellow in the foot window behind the menu — that shot predates the `LC_ALL=C.UTF-8` harness fix. Newer shots are clean; this one is a stale artifact, not a live flaw.
- The two bonus proofs for this piece (`/tmp/vile-look-settings.png`, `/tmp/vile-look-settings-alone.png`) live ONLY in /tmp — see flaw #2 in §5.

## 2. Dock fix — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Root cause: dock only re-rendered on running-set change; at startup the set is already empty, so `vile_shell_render_dock()` never ran | `main.c:2931` — one call `vile_shell_render_dock(server);` in `vile_shell_create()` after `vile_shell_fit()`; update-path call still at `:2806` | PASS | Code matches the story exactly. One call, scoped, nothing else changed |
| Dock now visible on the bare desktop | look-01 `4dfa790e…` + my own pixel re-measure (see §1) | PASS | Dock pill, foot icon, purple V, no running dots — read with my own eyes |
| Regressions re-run, exit 0 | Dock-fix section: tiling-01 `4ed33062…`, -02 `fc5390a1…`, -03 `2f3dbf29…`, fullscreen `abd4ec86…`, restored `2f3dbf29…` (= tiling-03), retile `40093798…`, nebula `0cdb89ba…` — ALL match disk (my sweep) | PASS | Every sha in the dock-fix section matches the files on disk. Tiling-01 dock region (45.8,39.7,81.6) vs band (43,45,58) per the section; running dot (110,74,255) under foot |

**Flaws:** none in the fix itself. Note: look-02/03 are identical again after this fix's re-run (same pattern as Piece 5's look-03).

## 3. Audit-trail repair — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| Stale shas corrected (lock-10/11/12, input-01/02/03, settings-01/02/03 header table, foot-logo-dark tail) | Correction table (BUILD_LOG lines 724–987) vs my sha sweep | PASS | All 13 corrected values match disk: lock-10 `1af4dfbe…093a93`, lock-11 `9afa92e7…ea4a75`, lock-12 `76cf8d2e…c7b9f0`, input-01 `e9a2872d…bc67a7`, input-02 `88bb4d04…5545b4`, input-03 `7d0458aa…e77b6f`, settings-01 `feefc807…4739bf`, settings-02 `7d6158cb…81a3d4`, settings-03 `b1b86393…7afa1d` |
| Super+Escape focus-clear re-proved durably | `vile-input-02-nofocus.png` `6d2d848e…657706`, `vile-input-02-escaped.png` `bb0bf6cf…8b42f7`, `vile-input-02-refocus.png` `f36113bc…e57673` — all match disk, all read by me | PASS | nofocus: only `echo vile`/`vile`, NO `nofocus`, border DIM. escaped: `echo yesfocus` present, NO `escaped`, border DIM. refocus: `echo backagain` appears, purple border |
| Fullscreen / restore / re-tile re-proved durably | `vile-tiling-fullscreen.png` `abd4ec86…f7766c`, `vile-tiling-restored.png` `2f3dbf29…` (= tiling-03), `vile-tiling-retile.png` `40093798…` — all match disk; fullscreen + retile read by me | PASS | Fullscreen: foot fills 1920x1080, no bar/dock/borders, `echo second` + `echo focus2` visible. Restored is byte-identical to tiling-03 (both `2f3dbf29…`) — restore proven by identity, not by a distinct shot. Retile: 2 windows side by side after close (sha matches; content per log) |
| Nebula wallpaper hook re-proved durably | `vile-tiling-wallpaper-nebula.png` `0cdb89ba…` — matches disk, read by me | PASS | Single tiled foot, dark title bar, top bar + dock; nebula visible in the strip below the dock. Thin proof by design (the tiled window covers the rest) — the hook works, the shot barely shows it |
| Picker dialog mid-open re-proved durably | `vile-settings-picker-open.png` `f1563a1e…b688f4` + `vile-settings-picker.png` `ed6e4f79…d9fd5e` — both match disk, both read by me | PASS | Real GTK "Choose profile picture" chooser open, tiled bottom-right, dark purple header, Cancel/Open, Home file list, Images filter. Dock shows BOTH running dots. Pick saved (AD avatar + "Profile picture updated.") |
| Picker click regression (Piece 5) found by probe and fixed | Probe script `compositor/tests/probe-remove-click.sh` present; picker-open shot proves clicks now reach GTK buttons | PASS | The regression was real (dead-centre Remove click did nothing on the pre-fix binary) and the fix is proven by the working picker, not just claimed |
| Settings About version fixed | `vile-settings.py:300` = `Compositor v0.5.0`; `meson.build` version `0.5.0`; visible as "Compositor v0.5.0 • wlroots 0.19.3 • Wayland" in settings-02 | PASS | Version drift closed |
| Stray mockup moved | `archive/` contains `README.md`, `render_preview.py`, `vile-desktop-preview.png`; project root listing shows neither file | PASS | Mockup is out of the root and labelled, not deleted |
| Test-env rot repaired (viletest, pam.d, python3-gi, test avatar) | `~/.config/vile/` is EMPTY (shipped state = default V); Settings launches in the shots above | PASS | Environment restored; shipped state clean |

**Flaws:**
- **The rot repeated.** The audit-fix stamp (05:37 UTC, "32 shas, 0 mismatches") is already stale for the files the dock-fix re-shot: Piece 5 section's look-01/02/03 (`7923f174…`/`2194db81…`) and tiling-01/02/03 (`05304feb…`/`c347a91d…`/`543d8e39…`) no longer match disk, and the audit appendix's own tiling-03/restored (`543d8e39…`), retile (`d4270925…`) and nebula (`3213806f…`) values are also superseded. The CURRENT values all live in the dock-fix section and all match — but the log now has three sections that disagree with the files. A one-off correction cannot keep up with re-shoots; the log needs a single current-sha table re-stamped per run.
- **Timestamp muddle in the appendix.** The input re-run is labelled "05:15 UTC" but the nofocus/escaped shots show a 05:33 clock — the files on disk are from Piece 5's parallel re-runs (05:30–05:36), which the appendix itself notes. Content is proven; the run attribution is muddled.
- `vile-input-03-unlocked.png` is byte-identical to `vile-input-02-refocus.png` AGAIN (`f36113bc…`). Unlock is proven by sequence + log line, not by unique pixels. Same pattern as look-03.
- `vile-look-settings-alone.png` (the judge-item-2 re-proof) and `vile-look-settings.png` live ONLY in /tmp — the look harness writes them there (`run-look-test.sh` lines 45–46). The exact failure mode audit #1 flagged, repeated by Piece 5's own harness.

## 4. Re-check of audit #1 must-fix items 1, 2, 4

| Item | Result | Evidence |
|---|---|---|
| #1 Foot's white title bar | **FIXED** | My own pixel measurement on current `vile-tiling-01.png`, col x=1000: wallpaper (43,45,58) y38–39, purple border (110,74,255) y40–43, title bar exactly (36,22,64) = #241640 y44–69, terminal (15,11,26) from y70. Seen dark in look-02, menu-final, tiling-01, nebula, picker-open, settings-02. Root cause (foot csd colours are AARRGGBB; `color=241640` parsed as alpha 00) is documented in `foot.ini` with the `ff`-prefixed fix. Caveat, unchanged: the fix only applies when `foot.ini` is installed (`~/.config/foot/foot.ini`); a fresh foot without it is white again. Piece 6 must bundle it |
| #2 Repair the audit trail | **PARTIALLY FIXED** | The corrections were real and every corrected sha verified — but the pattern repeated within the hour (see §3). The log still contradicts itself across sections. Needs a process fix, not another correction pass |
| #4 settings-04 placement anomaly | **RESOLVED** | My own pixel measurement on `/tmp/vile-look-settings-alone.png`, col x=1000: border band y40–43 (anti-aliased purple (96,64,222)/(93,63,216)/(91,61,211)/(68,46,158)), headerbar first appears at y=44 exactly (36,22,64), row y=44 is (36,22,64) across x=100–1500, wallpaper strip at x=5, content at x=12. The old pre-fix-geometry explanation is consistent with the evidence. Caveat: the re-proof shot is /tmp-only (see §3) |

## 5. Regressions — PASS

| Claim | Evidence | Result |
|---|---|---|
| Piece 1 byte-identical | `vile-screen-01.png` `e585a996…7425e5a` — matches | PASS |
| Piece 3 input still passing | nofocus/escaped/refocus read by me (§3); input-01/02/03 shas match | PASS |
| Piece 4 tiling on new geometry | tiling-01 `4ed33062…`, -02 `fc5390a1…`, -03 `2f3dbf29…` — all match dock-fix section; tiling-01 read by me | PASS |
| Lock v2 current | lock-10 `1af4dfbe…` — read by me: clean lock 05:34, blurred purple wallpaper, huge clock, default purple V avatar (not the AD test avatar), `viletest` + LOCKED, no terminal behind | PASS |
| Settings + picker still passing | settings-02 read by me (§3); picker-open read by me (§3) | PASS |
| Mouse arrow | Still UNPROVEN (pre-existing gap, headless renders no cursor) | UNPROVEN |

## Flaws found (complete list, new + carried)

1. **GTK file chooser is white** — in `vile-settings-picker-open.png` the chooser's file list is a big white Adwaita rectangle in the middle of a dark OS. Header is dark purple, list is white. This is now the #1 look break in the system, replacing the fixed title bar.
2. **Look-harness proof shots are /tmp-only** — `run-look-test.sh` saves `vile-look-settings.png` and `vile-look-settings-alone.png` to /tmp. The judge-item-2 re-proof will vanish on the next /tmp wipe, exactly as audit #1's evidence did.
3. **BUILD_LOG sha rot, second occurrence** — Piece 5 section + audit appendix already stale for files the dock-fix re-shot. No single current-sha table; older sections contradict the files.
4. **look-03 and input-03-unlocked are not independent evidence** — byte-identical to look-02 and refocus respectively. Dock close/reopen and unlock rest on log lines + sequence, not pixels.
5. **Audit appendix timestamp muddle** — input run labelled 05:15 UTC, shots show a 05:33 clock (files are Piece 5's parallel re-runs).
6. **Settings bottom half is dead empty dark space** — `vile-look-settings-alone.png`: Profile Picture / Wallpaper / About end at ~y=760, the rest is empty. Same as audit #1.
7. **Date touches the logo apex** in look-01 (builder admitted).
8. **Nebula proof is thin** — nebula visible only in the ~12px strip below the dock; the tiled window covers the rest by design.
9. **menu-final is a stale shot** — shows the locale warning (pre-LC_ALL-fix). Newer shots are clean.
10. **Obsolete `vile-setup.sh` (v1) still in the project root** — could mislead someone into running the dead Hyprland-era installer. Archive it.
11. **Carried from audit #1, still open:** mouse arrow unproven; real-hardware gate (DRM/GLES2, Dell/TV run, Reboot/Power Off unclicked); Super+M / Super+Shift+Q code-only, never screenshot-tested; no 4+ window shot; Settings wallpaper section info-only; Reveal delay 300 ms label-only; usernames `root`/`viletest` in test shots (must be `adnan` on his laptop).

## Must-fix before shipping (in order)

1. **Dark-theme the GTK file chooser** (Adwaita-dark for Settings/chooser, or a chooser theme). The white file list is the biggest white surface left in the system and it appears in the middle of the profile-picture flow — the exact flow Adnan will use.
2. **Make ALL proof shots durable.** Change `run-look-test.sh` to save the settings + settings-alone shots under `screenshots/`, and re-shoot the judge-item-2 proof there. Audit #1 set this rule; Piece 5's harness broke it again.
3. **One current-sha table in BUILD_LOG.** A single table, re-stamped after every harness run (or a script that regenerates it from disk). Stop hand-maintaining shas in five sections that rot on the next re-shoot.
4. **Real-hardware gate.** Mouse arrow, DRM/GLES2, real keyboard/mouse, Reboot/Power Off, Dell 7400 + Westinghouse TV. Unchanged from audit #1 — still the gate that matters most.
5. **Independent evidence for look-03 and input-03-unlocked.** Shoot them at a different minute and/or with different terminal content so they are not byte-identical to their siblings.

## Nice-to-have

- Nudge the desktop date down (or the clock up) so it clears the logo apex.
- Fill the Settings bottom half (more sections, or move About up).
- 4+ window tiling screenshot; Super+M / Super+Shift+Q screenshot test.
- Archive `vile-setup.sh` out of the project root.
- Wire the volume slider to real audio; make workspace pills switch.
- Wallpaper switching UI in Settings (hook already exists).
- Make Reveal delay a real setting or drop the row.
- On the Dell run, confirm Settings and the lock both show `adnan`.

## Still not done

- **Piece 6** (pure Arch ISO + QEMU boot screenshot) — and it must bundle `foot.ini`, or the title bar goes white again on a fresh install.
- Real-hardware gate for everything (see must-fix #4).
- Adnan's own profile photo (lock avatar is the purple V placeholder until he sets one in Settings, post-login).

## Judge's bottom line

Piece 5 delivered the look, and the dock fix completed it: the bare desktop is now a genuine poster frame — huge glowing clock, glowing logo, slim bar, floating dock — and it clears Adnan's edit-worthy bar. The lock screen was already there; now there are two frames an editor could build on. The white title bar is really gone (I measured it myself: exactly #241640), and the settings-04 anomaly is really resolved (headerbar at y=44, exactly #241640).

But the system has a new biggest white surface — the GTK file chooser — and the evidence discipline that audit #1 demanded is already slipping again: Piece 5's own harness writes its two bonus proofs to /tmp, and the build log's older sections are stale again an hour after the audit-trail repair. Fix the chooser theme, make every proof durable, give the log one sha table that a script maintains — then the next judge has nothing to complain about except the hardware gate, which only Adnan's Dell can close.
