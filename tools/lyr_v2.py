# Convert .lyr files (RLE tiles, optionally HKZ1-wrapped) to HKL2: every tile stored as its own zlib stream,
# so the 3DS keeps rooms compressed in RAM and inflates tiles on demand.
import sys, struct, zlib


def unwrap(d):
    if d[:4] == b"HKZ1":
        return zlib.decompress(d[8:])
    return d


def unrle(b):
    out = []
    q = 0
    while q < len(b):
        h = struct.unpack_from("<H", b, q)[0]
        q += 2
        if h & 0x8000:
            out.append(b[q : q + 2] * (h & 0x7FFF))
            q += 2
        else:
            out.append(b[q : q + 2 * h])
            q += 2 * h
    return b"".join(out)


def convert_layer(p):
    d = unwrap(open(p, "rb").read())
    if d[:4] == b"HKL2":
        return len(d), True
    assert d[:4] == b"HKLY", p
    n, T, S = struct.unpack_from("<HHH", d, 4)
    o = 10
    out = [b"HKL2", struct.pack("<HHH", n, T, S)]
    hdr = "<ffffHHHBB"
    for L in range(n):
        F, x0, y0, res, tx, ty, nt, fmt, front = struct.unpack_from(hdr, d, o)
        o += struct.calcsize(hdr)
        out.append(struct.pack(hdr, F, x0, y0, res, tx, ty, nt, fmt, front))
        for t in range(nt):
            i, j, sz = struct.unpack_from("<HHI", d, o)
            o += 8
            if sz == 0:
                out.append(struct.pack("<HHI", i, j, 0))
                out.append(d[o : o + 4])
                o += 4
                continue
            raw = unrle(d[o : o + sz])
            o += sz
            z = zlib.compress(raw, 9)
            out.append(struct.pack("<HHI", i, j, len(z)))
            out.append(z)
    data = b"".join(out)
    open(p, "wb").write(data)
    return len(data), False


if __name__ == "__main__":
    for path in sys.argv[1:]:
        convert_layer(path)
