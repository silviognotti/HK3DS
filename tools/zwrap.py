# Wrap data files in zlib ('HKZ1' u32 rawSize + zlib stream), in place. Already-wrapped files are skipped.
import sys, zlib, struct, os

def wrap_file(p):
    d = open(p, "rb").read()
    if d[:4] == b"HKZ1":
        return 0, 0
    z = b"HKZ1" + struct.pack("<I", len(d)) + zlib.compress(d, 9)
    open(p, "wb").write(z)
    return len(d), len(z)


if __name__ == "__main__":
    tot0 = tot1 = 0
    for path in sys.argv[1:]:
        size0, size1 = wrap_file(path)
        tot0 += size0
        tot1 += size1
    print(f"{tot0 // 1024} KB -> {tot1 // 1024} KB")
