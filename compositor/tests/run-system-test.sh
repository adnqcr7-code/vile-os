#!/bin/bash
# Vile Settings — System monitor test (Performance dashboard, 0.3.0)
#
# Usage: ./run-system-test.sh
#   Runs the real Vile compositor headless, opens the real Settings
#   window (Python + GTK3, Wayland native), and captures REAL grim
#   screenshots:
#
#   vile-system-00-top.png       Settings open (Profile Picture first)
#   vile-system-01-layout.png    System section: wide CPU card
#                                (model + temp + usage pill), Storage /
#                                Network (live graph) / Memory row,
#                                GPU wide card. No hardware rows any
#                                more — the cards carry the info.
#   vile-system-02-cpu.png       Close crop of the CPU card from 01
#                                (model line + temp + usage pill).
#   vile-system-03-net-a.png     Network graph under a real download
#   vile-system-03-net-b.png     Same view seconds later — the graph
#                                must have moved (or, if the download
#                                fails, the graph still renders flat
#                                history — honest, not a failure).
#   vile-system-04-load.png      CPU under a 2-core busy loop —
#                                pill must read ~50% (2 of 4 cores),
#                                proving live updates, not static.
#   vile-system-05-idle-again.png  burners stopped, 4s settle —
#                                pill must have swung back toward idle.
#
# Before the GUI run, the unit tests must pass: test_gpu.py (GPU
# detection/usage against stubbed fixtures), test_gauge_caps.py
# (ring arcs end in flat cuts, no discs), test_temp.py (thermal
# zone preference + hwmon fallback + no-sensor), test_net_graph.py
# (graph drawing, temp/battery bars, root-device pill parsing).
#
# The compositor is built into a SCRATCH dir (/tmp/vile-sysmon-build)
# and run from the real compositor dir (relative asset paths) — the
# ISO agent owns ./build right now and must not be disturbed. All
# app writes go to a scratch HOME (/tmp/vile-sysmon-home); the real
# home's vile config is never touched. No battery exists on this VM,
# so no battery bar must appear in any shot; no lspci, no thermal
# zones, root is overlay (no drive pill), temp reads "—". All of
# those omissions are the correct behaviour, not failures.
#
# Scrolling: the virtual-pointer wheel (vptr_scroll) over the
# Settings window — GTK scrolls the scrolled window under the
# cursor on wheel input regardless of keyboard focus. (Tab +
# Page_Down did NOT scroll: the key went to the focused widget,
# not the scrolled window.) Settings tiles right: x 966..1908.
scroll() { # $1 = clicks (>0 down, <0 up)
  ./tests/vptr_scroll 1400 500 "$1" 2>&1 | tee -a /tmp/vile-system-grim.log
}
#
# All shots must be checked by eye before calling the System
# section done. Values are cross-checked against /proc at the end.
set -u
cd "$(dirname "$0")/.."

SCRATCH_BUILD="/tmp/vile-sysmon-build"
SCRATCH_HOME="/tmp/vile-sysmon-home"
RUNTIME_DIR="/tmp/vile-sysmon-runtime"
SOCKET_FILE="/tmp/vile-sysmon-socket.txt"
SETTINGS="$HOME/workspace/vile-os/settings/vile-settings.py"
SHOTDIR="$HOME/workspace/vile-os/screenshots"
OUT0="$SHOTDIR/vile-system-00-top.png"
OUT1="$SHOTDIR/vile-system-01-layout.png"
OUT2="$SHOTDIR/vile-system-02-cpu.png"
OUT3A="$SHOTDIR/vile-system-03-net-a.png"
OUT3B="$SHOTDIR/vile-system-03-net-b.png"
OUT4="$SHOTDIR/vile-system-04-load.png"
OUT5="$SHOTDIR/vile-system-05-idle-again.png"
mkdir -p "$SHOTDIR" "$SCRATCH_HOME" "$RUNTIME_DIR"

export XDG_RUNTIME_DIR="$RUNTIME_DIR"
REAL_HOME="$HOME"  # save before HOME goes scratch (paths below need it)
export HOME="$SCRATCH_HOME"

echo "==> Unit tests (GPU + gauge caps + temp + net graph) — must pass first"
python3 "$REAL_HOME/workspace/vile-os/settings/test_gpu.py" > /tmp/vile-system-units.log 2>&1 || { echo "UNIT TESTS FAILED:"; cat /tmp/vile-system-units.log; exit 1; }
python3 "$REAL_HOME/workspace/vile-os/settings/test_gauge_caps.py" >> /tmp/vile-system-units.log 2>&1 || { echo "UNIT TESTS FAILED:"; cat /tmp/vile-system-units.log; exit 1; }
python3 "$REAL_HOME/workspace/vile-os/settings/test_temp.py" >> /tmp/vile-system-units.log 2>&1 || { echo "UNIT TESTS FAILED:"; cat /tmp/vile-system-units.log; exit 1; }
python3 "$REAL_HOME/workspace/vile-os/settings/test_net_graph.py" >> /tmp/vile-system-units.log 2>&1 || { echo "UNIT TESTS FAILED:"; cat /tmp/vile-system-units.log; exit 1; }
tail -12 /tmp/vile-system-units.log
export WLR_BACKENDS=headless
export WLR_RENDERER=pixman
export WLR_HEADLESS_OUTPUTS=1
export WLR_SEAT=seat0
export VILE_WALLPAPER="$REAL_HOME/workspace/vile-os/wallpapers/vile-purple-arch.jpg"
export VILE_SOCKET_FILE="$SOCKET_FILE"
export VILE_LOCK_USER="${VILE_LOCK_USER:-viletest}"
unset DISPLAY
export PKG_CONFIG_PATH="/usr/local/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export LC_ALL=C.UTF-8
# meson/ninja live in the REAL home's pip --user bin dir; HOME is
# scratch here, so put it on PATH explicitly (same fix as
# run-setup-test.sh). Without this the scratch build silently
# fails to find `meson` and the run dies later with a confusing
# "build/vile: No such file" — the `| tail -2` pipes mask the
# failure, so pipefail makes a real build error fail here.
export PATH="$REAL_HOME/.local/bin:$PATH"

if [ ! -x "$SCRATCH_BUILD/build/vile" ]; then
  echo "==> Building scratch compositor in $SCRATCH_BUILD ..."
  rm -rf "$SCRATCH_BUILD"
  mkdir -p "$SCRATCH_BUILD"
  cp -r src meson.build "$SCRATCH_BUILD/"
  # HOME is scratch here, so Python's user site (~/.local) is NOT
  # the real one and the pip-installed meson module is invisible
  # (ModuleNotFoundError). Point the user base back at the real
  # home so `meson setup` finds mesonbuild.
  (cd "$SCRATCH_BUILD" && set -o pipefail && PYTHONUSERBASE="$REAL_HOME/.local" meson setup build --prefix=/usr/local 2>&1 | tail -2 \
    && ninja -C build 2>&1 | tail -2) || { echo "SCRATCH BUILD FAILED"; exit 1; }
fi

shot() { # $1 = output path
  WAYLAND_DISPLAY="$SOCKET" grim -o HEADLESS-1 "$1" 2>&1 | tee -a /tmp/vile-system-grim.log
  python3 -c "from PIL import Image; im=Image.open('$1'); print('shot:', '$1', im.size)" 2>&1 || file "$1"
}

echo "==> Starting compositor (scratch build, unlocked, foot + Settings)..."
export VILE_QUIT_AFTER_SEC=180
export VILE_NO_SETUP=1  # these tests are about Settings, not first-run
rm -f "$SOCKET_FILE"
"$SCRATCH_BUILD/build/vile" -s foot > /tmp/vile-system-compositor.log 2>&1 &
VILE_PID=$!
for i in $(seq 1 50); do
  [ -f "$SOCKET_FILE" ] && break
  sleep 0.1
done
if [ ! -f "$SOCKET_FILE" ]; then
  echo "ERROR: compositor did not start. Log:"
  cat /tmp/vile-system-compositor.log
  kill $VILE_PID 2>/dev/null || true
  exit 1
fi
SOCKET="$(cat "$SOCKET_FILE")"
echo "==> Compositor up on WAYLAND_DISPLAY=$SOCKET (pid $VILE_PID)"

sleep 3
echo "==> Opening Settings..."
WAYLAND_DISPLAY="$SOCKET" XDG_RUNTIME_DIR="$RUNTIME_DIR" \
  GDK_BACKEND=wayland python3 "$SETTINGS" > /tmp/vile-system-app.log 2>&1 &
SETTINGS_PID=$!
sleep 5  # gauges need 1s to populate from real deltas; let it settle
echo "==> Shot: $OUT0 (top of Settings)"
shot "$OUT0"

echo "==> Wheel down to the System section"
scroll 3
sleep 1
echo "==> Shot: $OUT1 (Performance layout)"
shot "$OUT1"

echo "==> CPU card close crop (top of the Settings window in shot 01)"
python3 - "$OUT1" "$OUT2" <<'EOF'
import sys
from PIL import Image
src, dst = sys.argv[1], sys.argv[2]
im = Image.open(src)
# Settings tiles right: x 966..1908. The CPU card sits at the top
# of the scrolled view; crop a generous band and keep it 2x-wide.
crop = im.crop((966, 60, 1908, 480))
crop.save(dst)
print("crop:", dst, crop.size)
EOF

echo "==> Real download in the background (feeds the network graph)"
# Debian mirror (the sandbox proxy reaches it; hetzner does not —
# curl exits 000 there). ~540 KB/s sustained, so the graph draws
# a real mountain while Download reads a live rate.
curl -s -o /dev/null --max-time 15 https://deb.debian.org/debian/dists/stable/main/installer-amd64/current/images/netboot/mini.iso 2>/dev/null || true &
CURL_PID=$!
sleep 4
echo "==> Shot: $OUT3A (network graph, download running)"
shot "$OUT3A"
sleep 4
echo "==> Shot: $OUT3B (network graph seconds later — must have moved)"
shot "$OUT3B"
kill $CURL_PID 2>/dev/null || true
wait $CURL_PID 2>/dev/null || true

echo "==> CPU burner: 2 cores busy for 6s, gauges already in view"
timeout 6 python3 -c "while True: pass" &
BURNER1=$!
timeout 6 python3 -c "while True: pass" &
BURNER2=$!
sleep 2.5
echo "==> Shot: $OUT4 (CPU pill under load — must read ~50%)"
shot "$OUT4"
wait $BURNER1 $BURNER2  # burners done (not the compositor)

echo "==> Burners stopped — 4s to settle back to idle"
sleep 4
echo "==> Shot: $OUT5 (idle again — pill must have swung back down)"
shot "$OUT5"

echo "==> Cross-check values from /proc (compare with the shots):"
grep -m1 "model name" /proc/cpuinfo
grep -c "^processor" /proc/cpuinfo
grep -m1 "cpu MHz" /proc/cpuinfo
free -b | head -2
df -B1 / | tail -1
grep -E "MemTotal|MemAvailable" /proc/meminfo
grep -E "host0" /proc/net/dev
echo "thermal zones (must be: none here):"
ls /sys/class/thermal/ | grep thermal_zone || echo "  no thermal_zone*"
echo "root device (must be: overlay -> no pill here):"
grep -E " / " /proc/mounts | head -2
echo "GPU state on this VM (must be: no GPU detected):"
python3 -c "
import importlib.util
spec = importlib.util.spec_from_file_location('vs', '$REAL_HOME/workspace/vile-os/settings/vile-settings.py')
vs = importlib.util.module_from_spec(spec); spec.loader.exec_module(vs)
s = vs.GPUSampler()
print('vendor:', s.vendor, '| model:', s.model, '| usage:', s.sample_usage())
print('temp:', vs.read_cpu_temp(), '| model line:', vs.cpu_model_line())
print('root device:', vs.read_root_device())
"

echo "==> Stopping Settings + compositor"
kill $SETTINGS_PID 2>/dev/null || true
kill $VILE_PID 2>/dev/null || true
wait $VILE_PID 2>/dev/null || true
echo "==> Shots:"
ls -lh "$OUT0" "$OUT1" "$OUT2" "$OUT3A" "$OUT3B" "$OUT4" "$OUT5" 2>&1
echo "==> App log (errors):"
grep -iE "error|traceback|warning" /tmp/vile-system-app.log | head -20 || true
