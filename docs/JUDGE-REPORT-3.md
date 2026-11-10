# Vile Judge — Audit Report #3

**Judge run:** Nov 10, 2026 ~03:40–04:00 EST (overnight shift, Adnan asleep)
**Judge:** Vile Judge agent (`~/workspace/custom-skills/vile-judge/SKILL.md`, rules 1–9 incl. the edit-worthy bar)
**Scope:** NEW since audit #2 — the overnight polish round (dark GTK chooser, durable proof shots, sha-table script, independent look-03 / input-03-unlocked), Piece 6 v0.5.0 (pure Arch ISO + QEMU boot test), Piece 6 v2 v0.5.1 (rebuild, foot fix verified in a booted ISO), and regressions (Piece 1, edit-worthy re-check).
**Method:** every claimed screenshot checked for existence + `sha256sum` vs the claimed sha, then READ with my own eyes; behaviour claims checked against code and/or visible evidence; pixel measurements with PIL where a claim is numeric. The `refresh-sha-table.sh` script was re-run by me and its output diffed against the log's table. No evidence = UNPROVEN, not PASS. Nothing was built, fixed, or re-rendered by the judge.

## Verdicts at a glance

| Piece | Verdict | Edit-worthy? |
|---|---|---|
| Overnight polish | **PASS WITH NOTES** | N/A — process + theme fixes, not a look. But the chooser fix removes the last big white surface |
| Piece 6 — ISO v0.5.0 | **PASS WITH NOTES** | **YES** — boot-09 is a third poster frame: Vile running from its own ISO, visible mouse arrow |
| Piece 6 v2 — ISO v0.5.1 | **PASS** | N/A as a look (same desktop) — the foot fix is proven in a booted ISO |
| Regressions | **PASS** | — |
| **System as it stands** | **PASS WITH NOTES** | **YES — three edit-worthy frames now (lock + bare desktop + QEMU desktop). The QEMU frame has the date overlapping the logo apex, which is the one visible wart.** |

## 1. Overnight polish — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| GTK file chooser dark-themed (`gtk-application-prefer-dark-theme` + CSS in `vile-settings.py`) | `settings/vile-settings.py:415` has the property set; `vile-settings-picker-open.png` sha `12d73581…0774` — matches the 08:22 table | PASS | Real GTK "Choose profile picture" chooser, fully dark: dark file list, purple selected "agents" row, dark sidebar (Home selected), dark header, dark filter strip. No white surface |
| File list bg (15,11,26) = #0F0B1A; header (36,22,64); only #EDE9FE glyphs near-white | My own pixel measurement: list region mean (30.3,23.6,56.9); header px (36,22,64) exact; selected row px (109,74,255) = #6D4AFF; near-white px 3,369 of 429,975 (0.78%) scattered = glyphs/strokes, not a surface | PASS | Numbers confirm the claim. The brightest pixel in the chooser is (255,255,255) — a glyph/stroke, not a rectangle |
| Pick still works end to end (AD avatar saved) | `vile-settings-picker.png` `5fca9ed3…46c4` — matches table (sha only; content per log) | PASS (sha) | I did not re-read picker.png this run; the open-chooser shot + the log's save claim + the picker harness exit 0 are the evidence. The open shot proves the chooser works |
| All proof shots durable — look harness saves 6/6 shots under `screenshots/`; no /tmp proof PNGs in any harness | `run-look-test.sh:48–53` — all six OUT paths under `screenshots/`; my grep of all `run-*.sh` / `probe-*.sh` for `/tmp/*.png` paths: none | PASS | The durability fix is real in code, not just claimed. This closes audit #2 must-fix #2 |
| One current-sha table, script-maintained | `compositor/tests/refresh-sha-table.sh` exists; I re-ran it — output is byte-identical to the log's 08:22 table (55 files, same shas, same order) | PASS | The process fix works: the table is regenerable and matches disk. This closes audit #2 must-fix #3 |
| look-03 independent (echo reopened) | `vile-look-03.png` `658ac572…a380` — matches; sha differs from look-02 `fd74145f…`; read by me | PASS | Reopened foot shows `echo reopened` / `reopened`, dark title bar, purple border, dock with running dot. Independent evidence at last |
| input-03-unlocked independent (echo unlocked) | `vile-input-03-unlocked.png` `4634ce71…2b1c` — matches; sha differs from refocus `8ebea335…`; read by me | PASS | Foot back after unlock showing `echo vile`/`vile`, `echo yesfocus`/`yesfocus`, `echo backagain`/`backagain`, plus `echo unlocked`/`unlocked`. Independent evidence at last |
| Scope: polish edited `vile-settings.py` only, not `main.c` | mtimes: `vile-settings.py` 05:53 UTC (polish session); `main.c` 05:42 UTC (dock-fix era, untouched since) | PASS | Scoped correctly |

**Edit-worthy? N/A** — but the chooser fix matters for the bar: the profile-picture flow no longer flashes a white rectangle mid-edit.

**Flaws:**
- `foot.ini` comment (line ~25) still says "Fix NOT yet verified in a booted ISO — verify on the next rebuild." That is now false — the fix WAS verified in the 0.5.1 boot. Stale comment, one line.
- `vile-settings-picker.png` was not re-read by me this run (sha-verified only). Minor; the open-chooser shot carries the proof.

## 2. Piece 6 — ISO v0.5.0 — PASS WITH NOTES

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| ISO built (mkarchiso, exit 0), 1.4 GB, Volume ID VILE_OS, PVD matches file size | 0.5.0 ISO is DELETED (verified: `iso/` contains only 0.5.1) — sha/size cannot be re-checked on disk. Log's v2 section claims 1,482,342,400 bytes, sha `bb217ef0…`, 723,800 sectors | PASS (log) | The file is gone by design (superseded). The claim rests on the log + the fact that the v2 ISO was built from the same work dir. The boot shots below are the surviving proof it existed and worked |
| Kernel + initramfs, all archiso hooks run, VILE_OS + loop0 mounted | `vile-iso-boot-01.png` `8110e773…e454` — matches the 08:22 table (converted PPM→real PNG); read by me | PASS | Text-mode boot: "running hook [archiso]", "[archiso_loop_mnt]", "[archiso_pxe_common/nbd/http/nfs]", "Mounting '/dev/disk/by-label/VILE_OS' … mounted successfully", "Mounting '/dev/loop0' … mounted successfully". The v1 Switch Root failure is gone |
| Switch Root succeeded, systemd booting | `vile-iso-boot-02.png` `1e817616…776b` — matches table (sha only) | PASS (sha) | Not re-read this run; boot-01 + boot-09 bracket it (hooks ran, desktop later ran), so the chain is consistent |
| Autologin + root shell | boot-04 `21a85a30…`, boot-05 `6fe9677a…` — match table (sha only) | PASS (sha) | Same bracketing argument as above |
| Live-system files present (`/root/vile-os`, `/etc/pam.d/vile`) | boot-07 `2736153b…` — matches table (sha only) | PASS (sha) | Same bracketing argument |
| /dev/dri card0 present (QEMU std VGA provides DRM) | boot-08 `f353949f…` — matches table (sha only) | PASS (sha) | Same; and boot-09 proves the compositor actually ran on that DRM device |
| **Vile RUNS under QEMU** — full Piece 5 desktop, visible mouse arrow | `vile-iso-boot-09.png` `71818b78…2ef1` — matches table; read by me; my pixel check: px at (100,105) is (255,255,255) vs wallpaper (43,45,58) at (200,105) | PASS | Top bar (V Vile + pills, "Tue 10 Nov 08:02", status icons), huge glowing 08:02, "Tuesday, November 10", purple Arch logo, floating dock (foot + V). **A white mouse arrow is clearly visible top-left (~95,105)** — the first proof of a visible cursor anywhere. The headless gap is closed |
| Super+Enter opens a tiled foot, dark title bar, dock visible | `vile-iso-boot-10.png` `7e06ac12…2755` — matches table; read by me; my pixel check: 139 red-dominant px in the top text rows, title bar (36,22,64) | PASS | Tiled foot, dark title bar, purple border, dock visible, mouse arrow visible (~100,125). And the red error line is right there — see next row |
| **BUG: foot `[colors]` invalid in foot 1.28 — honestly recorded, not hidden** | boot-10 shows `error: foot: /root/.config/foot/foot.ini:18: [colors]: invalid section name: colors` in red, first line of the terminal; log §Piece 6 records it with the root cause and the fix status ("NOT yet verified in a booted ISO") | PASS | The bug is visible in the shipped shot and written up plainly. This is how a bug should be reported. Terminal still works; only fg/bg fell back to defaults |
| All 10 v1 boot shots converted PPM → real PNG in place | `file *.png` on all 55 screenshots: every one is a real PNG | PASS | The format quirk is fixed; no more misnamed PPMs |

**Edit-worthy? YES** — boot-09 is a third poster frame. A viewer seeing Vile boot from its own ISO with a glowing clock and a visible cursor would screen-record it. One wart: the date line crosses the logo apex at 1280x800 (see flaws).

**Flaws:**
- **BUILD_LOG contradicts itself on the 0.5.0 ISO.** §6.6 says 1,543,421,952 bytes, 753,624 sectors, sha `2af4c0de4…`; the v2 section says 1,482,342,400 bytes, 723,800 sectors, sha `bb217ef0…`. Both are labelled "the" 0.5.0 ISO. The likely truth is build3 vs build6 (different compression), but the log never says that. The file is deleted, so nothing on disk can settle it — a one-line reconciliation note is needed.
- **Date overlaps the logo apex at 1280x800.** In boot-09/iso2-boot-04, "Tuesday, November 10" is rendered across the top of the purple triangle — the text sits ON the logo, not clear of it. At 1920x1080 (look-01) it is only a touch; at 1280x800 it is a real overlap. The desktop clock layout is not resolution-independent.
- The v1 boot test was interrupted mid-run by a VM restart (~07:04 UTC, /tmp wiped, QEMU killed, toolchain reinstalled). Honestly recorded in the log; the test was completed afterwards. Not a flaw in the ISO, a flaw in the run's continuity — worth noting because boot-01/02's shas in the 08:04 table were the pre-restart files and the 08:22 table re-stamped them after the PPM→PNG conversion.
- `nbd-client` mkinitcpio warning open (cosmetic, PXE only). Honestly recorded; needs a full pacstrap re-run to fix properly.

## 3. Piece 6 v2 — ISO v0.5.1 — PASS

| Claim | Evidence | Result | What I saw |
|---|---|---|---|
| ISO exists, 1,482,342,400 bytes, sha `26e34447…`, Volume ID VILE_OS, `/arch/VERSION` reads 0.5.1 | My own `sha256sum` + `ls`: size and sha match exactly; PVD/VERSION are log-only (I did not parse the ISO9660 myself) | PASS | Size + sha verified by me. PVD and VERSION rest on the log's hand-parse + the .sfs sha512 hash chain (`64f00019…`) — consistent, not independently re-derived |
| `[colors-dark]` fix in all three foot.ini copies | `grep -c colors-dark`: project `foot.ini` has the `[colors-dark]` section header (line 27, bg `0f0b1a`, fg `ede9fe`); both archiso copies contain it 4× each | PASS | The fix is in the profile, not just the project file — the exact failure mode the v2 session caught |
| 0.5.1 boots, Vile runs, visible mouse arrow | `vile-iso2-boot-04.png` `22ea52db…ea25` — matches table; read by me | PASS | Same desktop as boot-09 at 08:21, mouse arrow top-left visible (~95,105). The desktop is the same build running from the new ISO |
| **Foot fix VERIFIED in a booted ISO — no error, dark bg** | `vile-iso2-boot-05.png` `40db1b9b…b6e1` — matches table; read by me; my pixel check: terminal bg (15,11,26) = #0F0B1A exactly, title bar (36,22,64) = #241640, ZERO red-dominant pixels in the top text rows (y40–110) | PASS | Plain prompt `[root@archlinux compositor]#` is the first line — no error line. The fix is proven in a booted ISO, not just in the project file |
| syslinux TIMEOUT 50 (the v1 KNOWN FLAW fixed) | `archiso/syslinux/syslinux.cfg:3` has `TIMEOUT 50`; log says the 0.5.1 boot auto-booted past the menu with no manual Enter | PASS | Fixed in the file; the boot shots are consistent (no stuck menu) |
| All 5 v2 boot shots are real PNGs at capture time | `file` check: real PNGs; shas match the 08:22 table | PASS | — |
| Superseded 0.5.0 ISO deleted | `ls iso/`: only `vile-os-0.5.1-x86_64.iso` remains | PASS | The buggy ISO cannot be handed to Adnan by mistake |
| Scope: `main.c` / `vile-settings.py` not edited | mtimes: `main.c` 05:42 UTC, `vile-settings.py` 05:53 UTC — both predate the Piece 6 sessions | PASS | Scoped correctly |
| Disk quota headroom ~400 MB after the 0.5.1 copy | Log-only (I did not re-probe; probing would write 1+ GB) | PASS (log) | The log's warning stands: free space before the next ISO build |

**Edit-worthy? N/A** — same desktop as v0.5.0. The fix it proves is invisible (absence of an error line), which is exactly the point.

**Flaws:** none in the rebuild itself. The two carried flaws that touch it: the stale `foot.ini` comment (see §1) and the `nbd-client` warning (see §2).

## 4. Regressions — PASS

| Claim | Evidence | Result |
|---|---|---|
| Piece 1 byte-identical | `vile-screen-01.png` `e585a996…5e5a` — matches the 08:22 table and every earlier table | PASS |
| look-01 poster frame intact | `vile-look-01.png` `0d808db8…62b8` — matches; read by me: huge glowing 05:55, logo, slim bar, floating dock, no running dots | PASS |
| System still edit-worthy | look-01 + lock (audit #2) + boot-09 (this run) | **YES** — three frames now. The QEMU frame is the weakest of the three only because of the date/logo overlap |

## Flaws found (complete list, new + carried)

1. **Date overlaps the logo apex at 1280x800** — boot-09 / iso2-boot-04: "Tuesday, November 10" is rendered across the top of the purple triangle. At 1920x1080 it is a touch (audit #2 flaw); at 1280x800 it is an overlap. The desktop clock layout is not resolution-independent, and the QEMU/Dell frame is the one Adnan will see first when he boots the ISO.
2. **BUILD_LOG contradicts itself on the 0.5.0 ISO** — §6.6 (1,543,421,952 bytes / 753,624 sectors / sha `2af4c0de4…`) vs the v2 section (1,482,342,400 / 723,800 / sha `bb217ef0…`). Likely build3 vs build6, but the log never says so. One reconciliation note would close it.
3. **Stale comment in `foot.ini`** — "Fix NOT yet verified in a booted ISO — verify on the next rebuild." False since the 0.5.1 boot. One line.
4. **`vile-setup.sh` (v1, dead Hyprland-era installer) still in the project root** — audit #2 flaw #10, not addressed by either session. Someone could run it by mistake.
5. **`nbd-client` mkinitcpio warning persists in 0.5.1** — cosmetic, PXE/NBD only, honestly recorded. Needs a full pacstrap re-run.
6. **Disk quota headroom ~400 MB** — tight before the next ISO build. The log warns about it; the pip-cache purge is a one-time trick.
7. **Carried from audit #2, still open:** Settings bottom half is dead empty dark space; volume slider visual-only; workspace pills visual-only; no 4+ window shot; Super+M / Super+Shift+Q code-only, never screenshot-tested; Reveal delay 300 ms label-only; test shots show usernames `root`/`viletest` (must be `adnan` on his laptop); live system is UTC by default; **no download link for the ISO yet**; real-hardware gate (DRM/GLES2 on the Intel GPU, real keyboard/mouse, mouse arrow on the TV, lock + PAM unlock with his password, Reboot/Power Off never clicked).

## Must-fix before shipping (in order)

1. **Real-hardware gate.** Dell 7400 + Westinghouse TV run of the 0.5.1 ISO. The ISO exists and boots in QEMU; only Adnan's Dell can close this. (Unchanged from audits #1–2 — still the gate that matters most.)
2. **Get the ISO to Adnan.** A download link for `vile-os-0.5.1-x86_64.iso` (48h link or lasting storage). Without it the Dell run cannot happen.
3. **Fix the date/logo overlap at small resolutions.** Make the desktop clock layout resolution-aware — nudge the date below the logo, or scale the clock/logo down at 1280x800. The QEMU frame is the first thing Adnan sees when he boots the ISO, and the overlap is the first thing a viewer notices in it.
4. **Fix the two log-hygiene items.** One-line `foot.ini` comment correction + one reconciliation note for the §6.6 vs v2 0.5.0 contradiction. Cheap; stops the next judge chasing ghosts.
5. **Archive `vile-setup.sh` out of the project root.** Audit #2 asked for this; it is still there.

## Nice-to-have

- Fill the Settings bottom half (more sections, or move About up).
- Wire the volume slider to real audio; make workspace pills switch.
- 4+ window tiling screenshot; Super+M / Super+Shift+Q screenshot test.
- Make Reveal delay a real setting or drop the row.
- Fix `nbd` properly on the next full pacstrap rebuild.
- Free disk headroom before the next ISO build (the 0.5.0 deletion already freed 1.4 GB; the ~400 MB figure predates it — re-probe before building again).
- On the Dell run, confirm Settings and the lock both show `adnan`, and set the timezone.

## Still not done

- **Piece 6 is done in QEMU; the Dell run is the remaining gate** — everything that could be proven here is proven.
- Adnan's own profile photo (lock avatar is the purple V placeholder until he sets one in Settings, post-login).
- Settings wallpaper switching UI (hook already exists).

## Judge's bottom line

Piece 6 is real. The ISO boots, the Switch Root failure is gone, Vile runs from its own ISO in QEMU, and the visible mouse arrow — the last unproven thing in the whole system — is there in the shot, measured white against the dark wallpaper. The foot `[colors]` bug was found, reported honestly with the error line visible in the shot, fixed, and then proven fixed in a booted 0.5.1 ISO by my own pixel measurement: background exactly #0F0B1A, zero red pixels where the error used to be. The polish round also did what it promised: the chooser is genuinely dark (I measured the list, not just the header), every proof shot is durable, and the sha table is a script that regenerates to the exact same values — the audit-trail rot is finally fixed as a process, not a one-off.

The system is still edit-worthy — three frames now, not two — but the QEMU frame has one visible wart: the date sits on top of the logo at 1280x800. Fix the layout for small screens, get the ISO onto the Dell, and the next judge's list is just the hardware gate.
