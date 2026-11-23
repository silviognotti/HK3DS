#!/usr/bin/env python3
"""Build everything for one room: collision+entities (.room), parallax art (.lyr), entity images (_ents.spr).

HKRM format:
  'HKRM' u16 version, u16 w, u16 h, u16 nents, f32 spawnx, f32 spawny, char music[32], char ambience[32], char title[32], char subtitle[32]
  solid bits: h rows (bottom-up) of (w+7)/8 bytes
  ents: u8 kind, u8 flags, u16 pad, f32 x, y, w, h, s16 hp, s16 geo, char name[32], char target[32]
"""

import sys, os, struct, json, re, math
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import hk_extract_room as X
import bake_scene as B
from build_sprites import SpriteSet
from hkcommon import *
from texpack import premul_resize

KIND = {k: i for i, k in enumerate(X.ALL_KINDS)}

ROOMS = {
    "kings_pass": dict(
        scene="Scenes/Tutorial_01.unity",
        music="",
        ambience="cave_wind_loop",
        title="King's Pass",
        subtitle="",
        spawn=(34.5, 78.0),
        gates={"right1": ("town", "left1")},
    ),
    "town": dict(
        scene="Scenes/Town.unity",
        music="dirtmouth",
        ambience="dirtmouth_wind_loop_a",
        title="Dirtmouth",
        subtitle="",
        spawn=None,
        gates={"left1": ("kings_pass", "right1")},
    ),
}

ENEMY_HP = {"Crawler": (8, 2), "Buzzer": (8, 3)}


def build(room, out_dir, S=16):
    cfg = ROOMS[room]
    scene = f"{EX}/{cfg['scene']}"
    sc = X.Scene(X.load_scene(scene))
    W, H, grid = X.extract_tilemap(sc)
    cols = X.colliders(sc)
    ents = X.classify(sc, cols)
    # hp / geo from the scene's HealthManagers
    for e in ents:
        e["hp"] = 0
        e["geo"] = 0
        e["target"] = ""
        if e["kind"] == "enemy":
            b = X.base(e["name"]).split()[0]
            e["hp"], e["geo"] = ENEMY_HP.get(b, (8, 2))
            # sprite pivot (transform) relative to the collider centre
            for gid, g in sc.go.items():
                if (
                    str(g["m_Name"]) == e["name"]
                    and gid in sc.go2tr
                    and sc.path(gid).startswith(("_Enemies", e["name"]))
                ):
                    Wt = sc.world(sc.go2tr[gid])
                    e["target"] = "%.3f %.3f" % (Wt["x"] - e["x"], Wt["y"] - e["y"])
                    break
        elif e["kind"] == "breakable_wall":
            e["hp"] = 3
        elif e["kind"] == "geo_rock":
            e["hp"] = 5
            e["geo"] = 5
        elif e["kind"] == "chest":
            e["hp"] = 1
            e["geo"] = 30
        elif e["kind"] == "collapse_floor":
            e["hp"] = 3
        elif e["kind"] == "gate":
            t = cfg["gates"].get(e["name"])
            if t:
                e["target"] = f"{t[0]}:{t[1]}"
    # bake art
    out, ent_items, _ = B.bake(
        scene, out_dir, room, S=S, bounds=(0, 0, W, H), preview=True
    )
    # entity images: render each entity object's sprites at the gameplay plane
    ss = SpriteSet(1.0)
    for name, items in ent_items.items():
        canv = {0: np.zeros((1, 1, 4), np.float32)}
        rendered = []
        for it in sorted(
            items, key=lambda it: (it["layer"], it["order"], -it["W"]["z"])
        ):
            r = B.render_item(it, S, 1.0)
            if r:
                rendered.append((r, it["add"]))
        if not rendered:
            continue
        x0 = min(l for (im, l, t), a in rendered)
        y0 = min(t for (im, l, t), a in rendered)
        x1 = max(l + im.width for (im, l, t), a in rendered)
        y1 = max(t + im.height for (im, l, t), a in rendered)
        w, h = int(math.ceil(x1 - x0)), int(math.ceil(y1 - y0))
        if w * h > 1024 * 1024 or w <= 0 or h <= 0:
            continue
        cv = {0: np.zeros((h, w, 4), np.float32)}
        for (im, l, t), add in rendered:
            B.composite(cv, 0, im, l, t, add, (x0, y0))
        a = cv[0]
        al = np.clip(a[..., 3:4], 0, 1)
        rgb = np.where(al > 1e-4, a[..., :3] / np.maximum(al, 1e-4), 0)
        img = Image.fromarray(
            (np.clip(np.concatenate([rgb, al], -1), 0, 1) * 255 + 0.5).astype(np.uint8),
            "RGBA",
        )
        bb = img.getbbox()
        if not bb:
            continue
        img = img.crop(bb)
        x0 += bb[0]
        y0 += bb[1]
        if img.width > 1000 or img.height > 1000:
            img = img.resize((min(img.width, 1000), min(img.height, 1000)))
        # offset relative to world origin in px (y down): store absolute top-left in world px
        ss.add_named(name, img, int(round(x0)), int(round(y0)))
    if ss.frames:
        ss.write(f"{out_dir}/{room}_ents.spr")
    # spawn
    sp = cfg["spawn"]
    if sp is None:
        g = [e for e in ents if e["kind"] == "gate"]
        sp = (g[0]["x"], g[0]["y"]) if g else (W / 2, H / 2)
    with open(f"{out_dir}/{room}.room", "wb") as fp:
        fp.write(b"HKRM")
        fp.write(struct.pack("<HHHHff", 1, W, H, len(ents), sp[0], sp[1]))
        for s in (cfg["music"], cfg["ambience"], cfg["title"], cfg["subtitle"]):
            fp.write(struct.pack("32s", s.encode()[:31]))
        for y in range(H):
            row = bytearray((W + 7) // 8)
            for x in range(W):
                if grid[y][x]:
                    row[x >> 3] |= 1 << (x & 7)
            fp.write(bytes(row))
        for e in ents:
            fp.write(
                struct.pack(
                    "<BBHffffhh32s32s",
                    KIND[e["kind"]],
                    0,
                    0,
                    e["x"],
                    e["y"],
                    e["w"],
                    e["h"],
                    e["hp"],
                    e["geo"],
                    e["name"].encode()[:31],
                    e["target"].encode()[:31],
                )
            )
    json.dump(dict(w=W, h=H, ents=ents), open(f"{out_dir}/{room}.json", "w"), indent=1)
    from collections import Counter

    print(room, W, H, dict(Counter(e["kind"] for e in ents)))


if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for r in sys.argv[2:]:
        build(r, out)
