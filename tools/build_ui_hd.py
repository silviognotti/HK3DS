#!/usr/bin/env python3
"""HD fonts, HUD, dialogue box and title screen for the HD (800x240) build:  python build_ui_hd.py <romfs> <romfs_hd>
Builds <romfs_hd>/gfx/font_menu.spr, font_big.spr, font_text.spr, hud.spr, dialog.spr and ui.spr at twice the texels across
(HK_XD=2), in the standard sheets' formats, and checks each has the standard sheet's animation and image names.
romfs_hd starts as hard links to romfs, so each old file is removed before writing (never write through a link).
"""

import os, sys, zlib, struct

os.environ["HK_XD"] = "2"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_ui as U
import build_sprites as BS
from pack_objs_hd import sheet_names


def wrap(path):
    d = open(path, "rb").read()
    if d[:4] != b"HKZ1":
        open(path, "wb").write(
            b"HKZ1" + struct.pack("<I", len(d)) + zlib.compress(d, 9)
        )


def main(src, dst):
    assert BS.XD == 2
    jobs = [
        (
            "font_menu.spr",
            lambda o: U.build_font(
                o, f"{U.FONTDIR}/TrajanPro-Regular.otf", 14, glow=True
            ),
        ),
        (
            "font_big.spr",
            lambda o: U.build_font(
                o, f"{U.FONTDIR}/TrajanPro-Regular.otf", 26, glow=True, tracking=1
            ),
        ),
        ("font_text.spr", lambda o: U.build_font(o, f"{U.FONTDIR}/Perpetua.ttf", 14)),
        ("hud.spr", BS.build_hud),
        ("dialog.spr", U.build_dialog),
        ("ui.spr", U.build_ui),
    ]
    bad = []
    for name, fn in jobs:
        out = f"{dst}/gfx/{name}"
        if os.path.exists(out):
            os.remove(out)
        fn(out)
        wrap(out)
        a0, n0 = sheet_names(f"{src}/gfx/{name}")
        a1, n1 = sheet_names(out)
        miss = (a0 - a1) | (n0 - n1)
        print(
            name,
            "HD",
            os.path.getsize(out) // 1024,
            "KB",
            "MISSING %s" % sorted(miss)[:5] if miss else "all names",
        )
        if miss:  # keep the standard sheet
            os.remove(out)
            os.link(f"{src}/gfx/{name}", out)
            bad.append(name)
    return bad


if __name__ == "__main__":
    bad = main(sys.argv[1], sys.argv[2])
    if bad:
        print("standard sheets kept for", bad)
        sys.exit(1)
