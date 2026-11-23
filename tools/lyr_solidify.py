#!/usr/bin/env python3
"""Drop a room's fully transparent tiles and turn its opaque single-colour 16-bit tiles into solid-colour tiles (no texture: the game fills them with a
rectangle, so they take no slot in the tile cache). The bake looks for them after dithering, and a dark colour that
isn't a 4-bit level dithers between two levels, so the black terrain (lifted a little by a scene's colour grade) was
stored as hundreds of textured tiles: in a big room like the Distant Village the Old 3DS cache couldn't hold a screen.
A tile counts as one colour when every pixel is opaque and each channel spans at most two neighbouring 4-bit levels;
the solid colour is the tile's average, which is what the dither showed.
Transparent tiles: the bake keeps a tile whose alpha reaches 4/255 somewhere, but at 4 bits alpha below 9/255 is 0, so
faint haze edges left whole grids of invisible tiles (in the Distant Village most of the foreground layers' tiles),
each one a cache slot and a texture fetch for nothing.
    python3 lyr_solidify.py <file.lyr> [...]      (HKL2 or HKZ1-wrapped; written back HKZ1-wrapped)
"""

import sys, struct, zlib
import numpy as np


def unwrap(d):
    return zlib.decompress(d[8:]) if d[:4] == b"HKZ1" else d


def wrap(d):
    return b"HKZ1" + struct.pack("<I", len(d)) + zlib.compress(d, 9)


def solidify(path):
    d = unwrap(open(path, "rb").read())
    assert d[:4] == b"HKL2", path
    n, T, S = struct.unpack_from("<HHH", d, 4)
    o = 10
    out = [d[:10]]
    hdr = "<ffffHHHBB"
    hs = struct.calcsize(hdr)
    made = total = dropped = 0
    for L in range(n):
        F, x0, y0, res, tx, ty, nt, fmt, front = struct.unpack_from(hdr, d, o)
        o += hs
        lo = []
        keep = 0
        for t in range(nt):
            i, j, sz = struct.unpack_from("<HHI", d, o)
            o += 8
            if sz == 0:
                lo.append(struct.pack("<HHI", i, j, 0))
                lo.append(d[o : o + 4])
                o += 4
                keep += 1
                continue
            blob = d[o : o + sz]
            o += sz
            total += 1
            if fmt == 0:
                if not (
                    np.frombuffer(zlib.decompress(blob), "<u4")[: T * T] & 255
                ).any():
                    dropped += 1
                    continue
            if fmt == 2:
                px = np.frombuffer(zlib.decompress(blob), "<u2")[: T * T].astype(
                    np.int32
                )
                a = px & 15
                if not a.any():
                    dropped += 1
                    continue
                if a.min() == 15:
                    ch = [(px >> s) & 15 for s in (12, 8, 4)]
                    if all(c.max() - c.min() <= 1 for c in ch):
                        col = bytes(int(round(c.mean() * 17)) for c in ch) + b"\xff"
                        lo.append(struct.pack("<HHI", i, j, 0))
                        lo.append(col)
                        made += 1
                        keep += 1
                        continue
            lo.append(struct.pack("<HHI", i, j, sz))
            lo.append(blob)
            keep += 1
        out.append(struct.pack(hdr, F, x0, y0, res, tx, ty, keep, fmt, front))
        out += lo
    open(path, "wb").write(wrap(b"".join(out)))
    return made, dropped, total


if __name__ == "__main__":
    tm = td = tt = 0
    for p in sys.argv[1:]:
        m, dr, t = solidify(p)
        tm += m
        td += dr
        tt += t
        if len(sys.argv) <= 3:
            print(
                p.split("/")[-1],
                f"of {t} textured tiles: {dr} empty, dropped; {m} now solid",
            )
    print(f"of {tt} textured tiles: {td} empty, dropped; {tm} now solid")
