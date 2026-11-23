# Re-pack existing HKSP files with RLE-compressed pages (in place).
import struct, sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texpack import rle

for path in sys.argv[1:]:
    d = open(path, "rb").read()
    if d[:4] != b"HKSP":
        continue
    npg = struct.unpack_from("<H", d, 4)[0]
    o = 12
    out = [d[:12]]
    before = len(d)
    for i in range(npg):
        w, h, fmt, a, b, sz = struct.unpack_from("<HHBBHI", d, o)
        o += 12
        raw = d[o : o + sz]
        o += sz
        if not (fmt & 0x80):
            unit = 4 if fmt == 0 else 2
            pk = rle(raw, unit)
            if len(pk) < len(raw) * 0.9:
                raw = pk
                fmt |= 0x80
        out.append(struct.pack("<HHBBHI", w, h, fmt, a, b, len(raw)))
        out.append(raw)
    out.append(d[o:])
    data = b"".join(out)
    open(path, "wb").write(data)
    print(path.split("/")[-1], before // 1024, "->", len(data) // 1024, "KB")
