#!/usr/bin/env python3
"""Split a room's layer tiles to 128x128 (HKL2, optionally HKZ1-wrapped; written back HKL2, re-wrap with zwrap.py).
A converted file is already 128 and is left alone. Empty quarters are dropped, uniform opaque ones become solid tiles.
    python3 lyr_split.py <file.lyr> [...]"""

import sys, struct, zlib
import numpy as np


def unwrap(d):
    return zlib.decompress(d[8:]) if d[:4] == b"HKZ1" else d


_M = None


def morton_index():
    """position (y*8+x) within an 8x8 block -> index in the block's Morton (tex3ds) order"""
    global _M
    if _M is None:
        m = np.zeros(64, int)
        for k in range(64):
            x = (k & 1) | ((k >> 1) & 2) | ((k >> 2) & 4)
            y = ((k >> 1) & 1) | ((k >> 2) & 2) | ((k >> 3) & 4)
            m[y * 8 + x] = k
        _M = m
    return _M


def unmorton(a, T):
    m = morton_index()
    blocks = a.reshape(T // 8, T // 8, 64)[:, :, m]  # [by, bx, y*8+x]
    return blocks.reshape(T // 8, T // 8, 8, 8).transpose(0, 2, 1, 3).reshape(T, T)


def morton(img):
    T = img.shape[0]
    m = morton_index()
    blocks = (
        img.reshape(T // 8, 8, T // 8, 8)
        .transpose(0, 2, 1, 3)
        .reshape(T // 8, T // 8, 64)
    )
    out = np.zeros_like(blocks)
    out[:, :, m] = blocks
    return out.reshape(-1)


def split(path, NT=128):
    d = unwrap(open(path, "rb").read())
    assert d[:4] == b"HKL2", path
    n, T, S = struct.unpack_from("<HHH", d, 4)
    if T == NT:
        return False
    k = T // NT
    o = 10
    out = [b"HKL2", struct.pack("<HHH", n, NT, S)]
    hdr = "<ffffHHHBB"
    for L in range(n):
        F, x0, y0, res, tx, ty, nt, fmt, front = struct.unpack_from(hdr, d, o)
        o += struct.calcsize(hdr)
        tiles = []
        for t in range(nt):
            i, j, sz = struct.unpack_from("<HHI", d, o)
            o += 8
            if sz == 0:  # a solid tile: four solid quarters
                c = d[o : o + 4]
                o += 4
                for b in range(k):
                    for a in range(k):
                        tiles.append((i * k + a, j * k + b, ("solid", c)))
                continue
            raw = zlib.decompress(d[o : o + sz])
            o += sz
            if fmt == 0:  # 32-bit tile: split the same way, four bytes a pixel
                px = np.frombuffer(raw, "<u4")[: T * T]
                img = unmorton(px.astype(np.uint64), T).astype(np.uint32)
            else:
                img = unmorton(np.frombuffer(raw, "<u2")[: T * T], T)
            for b in range(k):
                for a in range(k):
                    q = img[b * NT : (b + 1) * NT, a * NT : (a + 1) * NT]
                    if fmt == 0:
                        if not q.any():
                            continue
                        tiles.append(
                            (
                                i * k + a,
                                j * k + b,
                                zlib.compress(
                                    morton(np.ascontiguousarray(q))
                                    .astype("<u4")
                                    .tobytes(),
                                    9,
                                ),
                            )
                        )
                        continue
                    if not (q & 0xF).any() and not (q >> 4).any():
                        continue  # nothing there
                    if (q & 0xF).min() == 15 and q.min() == q.max():
                        v = int(q.flat[0])
                        c = bytes(
                            [
                                ((v >> 12) & 15) * 17,
                                ((v >> 8) & 15) * 17,
                                ((v >> 4) & 15) * 17,
                                255,
                            ]
                        )
                        tiles.append((i * k + a, j * k + b, ("solid", c)))
                        continue
                    tiles.append(
                        (
                            i * k + a,
                            j * k + b,
                            zlib.compress(
                                morton(np.ascontiguousarray(q)).astype("<u2").tobytes(),
                                9,
                            ),
                        )
                    )
        out.append(
            struct.pack(hdr, F, x0, y0, res, tx * k, ty * k, len(tiles), fmt, front)
        )
        for i, j, data in tiles:
            if isinstance(data, tuple):
                out.append(struct.pack("<HHI", i, j, 0))
                out.append(data[1])
            else:
                out.append(struct.pack("<HHI", i, j, len(data)))
                out.append(data)
    open(path, "wb").write(b"".join(out))
    return True


if __name__ == "__main__":
    for p in sys.argv[1:]:
        print(p.split("/")[-1], "split" if split(p) else "already 128")
