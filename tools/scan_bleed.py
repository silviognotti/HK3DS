#!/usr/bin/env python3
"""Find rooms with atlas bleed: sprites whose own (tight) mesh leaves out opaque pixels of their rect, i.e. pixels of
other sprites packed into their empty corners, which used to be drawn as part of them.
   python scan_bleed.py <objs json dir> <room list file> > bleed.txt     (prints "<room> <n bleeding sprites> <names>")
"""

import sys, os, json, re, pickle

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B, build_room as BR

OBJ = sys.argv[1]
rooms = [l.strip() for l in open(sys.argv[2]) if l.strip()]
CACHE = os.path.join(os.environ.get("HKDATA", "hkdata"), "bleed_cache.pkl")
cache = pickle.load(open(CACHE, "rb")) if os.path.exists(CACHE) else {}
GUID = re.compile(r"m_Sprite: \{fileID: \d+, guid: ([0-9a-f]{32})")


def bleed(g):
    if g in cache:
        return cache[g]
    r = None
    try:
        sp = B.sprite_asset(g)
        if sp and B.sprite_mesh(sp):
            rd = sp["m_RD"]
            tp = B.texture_path(rd["texture"].get("guid"))
            if tp:
                saved = B.tight_mask
                B.tight_mask = lambda s, im, *a: im
                raw = B.sprite_image(sp)
                B.tight_mask = saved
                if raw:
                    im = raw[0]
                    m = B.tight_mask(sp, im, *raw[1:3], *raw[5:8])
                    a0 = sum(1 for v in im.getchannel("A").getdata() if v > 40)
                    a1 = sum(1 for v in m.getchannel("A").getdata() if v > 40)
                    if a0 - a1 > max(60, a0 * 0.01):
                        r = (str(sp.get("m_Name")), a0 - a1, a0)
    except Exception as e:
        r = None
    cache[g] = r
    return r


for i, room in enumerate(rooms):
    sp = BR.scene_path(room)
    gs = set()
    if sp:
        gs |= set(GUID.findall(open(sp, encoding="utf-8", errors="ignore").read()))
    jp = os.path.join(OBJ, room + ".obj.json")
    if os.path.exists(jp):
        gs |= set(json.load(open(jp)).get("sprites") or [])
    bad = [b for b in (bleed(g) for g in sorted(gs)) if b]
    print(room, len(bad), " ".join(f"{n}:{d}/{a}" for n, d, a in bad[:6]), flush=True)
    if i % 10 == 9:
        pickle.dump(cache, open(CACHE, "wb"))
pickle.dump(cache, open(CACHE, "wb"))
