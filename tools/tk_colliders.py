#!/usr/bin/env python3
"""Per-frame tk2d collider tables for the runtime (tk2dBaseSprite.UpdateCollider, Physics2D), rewritten into romfs room
files in place:
  tkbox  {"<lib8>:<clip>": per frame [cx, cy, ex, ey] (box), 1 (no collider: tk2d switches the BoxCollider2D off) or 0}
         for the clips of objects that have a box collider. A box frame also switches the collider back on.
  tkpoly {"<lib8>:<clip>": per frame [x0, y0, x1, y1] (bounds of the frame's mesh outline), 1 (none) or 0} for objects
         with a polygon collider: tk2d swaps the PolygonCollider2D's path each frame (the Husk Guard's club swipe ends
         low in front of him; the outline saved in the scene is the high first frame).
Usage: tk_colliders.py <romfs>/rooms/*.obj"""

import sys, json

sys.path.insert(0, __import__("os").path.dirname(__file__))
from merge_obj_update import load_obj, save_obj
import tk2d
from hkcommon import metamap


def tables(tree):
    gm = metamap("GameObject")
    boxes, polys = {}, {}
    blibs, plibs = set(), set()
    for tr in [tree] + [v for v in (tree.get("prefabs") or {}).values() if v]:
        for o in tr["objects"]:
            ks = {c.get("k"): c for c in o["c"]}
            if "anim" in ks and ks["anim"].get("lib"):
                if any(c.get("k") == "col" and c.get("t") == "box" for c in o["c"]):
                    blibs.add(ks["anim"]["lib"])
                if any(c.get("k") == "col" and c.get("t") == "poly" for c in o["c"]):
                    plibs.add(ks["anim"]["lib"])
    for g in blibs | plibs:
        lp = None
        for full in tree.get("libs") or {}:
            if full.startswith(g[:8]):
                lp = gm.get(full)
        lp = lp or gm.get(g)
        lib = tk2d.load_library(lp) if lp else None
        if not lib:
            continue
        for cname, c in lib.items():
            if not cname:
                continue
            brow, prow, anyb, anyp = [], [], False, False
            for cg, sid, _ in c["frames"]:
                coll = tk2d.collection_by_guid(cg)
                sp = (
                    coll["sprites"][sid]
                    if coll and 0 <= sid < len(coll["sprites"])
                    else None
                )
                ct = sp.get("coltype") if sp else 0
                if sp and ct == 2 and len(sp.get("colverts") or []) >= 2:
                    (cx, cy), (ex, ey) = sp["colverts"][0], sp["colverts"][1]
                    brow.append(
                        [round(cx, 4), round(cy, 4), round(ex, 4), round(ey, 4)]
                    )
                    anyb = True
                elif sp and ct == 1 and sp.get("phys") == 1:
                    brow.append(1)
                    anyb = True
                else:
                    brow.append(0)
                pts = [p for pg in (sp.get("poly2d") or []) for p in pg] if sp else []
                if sp and ct == 3 and sp.get("phys") == 1 and pts:
                    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                    prow.append(
                        [
                            round(min(xs), 4),
                            round(min(ys), 4),
                            round(max(xs), 4),
                            round(max(ys), 4),
                        ]
                    )
                    anyp = True
                elif sp and ct == 1 and sp.get("phys") == 1:
                    prow.append(1)
                    anyp = True
                else:
                    prow.append(0)
            if anyb and g in blibs and any(isinstance(x, list) for x in brow):
                boxes[f"{g[:8]}:{cname}"] = brow
            if anyp and g in plibs and any(isinstance(x, list) for x in prow):
                polys[f"{g[:8]}:{cname}"] = prow
    return boxes, polys


if __name__ == "__main__":
    for p in sys.argv[1:]:
        try:
            t = load_obj(p)
        except Exception as e:
            print(p, "unreadable", e)
            continue
        b, q = tables(t)
        ob, oq = t.get("tkbox"), t.get("tkpoly")
        for k, v in (("tkbox", b), ("tkpoly", q)):
            if v:
                t[k] = v
            else:
                t.pop(k, None)
        if (ob or None) != (b or None) or (oq or None) != (q or None):
            save_obj(p, t)
            print(p.split("/")[-1], "boxes", len(b), "polys", len(q), flush=True)
