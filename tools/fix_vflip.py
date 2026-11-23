# One-off converter: older builds stored every texture bottom row first (vertically flipped).
# The PICA200 / tex3ds convention is top row first (memory row 0 = top of the image, sampled at v = 1),
# so this flips the pixel data of every texture page in .spr (HKSP) and .lyr (HKL2) files in place.
# Run it ONCE on data made before the texpack.encode fix. Files are HKZ1-wrapped in romfs; that is handled.
import sys, struct, zlib
import os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texpack import MORTON, rle

INV = np.argsort(MORTON)  # memory slot -> pixel index within the 8x8 tile


def flip_tiled(raw, w, h, bpp):
    t = np.frombuffer(raw, np.uint8).reshape(h // 8, w // 8, 64, bpp)
    px = t[:, :, MORTON, :]  # back to row-major pixels per tile
    img = (
        px.reshape(h // 8, w // 8, 8, 8, bpp)
        .transpose(0, 2, 1, 3, 4)
        .reshape(h, w, bpp)
    )
    img = img[::-1]
    t2 = (
        img.reshape(h // 8, 8, w // 8, 8, bpp)
        .transpose(0, 2, 1, 3, 4)
        .reshape(h // 8, w // 8, 64, bpp)
    )
    out = np.empty_like(t2)
    out[:, :, MORTON, :] = t2
    return out.tobytes()


def unrle(b, unit):
    out = []
    q = 0
    while q < len(b):
        hd = struct.unpack_from("<H", b, q)[0]
        q += 2
        if hd & 0x8000:
            out.append(b[q : q + unit] * (hd & 0x7FFF))
            q += unit
        else:
            out.append(b[q : q + unit * hd])
            q += unit * hd
    return b"".join(out)


def fix_spr(d):
    npg = struct.unpack_from("<H", d, 4)[0]
    o = 12
    out = [d[:12]]
    for _ in range(npg):
        w, h, fmt, a, b, sz = struct.unpack_from("<HHBBHI", d, o)
        o += 12
        raw = d[o : o + sz]
        o += sz
        packed = bool(fmt & 0x80)
        base = fmt & 0x7F
        unit = 4 if base == 0 else 2
        pix = unrle(raw, unit) if packed else raw
        assert len(pix) == w * h * unit, (w, h, fmt, len(pix))
        pix = flip_tiled(pix, w, h, unit)
        if packed:
            raw = rle(pix, unit)
        else:
            raw = pix
        out.append(struct.pack("<HHBBHI", w, h, fmt, a, b, len(raw)))
        out.append(raw)
    out.append(d[o:])
    return b"".join(out)


def fix_lyr(d):
    assert d[:4] == b"HKL2"
    n, T, S = struct.unpack_from("<HHH", d, 4)
    o = 10
    out = [d[:10]]
    hdr = "<ffffHHHBB"
    for _ in range(n):
        F, x0, y0, res, tx, ty, nt, fmt, front = struct.unpack_from(hdr, d, o)
        out.append(d[o : o + struct.calcsize(hdr)])
        o += struct.calcsize(hdr)
        bpp = 4 if fmt == 0 else 2
        for _ in range(nt):
            i, j, sz = struct.unpack_from("<HHI", d, o)
            o += 8
            if sz == 0:
                out.append(d[o - 8 : o + 4])
                o += 4
                continue
            pix = zlib.decompress(d[o : o + sz])
            o += sz
            assert len(pix) == T * T * bpp
            z = zlib.compress(flip_tiled(pix, T, T, bpp), 9)
            out.append(struct.pack("<HHI", i, j, len(z)))
            out.append(z)
    out.append(d[o:])
    return b"".join(out)


for p in sys.argv[1:]:
    d = open(p, "rb").read()
    wrapped = d[:4] == b"HKZ1"
    if wrapped:
        d = zlib.decompress(d[8:])
    if d[:4] == b"HKSP":
        d2 = fix_spr(d)
    elif d[:4] == b"HKL2":
        d2 = fix_lyr(d)
    else:
        print("skip", p, d[:4])
        continue
    if wrapped:
        d2 = b"HKZ1" + struct.pack("<I", len(d2)) + zlib.compress(d2, 9)
    open(p, "wb").write(d2)
    print("flipped", p.split("/")[-1])
