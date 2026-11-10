#!/usr/bin/env python3
"""Find click points in a Vile Setup screenshot (layout-aware).

The wizard's layout is computed by GTK at runtime, so the harness
does not hardcode pixel coords — it scans the screenshot for the
one region that can only be the target, and prints "X Y".

Targets:
  continue         the purple Continue / Finish / Install /
                   Open-desktop button (#6D4AFF, the largest solid
                   purple region in the bottom 400px — the dots are
                   muted, the "Current" tag is text, both smaller)
  name-entry       the name text entry (#171228 fill, largest such
                   region in the middle band — the only one on the
                   name step)
  choose-picture   the "Choose picture..." button: directly under the
                   name entry (same x, entry bottom + 35)
  wallpaper-nebula the middle wallpaper row: the centre of the
                   #171228 wallpaper frame (row 2 of 3 sits at the
                   frame's vertical centre)
  wallpaper-dunes  the third wallpaper row: frame bottom - 100
Usage: setup-coords.py <target> <png>
"""
import sys
from PIL import Image

PURPLE = (109, 74, 255)   # #6D4AFF
PURPLE_HOVER = (125, 92, 255)  # #7D5CFF — the button WHILE the pointer
# sits on it (after the previous click). The first run of this
# harness died on the apps step because the scan only knew the
# normal colour: after clicking Continue the pointer stays over the
# button, so it renders hovered and the scan found nothing.
ENTRY_BG = (23, 18, 40)   # #171228


def largest_component(im, colour, x0, y0, x1, y1):
    """Bounding box of the largest 8-connected run of `colour` in
    the given rect, or None. (A flood fill over the matching pixels;
    the sets here are a few thousand pixels at most.)"""
    px = im.load()
    seen = set()
    best = None
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x, y) in seen:
                continue
            if px[x, y][:3] != colour:
                continue
            # New component: flood it.
            stack = [(x, y)]
            seen.add((x, y))
            minx = maxx = x
            miny = maxy = y
            n = 0
            while stack:
                cx, cy = stack.pop()
                n += 1
                minx = min(minx, cx); maxx = max(maxx, cx)
                miny = min(miny, cy); maxy = max(maxy, cy)
                for nx in (cx - 1, cx, cx + 1):
                    for ny in (cy - 1, cy, cy + 1):
                        if (nx, ny) in seen:
                            continue
                        if not (x0 <= nx < x1 and y0 <= ny < y1):
                            continue
                        if px[nx, ny][:3] != colour:
                            continue
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if best is None or n > best[0]:
                best = (n, minx, miny, maxx, maxy)
    if best is None:
        return None
    _n, minx, miny, maxx, maxy = best
    return (minx, miny, maxx, maxy)


def largest_component_multi(im, colours, x0, y0, x1, y1):
    """Like largest_component, but a pixel matches if it is ANY of
    `colours` (the Continue button is #6D4AFF normally, #7D5CFF
    while hovered — same widget, same place)."""
    px = im.load()
    seen = set()
    best = None
    for y in range(y0, y1):
        for x in range(x0, x1):
            if (x, y) in seen:
                continue
            if px[x, y][:3] not in colours:
                continue
            stack = [(x, y)]
            seen.add((x, y))
            minx = maxx = x
            miny = maxy = y
            n = 0
            while stack:
                cx, cy = stack.pop()
                n += 1
                minx = min(minx, cx); maxx = max(maxx, cx)
                miny = min(miny, cy); maxy = max(maxy, cy)
                for nx in (cx - 1, cx, cx + 1):
                    for ny in (cy - 1, cy, cy + 1):
                        if (nx, ny) in seen:
                            continue
                        if not (x0 <= nx < x1 and y0 <= ny < y1):
                            continue
                        if px[nx, ny][:3] not in colours:
                            continue
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if best is None or n > best[0]:
                best = (n, minx, miny, maxx, maxy)
    if best is None:
        return None
    _n, minx, miny, maxx, maxy = best
    return (minx, miny, maxx, maxy)


def all_matching_bbox(im, colour, x0, y0, x1, y1):
    """Bounding box of ALL pixels of `colour` in the rect (not the
    largest component). Used for the wallpaper frame: the frame is
    NOT one component — each row button's 2px border splits its
    interior off from the frame background, so "largest component"
    picks an arbitrary ROW (the first run clicked Dunes instead of
    Nebula that way). The bbox of everything is the frame."""
    px = im.load()
    minx = miny = None
    maxx = maxy = None
    for y in range(y0, y1):
        for x in range(x0, x1):
            if px[x, y][:3] != colour:
                continue
            if minx is None:
                minx = maxx = x
                miny = maxy = y
            else:
                minx = min(minx, x); maxx = max(maxx, x)
                miny = min(miny, y); maxy = max(maxy, y)
    if minx is None:
        return None
    return (minx, miny, maxx, maxy)


def main():
    if len(sys.argv) != 3:
        print("usage: setup-coords.py <target> <png>", file=sys.stderr)
        return 2
    target, path = sys.argv[1], sys.argv[2]
    im = Image.open(path).convert("RGB")
    w, h = im.size

    if target == "continue":
        # The button never moves: (1287..1384, 820..862) on every
        # step. Scan only its band (y 810..1080) so the wallpaper
        # frame's purple selected-row border (y <= 801) cannot win.
        box = largest_component_multi(im, (PURPLE, PURPLE_HOVER),
                                      0, 810, w, h)
        if not box:
            print("no purple button found", file=sys.stderr)
            return 1
        print((box[0] + box[2]) // 2, (box[1] + box[3]) // 2)
        return 0

    if target in ("name-entry", "choose-picture"):
        box = largest_component(im, ENTRY_BG, 0, 200, w, 800)
        if not box:
            print("no entry found", file=sys.stderr)
            return 1
        cx = (box[0] + box[2]) // 2
        if target == "name-entry":
            print(cx, (box[1] + box[3]) // 2)
        else:
            print(cx, box[3] + 35)
        return 0

    if target in ("wallpaper-nebula", "wallpaper-dunes"):
        # Frame bbox from ALL #171228 pixels inside the window
        # (x 100..1820, y 100..950 — excludes the 12px wallpaper
        # border, the dock's ScreenFit tile and the bar).
        box = all_matching_bbox(im, ENTRY_BG, 100, 100, 1820, 950)
        if not box:
            print("no wallpaper frame found", file=sys.stderr)
            return 1
        cx = (box[0] + box[2]) // 2
        top, bottom = box[1], box[3]
        fh = bottom - top
        if target == "wallpaper-nebula":
            # Row 2 of 3: frame top + 18 pad + 155 row + 10 gap
            # + 77 (half a row) = top + 260 of a ~521px frame.
            print(cx, top + int(fh * 260 / 521))
        else:
            # Row 3 centre: frame bottom - 107.
            print(cx, bottom - 107)
        return 0

    print(f"unknown target: {target}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
