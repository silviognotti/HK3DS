# Post-process romfs .room files: give doors without a destination the return link when another room's exit points
# at them (the game enables some doors by script, e.g. Crossroads_06 door1 <- Crossroads_ShamanTemple left1).
import sys, os, glob, struct, zlib

d = sys.argv[1] if len(sys.argv) > 1 else "romfs/rooms"
FMT = "<BBHffffhh32s32s96s"
ES = struct.calcsize(FMT)
GATE = 8


def load(p):
    raw = open(p, "rb").read()
    wrapped = raw[:4] == b"HKZ1"
    data = bytearray(zlib.decompress(raw[8:]) if wrapped else raw)
    ver, W, H, n, sx, sy = struct.unpack_from("<HHHHff", data, 4)
    o = 20 + 4 * 32
    res = struct.unpack_from("<HH", data, o)[0]
    o += 4
    o += ((W * res + 7) // 8) * (H * res)
    return data, wrapped, n, o


rooms = {}
for p in glob.glob(os.path.join(d, "*.room")):
    data, wrapped, n, o = load(p)
    gates = {}
    for i in range(n):
        f = list(struct.unpack_from(FMT, data, o + i * ES))
        if f[0] == GATE:
            gates[f[9].split(b"\0")[0].decode()] = (
                o + i * ES,
                f[10].split(b"\0")[0].decode(),
            )
    rooms[os.path.basename(p)[:-5]] = [p, data, wrapped, gates, False]
for rid, (p, data, wrapped, gates, _) in rooms.items():
    for g, (off, tgt) in gates.items():
        if ":" not in tgt:
            continue
        troom, tgate = tgt.split(":", 1)
        if (
            troom in rooms
            and tgate in rooms[troom][3]
            and not rooms[troom][3][tgate][1]
        ):
            toff = rooms[troom][3][tgate][0]
            struct.pack_into(
                "32s",
                rooms[troom][1],
                toff + struct.calcsize("<BBHffffhh32s"),
                f"{rid}:{g}".encode()[:31],
            )
            rooms[troom][3][tgate] = (toff, f"{rid}:{g}")
            rooms[troom][4] = True
            print(f"linked {troom}:{tgate} -> {rid}:{g}")
for rid, (p, data, wrapped, gates, changed) in rooms.items():
    if not changed:
        continue
    out = bytes(data)
    if wrapped:
        out = b"HKZ1" + struct.pack("<I", len(out)) + zlib.compress(out, 9)
    open(p, "wb").write(out)
