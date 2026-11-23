#!/usr/bin/env python3
"""The Knight's vignette (vignette_large_v01: a black alpha gradient) as a 256x256 A8 texture, romfs/gfx/vignette.a8.
The Knight's sprite sheet keeps it at 32 bits only on a New 3DS; at 16 bits its 4-bit alpha showed rings across the
screen (it's drawn magnified ~10x). 8-bit alpha alone keeps it smooth for 64 KB.   python build_vignette.py <romfs>"""
import sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B
from texpack import MORTON
sp = B.sprite_asset(B.export_ids().get('vignette') or 'f6afd349486b57545a4d6b7a09281673')   # make_lookups.py
if sp is None: sys.exit('vignette_large_v01: no such sprite in the export (run make_lookups.py: tools/build.sh --from lookups)')
im = B.sprite_image(sp)[0]                     # 256 x 170, RGB 0
a = np.zeros((256, 256), np.uint8)
al = np.asarray(im)[..., 3]
a[:al.shape[0], :al.shape[1]] = al             # top rows first (sampled at v = 1, like texpack)
t = a.reshape(32, 8, 32, 8).transpose(0, 2, 1, 3).reshape(32, 32, 64)
out = np.empty_like(t); out[:, :, MORTON] = t
open(os.path.join(sys.argv[1], 'gfx', 'vignette.a8'), 'wb').write(out.tobytes())
print('vignette', im.size)
