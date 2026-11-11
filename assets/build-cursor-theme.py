#!/usr/bin/env python3
"""Build the Vile cursor theme from the purple arrow PNG.

Adnan's direction (Nov 10, 2026 ~22:52): the purple arrow (or
something like it) is Vile's main cursor. Source:
assets/cursor-purple-arrow.png — #C574FE, 213x213, transparent,
hotspot (the arrow tip) at (8,8) in the source crop.

Output: assets/cursor-theme/vile/
    cursors/default    XCursor file, sizes 24 + 32 (hotspot scaled
                       by the same 8/213 ratio -> (1,1) at both)
    cursors/left_ptr   same image (some clients ask for left_ptr)
    cursors/pointer    same image (link/hover pointer)
    index.theme        [Icon Theme] Name=Vile

The XCursor file format is the one wlroots' xcursor.c parses
(magic "Xcur", TOC, image chunks type 0xfffd0002). xcursorgen is
not available in this VM or on Arch's repos under that name, so
this script writes the format directly — pixels are premultiplied
ARGB32, the way pixman expects them. Re-run after editing the
source PNG; the script self-verifies the files it writes.

Install: vile-install.sh copies assets/cursor-theme/vile to
/usr/share/icons/vile; the ISO stages the same tree at
archiso/airootfs/usr/share/icons/vile.
"""
import os
import struct
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "cursor-purple-arrow.png")
SRC_SIZE = 213
SRC_HOT = (8, 8)
SIZES = (24, 32)
NAMES = ("default", "left_ptr", "pointer")
OUT_DIR = os.path.join(HERE, "cursor-theme", "vile", "cursors")

XCURSOR_MAGIC = 0x72756358  # "Xcur" as a little-endian uint32:
# the file must START with the bytes 58 63 75 72 ("Xcur").
# 0x58637572 is the same letters read big-endian — that writes
# "rucX" and wlroots rejects the file (xcursor_read_file_header
# magic check). Found by strict-parse against wlroots 0.19.3
# xcursor/xcursor.c, Nov 11 2026.
XCURSOR_IMAGE_TYPE = 0xFFFD0002
XCURSOR_IMAGE_HEADER_LEN = 36  # chunk header (16) + image header (20)


def premultiply(im):
    """RGBA -> list of premultiplied ARGB32 uints (row-major)."""
    px = []
    for (r, g, b, a) in im.getdata():
        if a == 0:
            px.append(0)
        elif a == 255:
            px.append((0xFF << 24) | (r << 16) | (g << 8) | b)
        else:
            px.append((a << 24) | ((r * a // 255) << 16)
                      | ((g * a // 255) << 8) | (b * a // 255))
    return px


def build_xcursor(images):
    """images: [(size, width, height, xhot, yhot, [argb...]), ...]"""
    ntoc = len(images)
    header_len = 16 + 12 * ntoc
    out = bytearray()
    out += struct.pack("<4I", XCURSOR_MAGIC, 16, 1, ntoc)
    # TOC first; chunk offsets computed from the chunk sizes.
    pos = header_len
    chunks = []
    for (size, w, h, xh, yh, px) in images:
        chunks.append((size, w, h, xh, yh, px))
    for (size, w, h, xh, yh, px) in chunks:
        out += struct.pack("<3I", XCURSOR_IMAGE_TYPE, size, pos)
        pos += XCURSOR_IMAGE_HEADER_LEN + 4 * w * h
    for (size, w, h, xh, yh, px) in chunks:
        out += struct.pack("<4I", XCURSOR_IMAGE_HEADER_LEN,
                           XCURSOR_IMAGE_TYPE, size, 1)
        out += struct.pack("<5I", w, h, xh, yh, 0)  # delay 0
        out += struct.pack("<%dI" % len(px), *px)
    return bytes(out)


def verify(path, expect):
    """Parse the file back; expect: {size: (w, h, xhot, yhot)}."""
    with open(path, "rb") as f:
        data = f.read()
    magic, header, version, ntoc = struct.unpack_from("<4I", data, 0)
    assert magic == XCURSOR_MAGIC, "bad magic"
    assert header == 16 and version == 1, "bad header/version"
    got = {}
    for i in range(ntoc):
        typ, sub, pos = struct.unpack_from("<3I", data, 16 + 12 * i)
        assert typ == XCURSOR_IMAGE_TYPE, "bad chunk type"
        hlen, ctyp, csub, cver = struct.unpack_from("<4I", data, pos)
        assert hlen == XCURSOR_IMAGE_HEADER_LEN, "bad chunk header len"
        assert ctyp == typ and csub == sub, "TOC/chunk mismatch"
        w, h, xh, yh, delay = struct.unpack_from("<5I", data, pos + 16)
        got[sub] = (w, h, xh, yh)
        # An opaque interior pixel must stay the purple, not black.
        # Centre of the arrow body at 24px is around (12, 14).
        if sub == 24:
            px = struct.unpack_from("<%dI" % (w * h), data,
                                    pos + 36)[14 * w + 12]
            a = (px >> 24) & 0xFF
            assert a > 200, "centre pixel not opaque (%d)" % a
            r = (px >> 16) & 0xFF
            assert 180 < r < 215, "centre pixel not purple (r=%d)" % r
    assert got == expect, "sizes/hotspots mismatch: %r != %r" % (got, expect)
    return len(data)


def main():
    src = Image.open(SRC).convert("RGBA")
    assert src.size == (SRC_SIZE, SRC_SIZE), src.size
    images = []
    expect = {}
    for size in SIZES:
        im = src.resize((size, size), Image.LANCZOS)
        xh = max(0, round(SRC_HOT[0] * size / SRC_SIZE))
        yh = max(0, round(SRC_HOT[1] * size / SRC_SIZE))
        images.append((size, size, size, xh, yh, premultiply(im)))
        expect[size] = (size, size, xh, yh)
        print("size %d: hotspot (%d,%d)" % (size, xh, yh))
    blob = build_xcursor(images)
    os.makedirs(OUT_DIR, exist_ok=True)
    for name in NAMES:
        path = os.path.join(OUT_DIR, name)
        with open(path, "wb") as f:
            f.write(blob)
        n = verify(path, expect)
        print("wrote %s (%d bytes, verified)" % (path, n))
    idx = os.path.join(os.path.dirname(OUT_DIR), "index.theme")
    with open(idx, "w") as f:
        f.write("[Icon Theme]\nName=Vile\nComment=Vile OS purple cursor\n")
    print("wrote %s" % idx)
    print("theme OK: sizes %s, hotspot (1,1), purple #C574FE" % (SIZES,))


if __name__ == "__main__":
    sys.exit(main())
