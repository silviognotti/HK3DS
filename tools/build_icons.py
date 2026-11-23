#!/usr/bin/env python3
"""HUD/inventory icons: the charm icons of CharmIconList (charm_<id>, plus the unbreakable / Grimmchild / Nymm
variants) and other named Sprite assets used by item messages, the inventory and shops -> romfs/gfx/icons.spr.
Icons are scaled so a charm is ICON px tall; each keeps its centre as the pivot.
   python build_icons.py <romfs/gfx>"""

import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import bake_scene as B
from build_sprites import SpriteSet
from texpack import premul_resize, FMT_RGBA4

SCALE = 0.24  # HUD sprite px -> 3DS px (charms ~36 px)
EXTRA = [
    "charm_backboard",
    "Inv_Empty",
    "charm_cost_full",
    "charm_cost_empty",
    "Charm_Notch",
    "charm_blocker",
    "Inv_0013_charm1",
    "Inv_0012_charm2",
    "Inv_0011_charm3",
    "Inv_0010_charm4",
]


def add(ss, name, guid, scale=SCALE):
    r = B.sprite_image(B.sprite_asset(guid))
    if not r:
        print("  missing", name)
        return
    im = r[0]
    w, h = max(1, round(im.width * scale)), max(1, round(im.height * scale))
    im = premul_resize(im, w, h)
    ss.add_named(name, im, -w // 2, -h // 2)


def main(out):
    ss = SpriteSet(1.0)
    objs = F.load_objs(f"{B.EX}/GameObject/_GameCameras.prefab")
    for k, (t, v) in objs.items():
        if t == "MonoBehaviour" and "spriteList" in v:
            for i, s in enumerate(v["spriteList"]):
                if s.get("guid"):
                    add(ss, f"charm_{i}", s["guid"])
            for key in (
                "unbreakableHeart",
                "unbreakableGreed",
                "unbreakableStrength",
                "grimmchildLevel1",
                "grimmchildLevel2",
                "grimmchildLevel3",
                "grimmchildLevel4",
                "nymmCharm",
            ):
                g = (v.get(key) or {}).get("guid")
                if g:
                    add(ss, f"charm_{key}", g)
            break
    sp = B.metamap("Sprite")
    byname = {os.path.basename(p)[:-6]: g for g, p in sp.items()}
    for n in EXTRA:
        if n in byname:
            add(ss, n, byname[n])
    ss.write(f"{out}/icons.spr", FMT_RGBA4, 1024)


if __name__ == "__main__":
    main(sys.argv[1])
