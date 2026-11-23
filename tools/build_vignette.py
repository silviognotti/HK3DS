#!/usr/bin/env python3
"""The Knight's vignette (vignette_large_v01: a black alpha gradient) as a 256x256 A8 texture, romfs/gfx/vignette.a8.
The Knight's sprite sheet keeps it at 32 bits only on a New 3DS; at 16 bits its 4-bit alpha showed rings across the
screen (it's drawn magnified ~10x). 8-bit alpha alone keeps it smooth for 64 KB.   python build_vignette.py <romfs>
"""

import sys, os
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B
from texpack import MORTON

sp = B.sprite_asset("f6afd349486b57545a4d6b7a09281673")
if sp is None:
    for guid, path in B.metamap("Sprite").items():
        if os.path.splitext(os.path.basename(path))[0] == "vignette_large_v01":
            sp = B.sprite_asset(guid)
            break
r = B.sprite_image(sp) if sp else None
if not r:
    sys.exit("Cannot resolve vignette_large_v01 sprite texture")
im = r[0]  # 256 x 170, RGB 0
a = np.zeros((256, 256), np.uint8)
al = np.asarray(im)[..., 3]
a[: al.shape[0], : al.shape[1]] = al  # top rows first (sampled at v = 1, like texpack)
t = a.reshape(32, 8, 32, 8).transpose(0, 2, 1, 3).reshape(32, 32, 64)
out = np.empty_like(t)
out[:, :, MORTON] = t
open(os.path.join(sys.argv[1], "gfx", "vignette.a8"), "wb").write(out.tobytes())
print("vignette", im.size)
