#!/usr/bin/env python3
"""HD object sprite sheets for the HD (800x240) build, from the packed rooms of the standard build.
   python pack_objs_hd.py <romfs> <romfs_hd> Room [Room ...]
Reads <romfs>/rooms/<Room>.obj (the packed tree: its libraries, sprite assets, fixed tk2d frames) and writes
<romfs_hd>/rooms/<Room>_objs.spr with the same animation and image names, at twice the texels across (HK_XD=2).
The .obj itself is shared: the game looks sprites up by name, so the HD sheet slots in for the standard one.
"""

import sys, os, json, zlib, struct

os.environ.setdefault("HK_XD", "2")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkcommon import metamap
import build_sprites as BS
from build_sprites import SpriteSet, hd_fit
from tk2d import load_library
import tk2d

XD = BS.XD


def load_tree(path):
    d = open(path, "rb").read()
    if d[:4] == b"HKZ1":
        d = zlib.decompress(d[8:])
    return json.loads(d)


def sheet_names(path):
    """the animation and image names of a sheet (to check the HD one has them all)"""
    d = open(path, "rb").read()
    if d[:4] == b"HKZ1":
        d = zlib.decompress(d[8:])
    np_, nf, na, nn = struct.unpack_from("<4H", d, 4)
    o = 12
    for _ in range(np_):
        w, h, fmt, b1, _, sz = struct.unpack_from("<HHBBHI", d, o)
        o += 12 + sz
    o += nf * 16
    anims = []
    total = 0
    for _ in range(na):
        anims.append(d[o : o + 32].split(b"\0")[0].decode("latin-1"))
        total += struct.unpack_from("<H", d, o + 40)[0]
        o += 44
    o += total * 4
    names = []
    for _ in range(nn):
        names.append(d[o : o + 32].split(b"\0")[0].decode("latin-1"))
        o += 36
    return set(anims), set(names)


def build(src, dst, room):
    tree = load_tree(f"{src}/rooms/{room}.obj")
    gm = metamap("GameObject")
    ss = SpriteSet(0.25)
    for g in tree["libs"]:
        lp = gm.get(g)
        if not lp:
            continue
        lib = load_library(lp)
        if not lib:
            continue
        try:
            ss.add_library(lp, set(k for k in lib if k), g[:8] + ":")
        except Exception as ex:
            print("  lib failed", g, ex)
            continue
    # Sprite assets: 16 px per world unit, XD times the columns
    import bake_scene as B
    from texpack import premul_resize

    for g in tree.get("sprites", []):
        sa = B.sprite_asset(g)
        r = B.sprite_image(sa) if sa else None
        if not r:
            continue
        im, offx, offy, RW, RH, pvx, pvy, ppu = r
        k = 16.0 / (ppu or 100)
        w, h = max(1, round(im.width * k)), max(1, round(im.height * k))
        if w > 1024 or h > 1024:
            continue  # (the standard sheet's rule, so the same images are in both)
        oy = round(-(offy + im.height - pvy) * k)
        if (
            w * XD + 2 > 1024
        ):  # too wide for a page at HD: a standard frame (its own density in the sheet)
            ss.add_named(
                "sp:" + g[:12],
                premul_resize(im, w, h),
                round((offx - pvx) * k),
                oy,
                dens=1,
            )
            continue
        im2 = premul_resize(im, w * XD, h)
        ox = round((offx - pvx) * k * XD)
        im2, ox = hd_fit(im2, ox)
        ss.add_named("sp:" + g[:12], im2, ox, oy)
    seen = set()
    for tr in [tree] + [v for v in tree["prefabs"].values() if v]:
        for o in tr["objects"]:
            ks = [c.get("k") for c in o["c"]]
            if "sprite" not in ks or "anim" in ks:
                continue
            c = [c for c in o["c"] if c.get("k") == "sprite"][0]
            g, sid = c.get("coll"), int(c.get("id") or 0)
            if not g or (g, sid) in seen:
                continue
            seen.add((g, sid))
            try:
                fi = ss.add_tk2d_frame(g, sid)
            except Exception as ex:
                print("  tk2d frame failed", g, sid, ex)
                continue
            ss.names.append(("tk:%s:%d" % (g[:8], sid), fi))
    for tr in [tree] + [v for v in tree["prefabs"].values() if v]:
        owner = {}
        for o in tr["objects"]:
            for c in o["c"]:
                if c.get("k") == "fsm":
                    owner[c.get("i")] = o
        for fi_, f in enumerate(tr.get("fsms") or []):
            o = owner.get(fi_)
            sc = [c for c in (o or {}).get("c", []) if c.get("k") == "sprite"]
            if not sc or not sc[0].get("coll"):
                continue
            g = sc[0]["coll"]
            for st in f["states"]:
                for a in st["actions"]:
                    if not a["type"].endswith("Tk2dSpriteSetId"):
                        continue
                    pr = a.get("params") or {}
                    if pr.get("gameObject", {}).get("owner") is not True:
                        continue
                    nm, sid = pr.get("ORSpriteName"), pr.get("spriteID")
                    if isinstance(nm, str) and nm:
                        coll = tk2d.collection_by_guid(g)
                        ids = [
                            i
                            for i, sp in enumerate((coll or {}).get("sprites") or [])
                            if sp.get("name") == nm
                        ]
                        if not ids:
                            continue
                        sid = ids[0]
                    if not isinstance(sid, int) or (g, sid) in seen:
                        continue
                    seen.add((g, sid))
                    try:
                        fi = ss.add_tk2d_frame(g, sid)
                    except Exception as ex:
                        print("  tk2d frame failed", g, sid, ex)
                        continue
                    ss.names.append(("tk:%s:%d" % (g[:8], sid), fi))
    if not ss.frames:
        print(room, "no frames")
        return False
    out = f"{dst}/rooms/{room}_objs.spr"
    if os.path.exists(out):
        os.remove(out)  # (a hard link to the standard sheet: never write through it)
    ss.write(out)
    import spr_to_rgba4

    spr_to_rgba4.convert(out, keep8=(0,) if room == "_Knight" else ())
    d = open(out, "rb").read()
    if d[:4] != b"HKZ1":
        open(out, "wb").write(b"HKZ1" + struct.pack("<I", len(d)) + zlib.compress(d, 9))
    a0, n0 = sheet_names(f"{src}/rooms/{room}_objs.spr")
    a1, n1 = sheet_names(out)
    miss = (a0 - a1) | (n0 - n1)
    print(
        room,
        "HD sheet",
        os.path.getsize(out) // 1024,
        "KB",
        "MISSING %d names: %s" % (len(miss), sorted(miss)[:5]) if miss else "all names",
    )
    return not miss


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    bad = [
        r
        for r in sys.argv[3:]
        if os.path.exists(f"{src}/rooms/{r}_objs.spr") and not build(src, dst, r)
    ]
    if bad:
        print("names missing in", bad)
