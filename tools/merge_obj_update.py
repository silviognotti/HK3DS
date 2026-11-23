#!/usr/bin/env python3
"""Bring installed room objects (romfs/rooms/<Room>.obj) up to date with a fresh export without repacking sprites:
copies each root's world z (parallax and draw order), the AudioSource components ('as': looping and on-load
sounds) and Animator clips ('uan', uanim.py), then converts any new sounds. A room whose object list differs from the fresh export is left alone and
reported (pack it with pack_objs.py instead).
   python merge_obj_update.py <fresh export dir> <romfs> Room1 [Room2 ...]"""

import sys, os, json, zlib, struct

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pack_objs import drop_skipped_roots, audio_names, convert_audio


def load_obj(p):
    d = open(p, "rb").read()
    if d[:4] == b"HKZ1":
        d = zlib.decompress(d[8:])
    return json.loads(d)


def save_obj(p, tree):
    raw = json.dumps(tree, separators=(",", ":")).encode()
    open(p, "wb").write(b"HKZ1" + struct.pack("<I", len(raw)) + zlib.compress(raw, 9))


def merge_objects(dst, src, roots_z):
    """dst/src: lists of objects with the same names in the same order. Returns (#z changed, #sources added)."""
    nz = na = 0
    for a, b in zip(dst, src):
        if (
            roots_z
            and a.get("p") == -1
            and abs(float(a.get("z", 0)) - float(b.get("z", 0))) > 1e-4
        ):
            a["z"] = b["z"]
            nz += 1
        add = [c for c in b["c"] if c.get("k") in ("as", "uan")]
        if add or any(c.get("k") in ("as", "uan") for c in a["c"]):
            a["c"] = [
                c for c in a["c"] if c.get("k") not in ("as", "uan")
            ] + add  # appended: component indices stay put
            na += len(add)
    return nz, na


def tk_tables(tree):
    """Per-frame tk2d data the runtime needs, read from the sprite collections:
    tkbox: tk2d sprites with a box collider resize that BoxCollider2D to each frame's box (tk2dBaseSprite.UpdateCollider):
      the Goam's box grows as it bursts out. {"<lib8>:<clip>": [[cx, cy, ex, ey] or 0 per frame]} for the clips of
      objects that have a box collider.
    tksid: the collection sprite id of each frame, for Tk2dSpriteGetId (Myla's pickaxe sound fires on sprite 20),
      only in rooms whose scripts ask for it. {"<lib8>:<clip>": [id per frame]}"""
    import tk2d
    from hkcommon import metamap

    gm = metamap("GameObject")
    boxes, sids = {}, {}
    want_sid = "Tk2dSpriteGetId" in json.dumps(tree.get("fsms") or {}) + json.dumps(
        tree.get("prefabs") or {}
    )
    blibs, alibs = set(), set()
    for tr in [tree] + [v for v in (tree.get("prefabs") or {}).values() if v]:
        for o in tr["objects"]:
            ks = {c.get("k"): c for c in o["c"]}
            if "anim" in ks and ks["anim"].get("lib"):
                alibs.add(ks["anim"]["lib"])
                if any(c.get("k") == "col" and c.get("t") == "box" for c in o["c"]):
                    blibs.add(ks["anim"]["lib"])
    for g in (alibs if want_sid else blibs):
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
            row, any_box, ids = [], False, []
            for cg, sid, _ in c["frames"]:
                ids.append(sid)
                coll = tk2d.collection_by_guid(cg)
                sp = (
                    coll["sprites"][sid]
                    if coll and 0 <= sid < len(coll["sprites"])
                    else None
                )
                if sp and sp.get("coltype") == 2 and len(sp.get("colverts") or []) >= 2:
                    (cx, cy), (ex, ey) = sp["colverts"][0], sp["colverts"][1]
                    row.append([round(cx, 4), round(cy, 4), round(ex, 4), round(ey, 4)])
                    any_box = True
                else:
                    row.append(0)
            if any_box and g in blibs:
                boxes[f"{g[:8]}:{cname}"] = row
            if want_sid:
                sids[f"{g[:8]}:{cname}"] = ids
    return boxes, sids


def put_tables(dst):
    tb, ts = tk_tables(dst)
    for k, v in (("tkbox", tb), ("tksid", ts)):
        if v:
            dst[k] = v
        else:
            dst.pop(k, None)
    return tb, ts


EFFECT_CLIPS = {
    (None, "Splat"): "splat_explode_orange",
    ("Fireball Hit", "fireball_impact_effect0003"): "spell_impact",
    ("Stun Effect", "Stun Effect"): "stun_impact_effect000",
    ("mega_mantis_tall_slash", "slash_core"): "mega_mantis_slash_big",
}


def add_uanims(tree):
    """Sprites animated by a Mecanim Animator get its clip ('uan'), found from the sprite and the object's name
    (uanim.find_clip needs both to agree here, since the installed data no longer says which objects had an Animator).
    """
    import uanim

    n = 0
    for tr in [tree] + [v for v in (tree.get("prefabs") or {}).values() if v]:
        for o in tr["objects"]:
            if tr is tree:
                o["c"] = [
                    c for c in o["c"] if not (c.get("k") == "uan" and not c.get("loop"))
                ]
            if any(c.get("k") == "uan" for c in o["c"]):
                continue
            sg = next((c.get("sp") for c in o["c"] if c.get("k") == "sr"), None)
            if not sg:
                continue
            on = uanim.norm(o["n"])
            cl = [
                c
                for c in uanim.index()
                if any(g == sg for t, g in c["spr"])
                and (
                    uanim.norm(c["name"]) == on
                    or (len(on) > 5 and on in uanim.norm(c["name"]))
                    or (len(uanim.norm(c["name"])) > 5 and uanim.norm(c["name"]) in on)
                )
            ]
            # in a scene only looping clips (pulsing blobs...): a one-shot clip there is a state something else starts
            # (a door opening), not what plays on load; spawned effects (prefabs) play theirs when they appear
            if tr is tree:
                cl = [c for c in cl if c["loop"]]
            if not cl and tr is not tree:
                # spawned effects whose names don't name their clip (checked by hand against the sprites they flip):
                # a barrel's or small corpse's "Splat" is the orange infection splat (the game's default blood),
                # the spell's wall/enemy impact, the stun burst and the Mantis Lords' slash
                want = EFFECT_CLIPS.get(
                    (tr["objects"][0]["n"], o["n"])
                ) or EFFECT_CLIPS.get((None, o["n"]))
                if want:
                    cl = [
                        c
                        for c in uanim.index()
                        if c["name"] == want and any(g == sg for t, g in c["spr"])
                    ]
            if cl:
                o["c"].append(uanim.to_comp(cl[0]))
                n += 1
    return n


def same(a, b):
    return len(a) == len(b) and all(
        x["n"] == y["n"] and x.get("p") == y.get("p") for x, y in zip(a, b)
    )


def merge(srcdir, romfs, room):
    sp, dp = f"{srcdir}/{room}.obj.json", f"{romfs}/rooms/{room}.obj"
    if not os.path.exists(dp):
        return f"{room}: skipped (not installed)"
    if not os.path.exists(sp):  # no fresh export: only the per-frame collider table
        dst = load_obj(dp)
        tb, ts = put_tables(dst)
        nu = add_uanims(dst)
        save_obj(dp, dst)
        return f"{room}: no export, sized clips {len(tb)}, sprite ids {len(ts)}, animators {nu}"
    src = drop_skipped_roots(json.load(open(sp)))
    dst = load_obj(dp)
    if not same(dst["objects"], src["objects"]):
        return f'{room}: MISMATCH {len(dst["objects"])} vs {len(src["objects"])}'
    nz, na = merge_objects(dst["objects"], src["objects"], True)
    for k, pf in (dst.get("prefabs") or {}).items():
        q = (src.get("prefabs") or {}).get(k)
        if pf and q and same(pf["objects"], q["objects"]):
            na += merge_objects(pf["objects"], q["objects"], False)[1]
    tb, ts = put_tables(dst)
    nu = add_uanims(dst)
    names = set()
    audio_names(dst, names)
    dst["audio"] = sorted(names)
    save_obj(dp, dst)
    n = convert_audio(names, f"{romfs}/sfx")
    return f"{room}: z {nz}, sounds {na}, sized clips {len(tb)}, sprite ids {len(ts)}, animators {nu}, new pcm {n}"


if __name__ == "__main__":
    srcdir, romfs = sys.argv[1], sys.argv[2]
    for r in sys.argv[3:]:
        print(merge(srcdir, romfs, r), flush=True)
