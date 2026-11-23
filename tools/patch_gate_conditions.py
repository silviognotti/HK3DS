#!/usr/bin/env python3
"""Mark room exits that the game hides by PlayerData: a TransitionPoint (or one of its parents) with
DeactivateIfPlayerdataFalse / True only works while that bool is true / false (the Grimm Troupe tents in Dirtmouth).
Appends " D <bool>" / " T <bool>" to the gate's extra field in the installed .room files.
   python patch_gate_conditions.py <romfs/rooms> [Room ...]      (no rooms: every room whose scene uses those components)
"""

import sys, os, glob, struct, zlib, re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_room as BR, build_objs as BO

X = BR.X
R = sys.argv[1]
FMT = "<BBHffffhh32s32s96s"
ES = struct.calcsize(FMT)
GATE = 8
EOFF = struct.calcsize("<BBHffffhh32s32s")
GUIDS = [
    g for g, c in BO.SCRIPT_GUIDS.items() if c.startswith("DeactivateIfPlayerdata")
]
rooms = sys.argv[2:] or sorted(
    os.path.basename(p)[:-5] for p in glob.glob(os.path.join(R, "*.room"))
)


def conditions(scene):
    objs = X.load_scene(scene)
    sc = X.Scene(objs)
    tr2go = {v: k for k, v in sc.go2tr.items()}
    conds = {}

    def go_conds(g):
        out = []
        for c in objs[g][1].get("m_Component", []):
            cid = int(c["component"]["fileID"])
            if (
                cid in objs
                and objs[cid][0] == "MonoBehaviour"
                and "boolName" in objs[cid][1]
            ):
                cls = BO.classify_mb(objs[cid][1])
                b = str(objs[cid][1].get("boolName") or "")
                if b and cls == "DeactivateIfPlayerdataFalse":
                    out.append("D " + b)
                elif b and cls == "DeactivateIfPlayerdataTrue":
                    out.append("T " + b)
        return out

    for g, (t, v) in objs.items():
        if t != "GameObject" or g not in sc.go2tr:
            continue
        if not any(
            int(c["component"]["fileID"]) in objs
            and objs[int(c["component"]["fileID"])][0] == "MonoBehaviour"
            and "isADoor" in objs[int(c["component"]["fileID"])][1]
            for c in v.get("m_Component", [])
        ):
            continue
        cs, tr = [], sc.go2tr[g]
        while tr in objs:
            cs += go_conds(tr2go[tr])
            f = int(objs[tr][1]["m_Father"]["fileID"])
            tr = f
        if cs:
            conds[str(v["m_Name"])] = cs
    return conds


total = 0
for room in rooms:
    sp = BR.scene_path(room)
    p = os.path.join(R, room + ".room")
    if not sp or not os.path.exists(p):
        continue
    if (
        len(sys.argv) <= 2
    ):  # quick filter: the scene uses one of the DeactivateIfPlayerdata* scripts and has a door
        t = open(sp, encoding="utf-8", errors="ignore").read()
        if "isADoor" not in t or not any(g in t for g in GUIDS):
            continue
    conds = conditions(sp)
    if not conds:
        continue
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
        name = f[9].split(b"\0")[0].decode(errors="ignore")
        if name not in conds:
            continue
        extra = f[11].split(b"\0")[0].decode(errors="ignore")
        extra = re.sub(r" [DT] \S+", "", extra)
        extra += "".join(" " + c for c in conds[name])
        struct.pack_into("96s", data, o + i * ES + EOFF, extra.encode()[:95])
        changed = True
        total += 1
        print(room, name, conds[name], flush=True)
    if changed:
        out = bytes(data)
        if wrapped:
            out = b"HKZ1" + struct.pack("<I", len(out)) + zlib.compress(out, 9)
        open(p, "wb").write(out)
print("marked", total, "gates")
