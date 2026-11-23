import struct, sys, numpy as np
import os
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texpack import MORTON


def decode(data, w, h, fmt):
    if fmt == 0:
        px = np.frombuffer(data, np.uint8).reshape(h // 8, w // 8, 64, 4)
    else:
        px = np.frombuffer(data, "<u2").reshape(h // 8, w // 8, 64, 1)
    t = px[:, :, MORTON, :]
    t = (
        t.reshape(h // 8, w // 8, 8, 8, -1)
        .transpose(0, 2, 1, 3, 4)
        .reshape(h, w, -1)[::-1]
    )
    if fmt == 0:
        return np.stack([t[..., 3], t[..., 2], t[..., 1], t[..., 0]], -1)
    v = t[..., 0].astype(np.uint32)
    if fmt == 2:
        return np.stack(
            [
                (v >> 12 & 15) * 17,
                (v >> 8 & 15) * 17,
                (v >> 4 & 15) * 17,
                (v & 15) * 17,
            ],
            -1,
        ).astype(np.uint8)


def read(path):
    d = open(path, "rb").read()
    assert d[:4] == b"HKSP"
    npg, nf, na, nn = struct.unpack_from("<HHHH", d, 4)
    o = 12
    pages = []
    for i in range(npg):
        w, h, fmt, _, _, sz = struct.unpack_from("<HHBBHI", d, o)
        o += 12
        raw = d[o : o + sz]
        if fmt & 0x80:
            fmt &= 0x7F
            unit = 4 if fmt == 0 else 2
            out = bytearray()
            q = 0
            while q < len(raw):
                hd = struct.unpack_from("<H", raw, q)[0]
                q += 2
                n = hd & 0x7FFF
                if hd & 0x8000:
                    out += raw[q : q + unit] * n
                    q += unit
                else:
                    out += raw[q : q + n * unit]
                    q += n * unit
            raw = bytes(out)
        pages.append(Image.fromarray(decode(raw, w, h, fmt), "RGBA"))
        o += sz
    return pages


if __name__ == "__main__":
    for i, p in enumerate(read(sys.argv[1])):
        p.save(f"{sys.argv[2]}_{i}.png")
