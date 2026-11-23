#!/usr/bin/env python3
"""Re-render only a room's art layers (.lyr) with the current bake settings, keeping its collision, exits and
entities.   python rebake_layers.py <build dir> Room [Room ...]   (then install the .lyr: lyr_v2.py + zwrap.py)
"""

import sys, os, struct

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B, build_room as BR, masks as MK

out_dir = sys.argv[1]
for room in sys.argv[2:]:
    raw = open(os.path.join(out_dir, room + ".room"), "rb").read()
    if raw[:4] == b"HKZ1":
        import zlib

        raw = zlib.decompress(raw[8:])
    ver, W, H = struct.unpack_from("<HHH", raw, 4)
    scene = BR.scene_path(room)
    B.SKIP_PATHS = [p for g, p, pers in MK.mask_roots(scene)]
    B.MASKS_AT_RUNTIME = True
    ur = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "data", "uoroots", f"{room}.txt"
    )
    if os.path.exists(ur):
        B.SKIP_PATHS += [l.strip() for l in open(ur) if l.strip()]
    B.bake(scene, out_dir, room, S=16, bounds=(0, 0, W, H), preview=False)
    B.SKIP_PATHS = []
    B.MASKS_AT_RUNTIME = False
    print(room, "layers ok", flush=True)
