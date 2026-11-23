#!/usr/bin/env python3
"""Fill in the exits of installed rooms: every gate whose TransitionPoint leads to a room that's in romfs gets its
"Room:gate" target (build_room.py only links rooms built in the same batch).   python fill_gate_targets.py <romfs/rooms> <build dir>
"""

import sys, os, glob, json, struct, zlib

R, B = sys.argv[1], sys.argv[2]
FMT = "<BBHffffhh32s32s96s"
ES = struct.calcsize(FMT)
GATE = 8
TOFF = struct.calcsize("<BBHffffhh32s")
have = {os.path.basename(p)[:-5] for p in glob.glob(os.path.join(R, "*.room"))}
fixed = 0
# doors whose own Door Control script holds the destination (Salubra's, the Black Egg Temple's...)
DOORS = {}
door_targets = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "data", "door_targets.txt"
)
if os.path.isfile(door_targets):
    for l in open(door_targets):
        if l.strip() and not l.startswith("#"):
            r, g, t = l.split()
            DOORS[(r, g)] = t
for p in sorted(glob.glob(os.path.join(R, "*.room"))):
    room = os.path.basename(p)[:-5]
    jp = os.path.join(B, room + ".json")
    meta = json.load(open(jp)) if os.path.exists(jp) else {}
    trans = dict(meta.get("transitions", {}))
    for (r, g), t in DOORS.items():
        if r == room and not (trans.get(g) or ("", ""))[0]:
            trans[g] = tuple(t.split(":", 1))
    noexit = {
        e["name"]
        for e in meta.get("ents", [])
        if e.get("kind") == "gate" and e.get("noexit")
    }
    raw = open(p, "rb").read()
    wrapped = raw[:4] == b"HKZ1"
    data = bytearray(zlib.decompress(raw[8:]) if wrapped else raw)
    ver, W, H, n, sx, sy = struct.unpack_from("<HHHHff", data, 4)
    o = 20 + 4 * 32
    res = struct.unpack_from("<HH", data, o)[0]
    o += 4
    o += ((W * res + 7) // 8) * (H * res)
    changed = False
    for i in range(n):
        f = struct.unpack_from(FMT, data, o + i * ES)
        if f[0] != GATE:
            continue
        name = f[9].split(b"\0")[0].decode()
        tgt = f[10].split(b"\0")[0].decode()
        if tgt or name in noexit:
            continue
        t = trans.get(name)
        if not t or not t[0] or t[0] not in have:
            continue
        struct.pack_into("32s", data, o + i * ES + TOFF, f"{t[0]}:{t[1]}".encode()[:31])
        changed = True
        fixed += 1
    if changed:
        out = bytes(data)
        if wrapped:
            out = b"HKZ1" + struct.pack("<I", len(out)) + zlib.compress(out, 9)
        if not os.environ.get("CHECK_ONLY"):
            open(p, "wb").write(out)
print("filled", fixed, "exits")
