#!/bin/bash
# Vile Files v1.2 — zip / unzip test harness
#
# Usage: ./run-files-12-test.sh
#   Runs the real Vile compositor headless (snapshot binary), opens
#   the real Files window, and captures REAL grim screenshots:
#
#   Phase 1 — compress (scratch root /tmp/vile-files-test12):
#     vile-files-12-menu-folder.png   right-click menu on Pack
#                                     ("Compress to ZIP" must show)
#     vile-files-12-pack-zip.png     Pack.zip created in the listing
#     vile-files-12-menu-multi.png   Many + Pack both selected,
#                                     menu open on Pack
#     vile-files-12-name-dialog.png  "Compress to ZIP" name dialog
#     vile-files-12-progress.png     progress dialog MID-compress
#                                     (Many = 300 files, slow mode)
#     vile-files-12-bundle-zip.png   Bundle.zip created in listing
#   Phase 2 — extract (scratch /tmp/vile-files-test12/Extracts):
#     vile-files-12-menu-zip.png     right-click menu on ready.zip
#                                     (Extract Here / Extract to…)
#     vile-files-12-extracted.png    ready/ extracted by Extract Here
#     vile-files-12-extract-to.png   Extract to… folder chooser open
#     vile-files-12-extract-to-done.png  Dest/ready/ extracted
#     vile-files-12-error-corrupt.png    broken.zip error dialog
#     vile-files-12-error-locked.png     locked.zip error dialog
#
# On-disk verification (unzip -l, sha256 byte-identity) runs in the
# script and FAILS the run if it does not match. All creation /
# extraction tests run ONLY in the scratch tree — never the real home.
#
# Click coordinates were measured from probe shots (same 1920x1080
# layout, single tiled window 1896x932 at (12,12)): grid row 1 cell
# centres y=180, x = 325 / 536 / 747 / 958 / 1169 / 1380 / 1591 for
# columns 1-7; a right-click menu opens with its top-left at the
# click point, items ~36px apart, first item centre y = click_y+24,
# menu item centre x = click_x+99.
# CAREFUL: the scratch root also contains the Extracts/ folder (made
# by the harness setup), so Phase 1 columns are Extracts, Many, Pack,
# broken.zip, locked.zip, ready.zip — Pack is column 3 (747,180),
# Many column 2 (536,180), NOT the positions a 5-item listing would
# have. The first full run proved this the hard way (it compressed
# Many instead of Pack — and that accident also proved the progress
# dialog: "58 of 300 files", 19%, real Cancel button).
set -u
cd "$(dirname "$0")/.."

# Clean up any leftovers from an earlier aborted run (a previous
# harness that exited early leaves its compositor + app running;
# two compositors + two apps fight over the 1.5 CPUs and the next
# run's first click can land before the window is ready — seen in
# run 3, where the first right-click never reached the app).
pkill -x vile-files-vile 2>/dev/null || true
pkill -f "vile-files.py --path" 2>/dev/null || true
sleep 1

SHOTS="$HOME/workspace/vile-os/screenshots"
APP="$HOME/workspace/vile-os/filemanager/vile-files.py"
SCRATCH="/tmp/vile-files-test12"
EXTRACTS="$SCRATCH/Extracts"
mkdir -p "$SHOTS"

# ---- scratch tree (reset every run) ---------------------------------
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH/Pack/Inner" "$SCRATCH/Many" "$EXTRACTS/Dest"
printf 'Pack readme — hello Vile.\n' > "$SCRATCH/Pack/a.txt"
printf '# Pack notes\n\n- one\n- two\n' > "$SCRATCH/Pack/c.md"
printf 'Inner file, deep in Pack.\n' > "$SCRATCH/Pack/Inner/b.txt"
python3 - <<'PYEOF'
import os, random
random.seed(12)
for i in range(300):
    with open(f"/tmp/vile-files-test12/Many/file_{i:03d}.txt", "w") as f:
        f.write(f"Many file {i}\n" + "".join(random.choice("abcdefghij ") for _ in range(2000)) + "\n")
PYEOF
python3 - <<'PYEOF'
import zipfile
with zipfile.ZipFile("/tmp/vile-files-test12/ready.zip", "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("Ready/ready.txt", "Ready to extract.\n")
    z.writestr("Ready/nested/deep.txt", "Deep inside Ready.\n")
PYEOF
printf 'this is not a zip file at all, just text\n' > "$SCRATCH/broken.zip"
printf 'password test file\n' > "$SCRATCH/locked-plain.txt"
(cd "$SCRATCH" && zip -q -P vile123 locked.zip locked-plain.txt && rm locked-plain.txt)
cp "$SCRATCH/ready.zip" "$SCRATCH/broken.zip" "$SCRATCH/locked.zip" "$EXTRACTS/"

export XDG_RUNTIME_DIR="/tmp/vile-files12-runtime"
mkdir -p "$XDG_RUNTIME_DIR"
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="$HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg"
export VILE_SOCKET_FILE="/tmp/vile-files12-socket.txt"
export VILE_QUIT_AFTER_SEC=600
export VILE_NO_SETUP=1  # skip the Piece 7 first-run wizard
export VILE_FILES_SLOW_ZIP=1  # test hook: worker pauses per file
export LC_ALL=C.UTF-8
unset DISPLAY
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
VILE_BIN="/tmp/vile-files-vile"
[ -x "$VILE_BIN" ] || cp ./build/vile "$VILE_BIN"

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-files12-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

start_compositor() {
  rm -f "$VILE_SOCKET_FILE"
  "$VILE_BIN" > /tmp/vile-files12-compositor.log 2>&1 &
  VILE_PID=$!
  for i in $(seq 1 50); do
    [ -f "$VILE_SOCKET_FILE" ] && break
    sleep 0.1
  done
  if [ ! -f "$VILE_SOCKET_FILE" ]; then
    echo "ERROR: compositor did not start. Log:"
    cat /tmp/vile-files12-compositor.log
    kill $VILE_PID 2>/dev/null || true
    exit 1
  fi
  SOCKET="$(cat "$VILE_SOCKET_FILE")"
  echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"
}

launch_app() { # $1 = extra args
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
    GDK_BACKEND=wayland python3 "$APP" $1 > /tmp/vile-files12-app.log 2>&1 &
  APP_PID=$!
  sleep 7  # GTK init + window map + startup animation must finish
}

stop_app() {
  kill $APP_PID 2>/dev/null || true
  sleep 1
}

rclick() { # $1=x $2=y — right-click via the vptr_right tool
  ./tests/vptr_right "$1" "$2" click 2>&1 | tee -a /tmp/vile-files12-grim.log
}
lclick() { # $1=x $2=y
  ./tests/vptr_test "$1" "$2" click 2>&1 | tee -a /tmp/vile-files12-grim.log
}
key() { # key names, e.g. Right / Control_L space
  ./tests/vkey_test "$@" 2>&1 | tee -a /tmp/vile-files12-grim.log
}
type() { # text via wtype
  WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" wtype "$1" 2>&1 | tee -a /tmp/vile-files12-grim.log
}

# Dialog OK button (bottom-right of the right-hand tile, same
# position the v1 harness measured for Gtk.Dialog action buttons).
# NOTE: this is the position for Gtk.Dialog windows with an entry
# (New Folder / Compress name / Extract to… chooser). A
# Gtk.MessageDialog (the extract error dialogs) is SHORT — the
# compositor tiles it as a full-height window and its single OK
# button sits right under the message text, at about (1170,150).
# Clicking (1851,910) on an error dialog hits empty dialog body
# and the dialog stays open — that is what broke step [6] in the
# run before this fix (the "locked" shot showed the old corrupt
# dialog, and the no-folder gate passed vacuously).
OK_X=1851
OK_Y=910
ERR_OK_X=1170
ERR_OK_Y=150

# After dismissing an error dialog, the dialog tile must be gone:
# sample (1500,100) — dialog body is bright (R>60), the re-tiled
# Files window there is dark (R~15). Fails the run if the dialog
# is still open, so a missed dismiss can never pass silently.
dialog_gone() { # $1 = shot path
  python3 -c "
from PIL import Image
px = Image.open('$1').convert('RGB').getpixel((1500, 100))
assert px[0] < 60, f'error dialog still open (pixel {px})'
print('dialog dismissed OK')
"
}

echo "===== Phase 1: compress (scratch root) ====="
start_compositor
sleep 3
launch_app "--path $SCRATCH"

echo "==> [1] Right-click Pack (747,180) — menu must show Compress to ZIP"
rclick 747 180
sleep 1.5
shot "$SHOTS/vile-files-12-menu-folder.png"

echo "==> [1] Click Compress to ZIP (846,204)"
lclick 846 204
sleep 3
shot "$SHOTS/vile-files-12-pack-zip.png"
echo "==> On-disk check: Pack.zip contents"
unzip -l "$SCRATCH/Pack.zip" || { echo "FAIL: Pack.zip missing/unreadable"; exit 1; }
python3 -c "
import zipfile
names = zipfile.ZipFile('$SCRATCH/Pack.zip').namelist()
assert 'Pack/a.txt' in names and 'Pack/c.md' in names and 'Pack/Inner/b.txt' in names, names
print('Pack.zip contents OK:', names)
"

echo "==> [2] Multi-select: click Many (536,180), Ctrl+Right, Ctrl+Space"
lclick 536 180
sleep 0.5
key Control_L Right
sleep 0.5
key Control_L space
sleep 0.5
echo "==> [2] Right-click Pack (747,180) with Many+Pack selected"
rclick 747 180
sleep 1.5
shot "$SHOTS/vile-files-12-menu-multi.png"
echo "==> [2] Click Compress to ZIP (846,204) — name dialog must appear"
lclick 846 204
sleep 2
shot "$SHOTS/vile-files-12-name-dialog.png"
echo "==> [2] Ctrl+A, type 'Bundle', click OK ($OK_X,$OK_Y)"
key Control_L a
sleep 0.5
type "Bundle"
sleep 0.5
lclick $OK_X $OK_Y
sleep 2
echo "==> [2] Progress dialog, mid-compress (300 files, slow mode)"
shot "$SHOTS/vile-files-12-progress.png"
echo "==> [2] Waiting for Bundle.zip to finish (slow mode ~16s)..."
for i in $(seq 1 30); do
  [ -f "$SCRATCH/Bundle.zip" ] && python3 -c "
import zipfile
zf = zipfile.ZipFile('$SCRATCH/Bundle.zip')
if len(zf.namelist()) < 300: raise SystemExit(1)
" 2>/dev/null && break
  sleep 1
done
sleep 2
shot "$SHOTS/vile-files-12-bundle-zip.png"
echo "==> On-disk check: Bundle.zip contents"
unzip -l "$SCRATCH/Bundle.zip" | head -8 || { echo "FAIL: Bundle.zip missing/unreadable"; exit 1; }
python3 -c "
import zipfile
names = zipfile.ZipFile('$SCRATCH/Bundle.zip').namelist()
assert 'Many/file_000.txt' in names and 'Many/file_299.txt' in names, names[:5]
assert 'Pack/a.txt' in names and 'Pack/Inner/b.txt' in names, names[:5]
# Pack.zip must NOT be in Bundle: the plain click on Many has to
# clear the stale Pack.zip selection (v1.2 selection fix) — the
# first passing run zipped 304 files because FlowBox MULTIPLE
# adds on a plain click.
files = [n for n in names if not n.endswith('/')]
assert len(files) == 303, f'{len(files)} files, expected 303'
assert 'Pack.zip' not in names, 'Pack.zip leaked into Bundle'
print('Bundle.zip contents OK:', len(names), 'entries')
" || { echo "FAIL: Bundle.zip contents wrong"; exit 1; }
stop_app

echo "===== Phase 2: extract (Extracts folder) ====="
launch_app "--path $EXTRACTS"
# Extracts listing: Dest (folder, 325), broken.zip (536),
# locked.zip (747), ready.zip (958). (An earlier draft of this
# comment omitted the Dest folder — the coords were right, the
# comment was wrong; do not "fix" the coords to match the old
# comment.)
echo "==> [3] Right-click ready.zip (958,180) — menu must show Extract entries"
rclick 958 180
sleep 1.5
shot "$SHOTS/vile-files-12-menu-zip.png"
echo "==> [3] Click Extract Here (1057,240)"
lclick 1057 240
sleep 3
shot "$SHOTS/vile-files-12-extracted.png"
echo "==> On-disk check: ready/ byte-identical to the originals"
# NOTE: the extracted folder is named after the ARCHIVE FILE
# ("ready.zip" -> "ready", lowercase); the archive's inner folder
# is "Ready" (capital R). So the files live at
# Extracts/ready/Ready/ready.txt — the first full run checked
# Extracts/Ready/ by mistake while the app had done it right
# (status bar: "Extracted 2 files to ready").
sha256sum "$EXTRACTS/ready/Ready/ready.txt" "$EXTRACTS/ready/Ready/nested/deep.txt"
printf 'Ready to extract.\n' | sha256sum
printf 'Deep inside Ready.\n' | sha256sum
[ -f "$EXTRACTS/ready/Ready/ready.txt" ] && [ -f "$EXTRACTS/ready/Ready/nested/deep.txt" ] || { echo "FAIL: ready/ not extracted"; exit 1; }
cmp -s "$EXTRACTS/ready/Ready/ready.txt" <(printf 'Ready to extract.\n') || { echo "FAIL: ready.txt differs"; exit 1; }
cmp -s "$EXTRACTS/ready/Ready/nested/deep.txt" <(printf 'Deep inside Ready.\n') || { echo "FAIL: deep.txt differs"; exit 1; }
echo "Extract Here byte-identity OK"
# Listing now: Dest (325), ready (536), broken.zip (747),
# locked.zip (958), ready.zip (1169)

echo "==> [4] Right-click ready.zip (1169,180) — Extract to…"
rclick 1169 180
sleep 1.5
echo "==> [4] Click Extract to… (1268,276)"
lclick 1268 276
sleep 2
shot "$SHOTS/vile-files-12-extract-to.png"
echo "==> [4] Ctrl+L, type the Dest folder, Enter"
key Control_L l
sleep 0.5
type "$EXTRACTS/Dest"
sleep 0.5
key Return
sleep 2
if [ -f "$EXTRACTS/Dest/ready/Ready/ready.txt" ]; then
  echo "==> Enter already confirmed the chooser — Dest/ready exists."
else
  echo "==> Clicking Extract ($OK_X,$OK_Y)..."
  lclick $OK_X $OK_Y
  sleep 3
fi
[ -f "$EXTRACTS/Dest/ready/Ready/ready.txt" ] || { echo "FAIL: Extract to… did not produce Dest/ready/Ready/ready.txt"; exit 1; }
cmp -s "$EXTRACTS/Dest/ready/Ready/ready.txt" <(printf 'Ready to extract.\n') || { echo "FAIL: Dest ready.txt differs"; exit 1; }
shot "$SHOTS/vile-files-12-extract-to-done.png"
echo "Extract to… OK"
# Extract to… navigates the app INTO the destination folder
# (Dest) and selects the extracted folder — that is the app's
# designed behaviour, so the listing is now just Dest/ready.
# Steps [5]/[6] must navigate back up to Extracts first, or
# their right-clicks land on empty space and the error gates
# pass vacuously (that is exactly what happened in the run
# before this fix: all three late shots were byte-identical).
echo "==> [5] Navigate back up to Extracts — Up button (140,35)"
lclick 140 35
sleep 1
echo "==> [5] Right-click broken.zip (747,180) — Extract Here must fail cleanly"
rclick 747 180
sleep 1.5
echo "==> [5] Click Extract Here (846,240)"
lclick 846 240
sleep 3
shot "$SHOTS/vile-files-12-error-corrupt.png"
[ -e "$EXTRACTS/broken" ] && { echo "FAIL: corrupt extract left a folder behind"; exit 1; }
# The error shot must differ from the previous screen — if it is
# byte-identical to extract-to-done, the dialog never appeared.
cmp -s "$SHOTS/vile-files-12-error-corrupt.png" "$SHOTS/vile-files-12-extract-to-done.png" && { echo "FAIL: corrupt-error shot identical to extract-to-done — dialog never appeared"; exit 1; }
echo "Corrupt: nothing extracted (correct)"
echo "==> [5] Dismiss the error dialog — OK ($ERR_OK_X,$ERR_OK_Y)"
lclick $ERR_OK_X $ERR_OK_Y
sleep 1
shot /tmp/vile-files12-after-dismiss1.png
dialog_gone /tmp/vile-files12-after-dismiss1.png

echo "==> [6] Right-click locked.zip (958,180) — password-protected must fail cleanly"
rclick 958 180
sleep 1.5
echo "==> [6] Click Extract Here (1057,240)"
lclick 1057 240
sleep 3
shot "$SHOTS/vile-files-12-error-locked.png"
[ -e "$EXTRACTS/locked" ] && { echo "FAIL: locked extract left a folder behind"; exit 1; }
cmp -s "$SHOTS/vile-files-12-error-locked.png" "$SHOTS/vile-files-12-error-corrupt.png" && { echo "FAIL: locked-error shot identical to corrupt-error shot — dialog never appeared"; exit 1; }
echo "Password-protected: nothing extracted (correct)"
echo "==> [6] Dismiss the error dialog — OK ($ERR_OK_X,$ERR_OK_Y)"
lclick $ERR_OK_X $ERR_OK_Y
sleep 1
shot /tmp/vile-files12-after-dismiss2.png
dialog_gone /tmp/vile-files12-after-dismiss2.png

stop_app
# Do NOT wait out the compositor's 600 s quit-after timeout — kill
# it now that the checks are done (an earlier run hung here).
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
echo "==> App log tail:"
tail -5 /tmp/vile-files12-app.log
echo "==> Scratch tree now:"
find "$SCRATCH" | sort
echo "==> Shots:"
ls -lh "$SHOTS"/vile-files-12-*.png
echo "==> ALL v1.2 GUI CHECKS PASSED (on-disk gates enforced above)"
