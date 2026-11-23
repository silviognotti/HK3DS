#!/usr/bin/env python3
"""Particle systems on a room's live objects and on the prefabs they spawn (enemy dust and spit trails, breakable
debris, lamp motes, spell and death effects...): each becomes a {"k": "ps"} component on its object in <Room>.obj,
with its matrices relative to the object's own frame (the runtime's 2D world transform, uo_world), so the particles
follow the object when it moves, turns or flips. Textures go to <Room>_pto.spr.
    python3 obj_particles.py <romfs> Room [Room ...]
The objects are matched to the scene / prefab GameObjects in build_objs' export order (checked by name); a room or
prefab that doesn't match is left as it is."""

import sys, os, json, zlib, struct, math
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import particles as P
import bake_scene as B
import build_objs as BO
import build_room as BR
from hkcommon import metamap
from build_sprites import SpriteSet
from texpack import FMT_RGBA8


def unwrap(d):
    return zlib.decompress(d[8:]) if d[:4] == b"HKZ1" else d


def wrap(d):
    return b"HKZ1" + struct.pack("<I", len(d)) + zlib.compress(d, 9)


def frame2d(objs, i, prefab=False):
    """uo_world: (wx, wy, wsx, wsy, wrz, wz) of objects[i] from the exported values. A prefab's root is placed at the
    spawn point by the game: (0, 0) here."""
    chain = []
    while i >= 0:
        chain.append(i)
        i = objs[i]["p"]
    chain.reverse()
    r = objs[chain[0]]
    wx, wy = (0.0, 0.0) if prefab else (float(r["x"]), float(r["y"]))
    wsx, wsy, wrz = float(r["sx"]), float(r["sy"]), float(r["rz"])
    wz = 0.0 if prefab else float(r.get("z", 0))
    for k in chain[1:]:
        o = objs[k]
        a = math.radians(wrz)
        lx = float(o["x"]) * wsx
        ly = float(o["y"]) * wsy
        wx, wy = (
            wx + math.cos(a) * lx - math.sin(a) * ly,
            wy + math.sin(a) * lx + math.cos(a) * ly,
        )
        wrz = wrz + (-float(o["rz"]) if wsx * wsy < 0 else float(o["rz"]))
        wsx *= float(o["sx"])
        wsy *= float(o["sy"])
        wz += float(o.get("z", 0))
    return wx, wy, wsx, wsy, wrz, wz


def order(doc, roots):
    out = []

    def walk(g):
        out.append(g)
        for k in doc.kids(g):
            walk(k)

    for r in roots:
        walk(r)
    return out


def localize(e, W, fr, Wz_origin=0.0):
    """Bake e with world matrix W (P.place), then express A/D/R relative to the object frame fr."""
    P.place(e, W)
    wx, wy, wsx, wsy, wrz, wz = fr
    a = math.radians(wrz)
    Rm = np.array([[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]])
    sx = wsx if abs(wsx) > 1e-4 else 1e-4
    sy = wsy if abs(wsy) > 1e-4 else 1e-4
    Ui = np.linalg.inv(Rm @ np.diag([sx, sy]))
    Un = np.linalg.inv(Rm @ np.diag([1.0 if sx > 0 else -1.0, 1.0 if sy > 0 else -1.0]))
    A = np.array(e["A"]).reshape(2, 4)
    LA = np.zeros((2, 4))
    LA[:, :3] = Ui @ A[:, :3]
    LA[:, 3] = Ui @ (A[:, 3] - [wx, wy])
    LD = Un @ np.array(e["D"]).reshape(2, 3)
    LR = Un @ np.array(e["R"]).reshape(2, 3)
    e["A"] = [round(float(x), 4) for x in LA.reshape(-1)]
    e["D"] = [round(float(x), 4) for x in LD.reshape(-1)]
    e["R"] = [round(float(x), 4) for x in LR.reshape(-1)]
    s = (abs(sx) + abs(sy)) / 2
    e["ssz"] = round(e["ssz"] / s, 4) if s > 1e-4 else e["ssz"]
    e["z"] = round(
        float(e["pos"][2]) - Wz_origin - wz, 3
    )  # depth relative to the object's own
    del e["pos"]
    return e


def attach(doc, gos, objs, sheet, texcache, prefab, stats, root_tid=None):
    if len(gos) != len(objs) or any(
        str(doc.go[g].get("m_Name")) != str(o["n"]) for g, o in zip(gos, objs)
    ):
        stats["mismatch"] += 1
        return 0
    mats = P.Mats(doc.tr)
    T0 = np.eye(4)
    if prefab and root_tid is not None:
        T0[:3, 3] = -mats.world(root_tid)[:3, 3]  # the root's position: the spawn point
    n = 0
    for i, (g, o) in enumerate(zip(gos, objs)):
        comps = doc.comps.get(g, [])
        # SpriteRenderers: on the game's lit material (the scene's ambient light tints them) or an additive one
        srs = [v for fid, t, v in comps if t == "SpriteRenderer"]
        src = [c for c in o["c"] if c.get("k") == "sr"]
        for v, c in zip(srs, src):
            ml = v.get("m_Materials") or []
            guid = ml[0].get("guid") if ml and isinstance(ml[0], dict) else None
            mname = B.material_name(guid).lower() if guid else ""
            add = guid in B.ADDITIVE_MATERIALS or any(
                k in mname for k in ("additive", "add", "glow", "light")
            )  # as bake_scene does
            if (
                not mname
            ):  # material not in the export: the game's light/haze sprites use additive materials
                spn = str(c.get("sp") or "").lower()
                if (
                    spn.startswith(
                        (
                            "light_effect",
                            "haze",
                            "glow",
                            "light_ray",
                            "lightray",
                            "light_beam",
                            "light_shaft",
                            "shaft",
                        )
                    )
                    or "glow" in spn
                    or "caustic" in spn
                    or "beam" in spn
                    or "waterfall" in spn
                ):
                    add = True
            if "darkness" in mname:
                c["cut"] = 1
                stats["cut"] = stats.get("cut", 0) + 1
                continue  # Sprites-Darkness-Cutout: not drawn
            if add:
                c["add"] = 1
                stats["add"] += 1
            elif guid and B.material_lit(guid):
                c["lit"] = 1
                stats["lit"] += 1
        rend = next((v for fid, t, v in comps if t == "ParticleSystemRenderer"), None)
        for fid, t, v in comps:
            if t != "ParticleSystem":
                continue
            try:
                e = P.compile_ps(v, rend, sheet, texcache)
            except Exception as ex:
                stats["err"] += 1
                continue
            if not e:
                stats["skip"] += 1
                continue
            W = T0 @ mats.world(doc.go2tr[g])
            localize(e, W, frame2d(objs, i, prefab))
            e["k"] = "ps"
            o["c"].append(
                e
            )  # appended last: the indices of the other components stay put
            n += 1
    return n


def process(romfs, room):
    p = f"{romfs}/rooms/{room}.obj"
    if not os.path.exists(p):
        return None
    tree = json.loads(unwrap(open(p, "rb").read()))
    if tree.get("post", 0) >= 1 and not os.environ.get("PTC_FORCE"):
        return "done before"
    # a room converted by an earlier version of this script: its particle components (always the last ones) go
    for o in tree["objects"] + [
        o
        for pf in (tree.get("prefabs") or {}).values()
        if pf
        for o in pf.get("objects", [])
    ]:
        if any(c.get("k") == "ps" for c in o.get("c", [])):
            o["c"] = [c for c in o["c"] if c.get("k") != "ps"]
    stats = dict(mismatch=0, err=0, skip=0, lit=0, add=0)
    sheet = SpriteSet(1.0)
    texcache = {}
    n = 0
    # the room's own objects
    ur = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "data", "uoroots", f"{room}.txt"
    )
    sp = BR.scene_path(room)
    if os.path.exists(ur) and sp and tree["objects"]:
        doc = BO.Doc(sp)
        bypath = {}
        for g in doc.go:
            if g in doc.go2tr:
                bypath.setdefault(doc.P.path(g), g)
        roots = [bypath.get(l.strip()) for l in open(ur) if l.strip()]
        if all(r is not None for r in roots):
            n += attach(
                doc, order(doc, roots), tree["objects"], sheet, texcache, False, stats
            )
        else:
            stats["mismatch"] += 1
    # the prefabs they spawn
    gomap = metamap("GameObject")
    for guid, pf in (tree.get("prefabs") or {}).items():
        if not pf or not pf.get("objects"):
            continue
        path = gomap.get(guid)
        if not path or not path.endswith(".prefab"):
            continue
        doc = BO.Doc(path)
        roots = [g for g in doc.go if doc.parent(g) is None][:1]
        if not roots:
            continue
        n += attach(
            doc,
            order(doc, roots),
            pf["objects"],
            sheet,
            texcache,
            True,
            stats,
            doc.go2tr[roots[0]],
        )
    for ext in ("_pto.spr",):
        if os.path.exists(f"{romfs}/rooms/{room}{ext}"):
            os.remove(f"{romfs}/rooms/{room}{ext}")
    if n:
        sheet.write(f"{romfs}/rooms/{room}_pto.spr", FMT_RGBA8, 512)
    tree["post"] = 1  # particles and sprite materials added (don't run twice)
    open(p, "wb").write(wrap(json.dumps(tree, separators=(",", ":")).encode()))
    return n, stats


if __name__ == "__main__":
    rf = sys.argv[1]
    for room in sys.argv[2:]:
        print(room, process(rf, room), flush=True)
