# Audit room transitions in romfs: each gate's target, whether the target room/gate exists, and the reverse link.
import sys, os, glob, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from debug_overlay import load_room

d = sys.argv[1] if len(sys.argv) > 1 else "romfs/rooms"
R = {}
for p in sorted(glob.glob(os.path.join(d, "*.room"))):
    rid = os.path.basename(p)[:-5]
    W, H, res, grid, ents = load_room(p)
    R[rid] = {e["name"]: e for e in ents if e["kind"] == "gate"}
out = []
for rid, gates in R.items():
    for gname, g in sorted(gates.items()):
        t = g["target"]
        if not t:
            status = "NO_TARGET"
        else:
            troom, tgate = (t.split(":") + [""])[:2]
            if troom not in R:
                status = "TARGET_ROOM_MISSING " + troom
            elif tgate and tgate not in R[troom]:
                status = f"TARGET_GATE_MISSING {troom}:{tgate} (has {sorted(R[troom])})"
            else:
                back = R[troom].get(tgate, {}).get("target", "") if tgate else ""
                status = (
                    "OK" if back == f"{rid}:{gname}" else f"OK_ONEWAY (back={back!r})"
                )
        out.append(
            (
                rid,
                gname,
                t,
                status,
                round(g["x"], 1),
                round(g["y"], 1),
                round(g["w"], 1),
                round(g["h"], 1),
            )
        )
for o in out:
    if not o[3].startswith("OK") or o[3].startswith("OK_ONEWAY"):
        print(*o)
import collections

print(collections.Counter(o[3].split()[0] for o in out))
json.dump(out, open("/tmp/gate_audit.json", "w"))
