# tk2d sprite collection + animation library reader.
import functools, os, math
from hkcommon import *


@functools.lru_cache(None)
def load_collection(prefab):
    objs = load_yaml_docs(prefab)
    for t, v in objs.values():
        if t == "MonoBehaviour" and "spriteDefinitions" in v:
            sprites = []
            for sd in v["spriteDefinitions"]:
                pos = [(f(p["x"]), f(p["y"])) for p in (sd.get("positions") or [])]
                uvs = [(f(p["x"]), f(p["y"])) for p in (sd.get("uvs") or [])]
                cv = [
                    (f(p["x"]), f(p["y"])) for p in (sd.get("colliderVertices") or [])
                ]
                pc = [
                    [(f(p["x"]), f(p["y"])) for p in (pg.get("points") or [])]
                    for pg in (sd.get("polygonCollider2D") or [])
                    if isinstance(pg, dict)
                ]
                sprites.append(
                    dict(
                        name=sd.get("name"),
                        pos=pos,
                        uvs=uvs,
                        flipped=int(sd.get("flipped") or 0),
                        mat=int(sd.get("materialId") or 0),
                        coltype=int(sd.get("colliderType") or 0),
                        colverts=cv,  # 0 unset, 1 none, 2 box: [centre, half extents], 3 mesh
                        phys=int(sd.get("physicsEngine") or 0),
                        poly2d=pc,
                    )
                )  # physicsEngine 1 = Physics2D; mesh outlines for it
            texs = [x.get("guid") for x in (v.get("textures") or [])]
            return dict(
                name=v.get("spriteCollectionName"),
                sprites=sprites,
                textures=texs,
                prefab=prefab,
            )
    return None


def collection_by_guid(guid):
    p = metamap("GameObject").get(guid)
    return load_collection(p) if p else None


@functools.lru_cache(None)
def load_library(prefab):
    objs = load_yaml_docs(prefab)
    for t, v in objs.values():
        if t == "MonoBehaviour" and "clips" in v:
            clips = {}
            for c in v["clips"]:
                fr = [
                    (
                        x["spriteCollection"].get("guid"),
                        int(x["spriteId"]),
                        int(x.get("triggerEvent") or 0),
                    )
                    for x in (c.get("frames") or [])
                ]
                clips[c["name"]] = dict(
                    fps=f(c.get("fps", 12)),
                    loopStart=int(c.get("loopStart") or 0),
                    wrap=int(c.get("wrapMode") or 0),
                    frames=fr,
                )
            return clips
    return None


def collection_texture(coll, idx=0):
    if coll["name"] == "Knight":
        return KNIGHT_ATLAS
    g = coll["textures"][idx] if idx < len(coll["textures"]) else None
    return texture_path(g) if g else None


def render_sprite(img, spr):
    """Crop a tk2d sprite out of atlas `img` (PIL). Returns (PIL image upright, x0, y1 in units)."""
    from PIL import Image

    W, H = img.size
    us = [u for u, v in spr["uvs"]]
    vs = [v for u, v in spr["uvs"]]
    u0, u1, v0, v1 = min(us), max(us), min(vs), max(vs)
    box = (round(u0 * W), round((1 - v1) * H), round(u1 * W), round((1 - v0) * H))
    c = img.crop(box)
    if spr["flipped"]:
        c = c.transpose(Image.Transpose.TRANSVERSE)
    xs = [p[0] for p in spr["pos"]]
    ys = [p[1] for p in spr["pos"]]
    return c, min(xs), max(ys), max(xs) - min(xs), max(ys) - min(ys)


_AC = None


def match_texture_for(coll, idx=0):
    """Find the hksprites texture matching a tk2d collection's UV layout (for textures with no .meta)."""
    global _AC
    import pickle, numpy as np

    if _AC is None:
        _AC = pickle.load(open(f"{ROOT}/hs_alpha4.pkl", "rb"))
    rects = []
    for s in coll["sprites"]:
        if s["mat"] != idx or not s["uvs"]:
            continue
        us = [u for u, v in s["uvs"]]
        vs = [v for u, v in s["uvs"]]
        rects.append((min(us), min(vs), max(us), max(vs)))
    best = None
    cands = []
    for p, (w, h, a) in _AC.items():
        if not os.path.basename(p).startswith("atlas"):
            continue
        ins = []
        cover = np.zeros_like(a)
        for u0, v0, u1, v1 in rects:
            x0 = int(u0 * w / 4)
            x1 = int(u1 * w / 4) + 1
            y0 = int((1 - v1) * h / 4)
            y1 = int((1 - v0) * h / 4) + 1
            sub = a[y0:y1, x0:x1]
            if sub.size:
                ins.append(sub.mean())
            cover[y0:y1, x0:x1] = True
        if not ins:
            continue
        ring = []
        for u0, v0, u1, v1 in rects[:400]:
            x0 = int(u0 * w / 4) - 1
            x1 = int(u1 * w / 4) + 1
            y0 = int((1 - v1) * h / 4) - 1
            y1 = int((1 - v0) * h / 4) + 1
            if y0 >= 0:
                ring.append(a[y0, max(0, x0) : x1].mean())
            if y1 < a.shape[0]:
                ring.append(a[y1, max(0, x0) : x1].mean())
            if x0 >= 0:
                ring.append(a[max(0, y0) : y1, x0].mean())
            if x1 < a.shape[1]:
                ring.append(a[max(0, y0) : y1, x1].mean())
        sc = (
            np.mean(ins)
            - (a[~cover].mean() if (~cover).any() else 0)
            - 0.7 * (np.mean(ring) if ring else 0)
        )
        cands.append((sc, p))
        if best is None or sc > best[0]:
            best = (sc, p)
    cands.sort(reverse=True)
    print("    top:", [(round(c[0], 3), os.path.basename(c[1])) for c in cands[:4]])
    return best


_tex_override = {
    (
        "Dialogue Cln",
        0,
    ): f"{ROOT}/hs/Texture2D/atlas0_317.png",  # verified by eye (score 0.24)
    # just under the 0.3 the matcher trusts on its own, each verified by eye (frames rendered from the atlas):
    # these had been left blank (1x1 frames): the Mantis Warriors were invisible, the Collector too...
    ("Mantis_v02", 0): f"{ROOT}/hs/Texture2D/atlas0_245.png",  # 0.286
    ("Collector Cln", 0): f"{ROOT}/hs/Texture2D/atlas0_288.png",  # 0.282
    (
        "Kings Guard Cln",
        0,
    ): f"{ROOT}/hs/Texture2D/atlas0_104.png",  # 0.300 (Kingsmoulds)
    ("Abyss Tendrils Cln", 0): f"{ROOT}/hs/Texture2D/atlas0_143.png",  # 0.223
    ("Waterways Crank", 0): f"{ROOT}/hs/Texture2D/atlas0_113.png",  # 0.229
    ("Debris", 0): f"{ROOT}/hs/Texture2D/atlas0_160.png",  # 0.254 (breakable poles)
    ("UI Journal Cln", 0): f"{ROOT}/hs/Texture2D/atlas0_322.png",
}  # 0.289 (the journal's quill)


def collection_texture(coll, idx=0):
    if coll["name"] == "Knight":
        return KNIGHT_ATLAS
    g = coll["textures"][idx] if idx < len(coll["textures"]) else None
    p = texture_path(g) if g else None
    if p:
        return p
    key = (coll["name"], idx)
    if key not in _tex_override:
        b = match_texture_for(coll, idx)
        _tex_override[key] = b[1] if b and b[0] > 0.3 else None
        print(
            "  matched texture for",
            coll["name"],
            idx,
            "->",
            os.path.basename(b[1]) if b else None,
            round(b[0], 3) if b else None,
        )
    return _tex_override[key]
