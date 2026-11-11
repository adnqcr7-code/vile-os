#!/bin/bash
# Stage the 0.5.6 tree into archiso/airootfs/root/vile-os/ for the ISO.
# Mirrors build-iso.sh's staging (cp -r of whole dirs, NO trailing
# slashes — a trailing slash copies the dir CONTENTS into the dest
# and --delete then wipes the dest's subdirs; that mangled the
# staging twice on Nov 11 2026, do not reintroduce it).
# Sources: live tree for the code, publish tree for docs/screenfit/
# LICENSE/vile-install.sh (the publish tree is the repo source of
# truth for those). The 0.5.6 cursor block is re-applied to the
# staged vile-install.sh after copying (the publish copy does not
# have it — it is a 0.5.6 staging-only addition until the push).
set -u
cd /home/hatch/workspace/vile-os
STAGE="archiso/airootfs/root/vile-os"
PUB="/home/hatch/workspace/vile-os-publish"
rm -rf "$STAGE"
mkdir -p "$STAGE"
for d in compositor settings filemanager setup assets wallpapers; do
  cp -r "$d" "$STAGE/"
done
for d in docs screenfit; do
  cp -r "$PUB/$d" "$STAGE/"
done
for f in foot.ini README.md; do
  cp "$f" "$STAGE/"
done
for f in LICENSE vile-install.sh; do
  cp "$PUB/$f" "$STAGE/"
done
# Never ship build dirs or caches.
rm -rf "$STAGE/compositor/build"
find "$STAGE" -name __pycache__ -type d -exec rm -rf {} + 2>/dev/null || true
chmod +x "$STAGE/compositor/vile-foot.sh" "$STAGE/settings/vile-settings" \
  "$STAGE/filemanager/vile-files" "$STAGE/setup/vile-setup" 2>/dev/null || true
# 0.5.6: cursor-theme install block (after the foot-logo block).
python3 - "$STAGE/vile-install.sh" <<'PYEOF'
import sys
p = sys.argv[1]
s = open(p).read()
block = '''
# Purple cursor theme (0.5.6 — Adnan's purple arrow is the default
# pointer). The compositor loads it from the standard icon theme
# location and falls back to the system default if it is missing.
if [ -d "$REPO_DIR/assets/cursor-theme/vile" ]; then
  sudo mkdir -p /usr/share/icons/vile
  # Copy the theme CONTENTS (cursors/ + index.theme), not the
  # directory itself — /usr/share/icons/vile already exists from
  # the foot-logo block above, and cp -r of the dir would nest it
  # as /usr/share/icons/vile/vile/ where nothing looks for it.
  sudo cp -r "$REPO_DIR"/assets/cursor-theme/vile/cursors \\
    "$REPO_DIR"/assets/cursor-theme/vile/index.theme \\
    /usr/share/icons/vile/
  sudo chmod -R a+rX /usr/share/icons/vile
else
  echo "NOTE: cursor theme not in the repo yet — the system default cursor will be used."
fi
'''
anchor = 'echo "== 5/5 ScreenFit =="'
assert anchor in s and "cursor-theme" not in s, "vile-install.sh anchor missing or block already present"
s = s.replace(anchor, block.strip("\n") + "\n\n" + anchor, 1)
open(p, "w").write(s)
print("cursor block applied to staged vile-install.sh")
PYEOF
# Cursor theme files into the airootfs (standard icon location).
rm -rf archiso/airootfs/usr/share/icons/vile
mkdir -p archiso/airootfs/usr/share/icons/vile
cp -r assets/cursor-theme/vile/cursors assets/cursor-theme/vile/index.theme \
  archiso/airootfs/usr/share/icons/vile/
# Also refresh the staged BUILD_LOG from the live tree (it is
# appended during the release; the publish copy lags it).
cp BUILD_LOG.md "$STAGE/docs/BUILD_LOG.md"
echo "--- staged main.c vs live:"
sha256sum "$STAGE/compositor/src/main.c" compositor/src/main.c | cut -c1-16
echo "--- staged cursor theme vs live:"
sha256sum "$STAGE/assets/cursor-theme/vile/cursors/default" assets/cursor-theme/vile/cursors/default | cut -c1-16
sha256sum archiso/airootfs/usr/share/icons/vile/cursors/default | cut -c1-16
echo "--- staged vile-install.sh cursor block:"
grep -c "cursor-theme" "$STAGE/vile-install.sh"
echo "--- staging layout:"
ls "$STAGE"
