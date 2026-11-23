#!/usr/bin/env python3
"""The Stagway map (Stag Map.prefab): the map pieces of each station, the backing, the list glow and divider,
rendered flat (HUD camera, orthographic) into romfs/gfx/stagmenu.spr. Every frame is named after its object and
keeps its offset from the map's origin, so drawing them all at one screen point rebuilds the map.
   python build_stagmenu.py <romfs/gfx>"""

import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B
from build_sprites import SpriteSet
from texpack import FMT_RGBA4

S = 15.0  # px per HUD unit on the 3DS top screen
ORIGIN = (-21.0, 5.6)  # map centre (prefab space)


def main(out):
    p = f"{B.EX}/GameObject/Stag Map.prefab"
    sc, items = B.collect(p)
    ss = SpriteSet(1.0)
    seen = set()
    for it in items:
        name = it["path"].split("/")[-1]
        if name == "Glow":
            name = "glow"
        if name in seen:
            continue
        seen.add(name)
        r = B.render_item(it, S, 1.0)
        if not r:
            continue
        img, left, top = r
        bb = img.getbbox()
        if not bb:
            continue
        img = img.crop(bb)
        left += bb[0]
        top += bb[1]
        ox = left - ORIGIN[0] * S
        oy = top + ORIGIN[1] * S
        ss.add_named(name, img, int(round(ox)), int(round(oy)))
        print(name, img.size, int(ox), int(oy))
    ss.write(f"{out}/stagmenu.spr", FMT_RGBA4)


if __name__ == "__main__":
    main(sys.argv[1])
