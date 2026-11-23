#!/usr/bin/env python3
"""Census of PlayMaker FSM names per room (non-enemy objects), to plan which interactables to bring in.
python survey_fsms.py Room [Room ...]  -> prints 'room<TAB>object path<TAB>fsm name'"""

import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import build_room as BR

for room in sys.argv[1:]:
    p = BR.scene_path(room)
    if not p:
        continue
    objs = F.load_objs(p)
    P = F.Paths(objs)
    for k, (t, v) in objs.items():
        if t != "MonoBehaviour" or not isinstance(v.get("fsm"), dict):
            continue
        gid = int(v["m_GameObject"]["fileID"])
        print("%s\t%s\t%s" % (room, P.path(gid), v["fsm"].get("name")), flush=True)
