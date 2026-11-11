#!/usr/bin/env python3
"""Closed-loop mouse v2: every move IS the probe. step_measure(dx,dy)
screenshots before/after a move and diffs them to find the new cursor,
so no move is wasted. glide2() iterates with gain 0.6 toward a target.
Usage: qglide2.py click X Y | glide X Y"""
import socket, sys, time
from PIL import Image
SOCK = "/home/hatch/workspace/vile-os/build-env/qemu-monitor"
PPM = "/tmp/qglide3.ppm"
def is_cursor_px(r, g, b):
    # white arrow (old) OR purple arrow #C574FE (0.5.6 default)
    if r > 225 and g > 225 and b > 225:
        return True
    return b > 200 and 150 < r < 230 and 80 < g < 170

def mon(cmd):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(10); s.connect(SOCK); time.sleep(0.3); s.recv(65536)
    s.sendall((cmd + "\n").encode()); time.sleep(0.4)
    out = b""; s.settimeout(2)
    try:
        while True:
            ch = s.recv(65536)
            if not ch: break
            out += ch
            if b"(qemu)" in out: break
    except socket.timeout: pass
    s.close()
    return out
def shot():
    mon(f"screendump {PPM}")
    return Image.open(PPM).convert("RGB")
def clusters(pts):
    pts = set(pts); comps = []
    while pts:
        seed = pts.pop(); comp = [seed]; stack = [seed]
        while stack:
            x, y = stack.pop()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    q = (x + dx, y + dy)
                    if q in pts:
                        pts.discard(q); stack.append(q); comp.append(q)
        comps.append(comp)
    return comps
def find_cursor(im):
    """Arrow tip = top-left of the small bright arrow cluster, if the
    background there is dark enough to isolate it. Returns None if the
    arrow merges with bright wallpaper."""
    W, H = im.size; px = im.load()
    bright = set()
    for y in range(0, H):
        for x in range(0, W):
            if 550 <= x <= 730 and y >= 755:
                continue
            r, g, b = px[x, y]
            if is_cursor_px(r, g, b):
                bright.add((x, y))
    for c in clusters(bright):
        xs = [p[0] for p in c]; ys = [p[1] for p in c]
        w, h = max(xs) - min(xs), max(ys) - min(ys)
        if 15 <= len(c) <= 400 and 8 <= w <= 30 and 10 <= h <= 32:
            return min(xs), min(ys)
    return None
def step_measure(dx, dy, prev):
    """Send one move; diff prev screenshot vs the new one; the new
    cursor is the small bright cluster present in the new shot inside
    the diff region. Returns (new_pos, new_shot)."""
    if dx or dy:
        mon(f"mouse_move {dx} {dy}")
        time.sleep(0.5)
    cur = shot()
    W, H = prev.size
    pa, pb = prev.load(), cur.load()
    changed = []
    for y in range(0, H):
        for x in range(0, W):
            if 550 <= x <= 730 and y >= 755:
                continue  # clock digits tick once a minute
            ra, ga, ba = pa[x, y]; rb, gb, bb = pb[x, y]
            if abs(ra - rb) + abs(ga - gb) + abs(ba - bb) > 90:
                changed.append((x, y))
    if not changed:
        return None, cur
    # new cursor: bright cluster in cur shot overlapping the changed set
    chset = set(changed)
    pb2 = cur.load()
    bright = set()
    for (x, y) in changed:
        r, g, b = pb2[x, y]
        if is_cursor_px(r, g, b):
            bright.add((x, y))
    cands = []
    for c in clusters(bright):
        xs = [p[0] for p in c]; ys = [p[1] for p in c]
        w, h = max(xs) - min(xs), max(ys) - min(ys)
        if 15 <= len(c) <= 400 and 8 <= w <= 30 and 10 <= h <= 32:
            cands.append((min(xs), min(ys), len(c)))
    if cands:
        # prefer the cluster closest to where the move should have gone
        cands.sort(key=lambda t: t[0])
        return (cands[-1][0], cands[-1][1]), cur
    return None, cur
def glide2(tx, ty, tol=10, iters=10):
    prev = shot()
    # establish position: try small probes in several directions (the
    # cursor may be clamped at an edge, where a probe that way is a no-op)
    pos = None
    for (px, py) in ((20, 0), (-20, 0), (0, 20), (0, -20)):
        pos, prev = step_measure(px, py, prev)
        if pos is not None:
            print(f"glide2: probe ({px},{py}) located cursor at {pos}")
            break
    if pos is None:
        print("glide2: cannot locate cursor"); return False
    for i in range(iters):
        cx, cy = pos
        dx, dy = tx - cx, ty - cy
        print(f"glide2 iter {i}: cursor ({cx},{cy}) err ({dx},{dy})")
        if abs(dx) <= tol and abs(dy) <= tol:
            print(f"glide2: ON TARGET ({cx},{cy})")
            return True
        sx = max(-160, min(160, int(dx * 0.6)))
        sy = max(-160, min(160, int(dy * 0.6)))
        if sx == 0 and dx != 0: sx = 1 if dx > 0 else -1
        if sy == 0 and dy != 0: sy = 1 if dy > 0 else -1
        pos, prev = step_measure(sx, sy, prev)
        if pos is None:
            print("glide2: lost cursor"); return False
    cx, cy = pos
    print(f"glide2: END ({cx},{cy}) target ({tx},{ty})")
    return abs(cx - tx) <= 16 and abs(cy - ty) <= 16
if __name__ == "__main__":
    a = sys.argv[1]
    if a == "glide":
        print("ok" if glide2(int(sys.argv[2]), int(sys.argv[3])) else "MISS")
    elif a == "click":
        ok = glide2(int(sys.argv[2]), int(sys.argv[3]))
        mon("mouse_button 1"); time.sleep(0.3); mon("mouse_button 0")
        print("clicked, on-target:", ok)
