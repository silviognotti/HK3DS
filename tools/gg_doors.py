#!/usr/bin/env python3
"""Godhome pantheon doors (BossSequenceDoor) -> romfs/text/gg_doors.txt: "<scene> <door x> <tier>"   (python gg_doors.py <romfs>)"""

import sys, os, re

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hk_extract_room as H, build_room as BR

out = []
for room in ("GG_Atrium", "GG_Atrium_Roof"):
    p = BR.scene_path(room)
    txt = open(p, errors="ignore").read()
    sc = H.Scene(H.load_scene(p))
    for m in re.finditer(
        r"m_GameObject: \{fileID: (\d+)\}\n(?:  .*\n){0,8}?  playerDataString: bossDoorStateTier(\d)",
        txt,
    ):
        go, tier = int(m.group(1)), m.group(2)
        w = sc.world(sc.go2tr[go])
        out.append(f'{room} {w["x"]:.2f} {tier}')
open(f"{sys.argv[1]}/text/gg_doors.txt", "w").write("\n".join(out) + "\n")
print("\n".join(out))
