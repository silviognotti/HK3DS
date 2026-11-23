#!/usr/bin/env python3
"""Re-encode HKSP sprite sheets from RGBA8 to RGBA4 in place (halves their size on the SD card and in memory).
Handles HKZ1-wrapped files. RGB gets a 4x4 ordered dither (the 3DS tiled layout is decoded for pixel positions),
alpha is rounded.   python spr_to_rgba4.py file.spr ..."""

import sys, os, struct, zlib
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texpack import rle, MORTON

BAYER = (
    np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32)
    / 16.0
    - 0.5
) * 17.0


def unrle(buf, unit, n):
    dt = "<u2" if unit == 2 else "<u4"
    out = np.empty(n, dt)
    o = 0
    p = 0
    L = len(buf)
    while p < L and o < n:
        h = struct.unpack_from("<H", buf, p)[0]
        p += 2
        c = h & 0x7FFF
        if h & 0x8000:
            out[o : o + c] = np.frombuffer(buf, dt, 1, p)[0]
            p += unit
        else:
            out[o : o + c] = np.frombuffer(buf, dt, c, p)
            p += unit * c
        o += c
    return out


_pos = {}


def positions(w, h):
    """x, y of each texel in GPU order (8x8 tiles, morton inside)."""
    if (w, h) in _pos:
        return _pos[(w, h)]
    inv = np.empty(64, np.int32)
    inv[MORTON] = np.arange(64)
    tx, ty = w // 8, h // 8
    i = np.arange(w * h)
    t, m = i // 64, inv[i % 64]
    x = (t % tx) * 8 + m % 8
    y = (t // tx) * 8 + m // 8
    _pos[(w, h)] = (x, y)
    return x, y


def convert(path, keep8=()):
    """keep8: page numbers left at RGBA8 (soft, faint glows band visibly at 4 bits)"""
    d = open(path, "rb").read()
    wrapped = d[:4] == b"HKZ1"
    if wrapped:
        d = zlib.decompress(d[8:])
    if d[:4] != b"HKSP":
        return None
    npg = struct.unpack_from("<H", d, 4)[0]
    o = 12
    out = [d[:12]]
    changed = False
    for pi in range(npg):
        w, h, fmt, a, b, sz = struct.unpack_from("<HHBBHI", d, o)
        o += 12
        raw = d[o : o + sz]
        o += sz
        packed, f = fmt & 0x80, fmt & 0x7F
        if f != 0 or pi in keep8:
            out.append(struct.pack("<HHBBHI", w, h, fmt, a, b, sz))
            out.append(raw)
            continue
        px = unrle(raw, 4, w * h) if packed else np.frombuffer(raw, "<u4", w * h)
        by = px.view(np.uint8).reshape(-1, 4).astype(np.float32)  # A B G R bytes
        al, bl, gr, rd = by[:, 0], by[:, 1], by[:, 2], by[:, 3]
        x, y = positions(w, h)
        dz = BAYER[y % 4, x % 4]
        q = lambda c: np.clip(np.round((c + dz) * 15 / 255), 0, 15).astype(np.uint16)
        qa = np.clip(np.round(al * 15 / 255), 0, 15).astype(np.uint16)
        v = (q(rd) << 12) | (q(gr) << 8) | (q(bl) << 4) | qa
        v[qa == 0] = 0  # fully transparent texels: one value, so they RLE well
        data = v.astype("<u2").tobytes()
        pk = rle(data, 2)
        if len(pk) < len(data) * 0.9:
            out.append(struct.pack("<HHBBHI", w, h, 2 | 0x80, a, b, len(pk)))
            out.append(pk)
        else:
            out.append(struct.pack("<HHBBHI", w, h, 2, a, b, len(data)))
            out.append(data)
        changed = True
    if not changed:
        return None
    out.append(d[o:])
    nd = b"".join(out)
    if wrapped:
        nd = b"HKZ1" + struct.pack("<I", len(nd)) + zlib.compress(nd, 9)
    open(path, "wb").write(nd)
    return len(nd)


if __name__ == "__main__":
    tot0 = tot1 = 0
    for p in sys.argv[1:]:
        s0 = os.path.getsize(p)
        r = convert(p)
        if r:
            tot0 += s0
            tot1 += r
    print(f"{tot0 // 1048576} MB -> {tot1 // 1048576} MB")
