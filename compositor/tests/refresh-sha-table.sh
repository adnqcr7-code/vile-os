#!/bin/bash
# refresh-sha-table.sh — the ONE current-sha table for BUILD_LOG.md.
#
# Audit #2 found the rot pattern twice: per-section shas in BUILD_LOG
# go stale the moment any harness re-shoots a file, and the log ends
# up with three sections disagreeing with the files on disk. The fix
# is process, not another correction pass: this script recomputes the
# sha256 (first 8 … last 4 chars) of EVERY png under screenshots/ and
# prints one markdown table. After any harness run, run it and append
# its output to BUILD_LOG.md (cat >> only) with a line naming which
# harness run produced the files. That appended table is the single
# source of truth; older per-section shas are history.
#
# Usage: ./refresh-sha-table.sh ["note naming the harness run"]
#   e.g. ./refresh-sha-table.sh "run-look-test.sh + run-input-test.sh + run-settings-picker-test.sh, Nov 10 2026 ~01:00 EST"
set -u
SHOTS="$HOME/workspace/vile-os/screenshots"
cd "$SHOTS" || { echo "ERROR: no screenshots dir: $SHOTS" >&2; exit 1; }

echo "### Current screenshot shas — $(date -u '+%Y-%m-%d %H:%M UTC')"
[ -n "${1:-}" ] && echo ""
[ -n "${1:-}" ] && echo "Produced by: $1"
echo ""
echo "| File | sha256 (first 8 … last 4) |"
echo "|---|---|"
count=0
for f in *.png; do
  h="$(sha256sum "$f" | cut -c1-64)"
  echo "| \`$f\` | \`${h:0:8}…${h:60:4}\` |"
  count=$((count + 1))
done
echo ""
echo "_$count files. This table is the single source of truth for"
echo "screenshot shas — older per-section values in this log are"
echo "history and may not match disk._"
