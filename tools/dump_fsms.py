#!/usr/bin/env python3
"""Print the PlayMaker FSMs of objects in a scene (or prefab) whose path starts with a prefix.
python dump_fsms.py <Room or .prefab path> <path prefix> [fsm name] [-v]   (-v: every action's params)
"""

import sys, os, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import build_room as BR


def main():
    args = [a for a in sys.argv[1:] if a != "-v"]
    verbose = "-v" in sys.argv
    src, prefix = args[0], args[1]
    only = args[2] if len(args) > 2 else None
    p = src if src.endswith(".prefab") else BR.scene_path(src)
    objs = F.load_objs(p)
    P = F.Paths(objs)
    for k, (t, v) in objs.items():
        if t != "MonoBehaviour" or not isinstance(v.get("fsm"), dict):
            continue
        path = P.path(int(v["m_GameObject"]["fileID"]))
        if not path.startswith(prefix):
            continue
        f = F.decode_fsm(v, P, "")
        if only and f["fsm"] != only:
            continue
        print(
            f'=== {path} :: {f["fsm"]} ({len(f["states"])} states, start {f.get("start")}) globals {f.get("global")}'
        )
        if verbose:
            print("  vars", json.dumps(f.get("variables"))[:600])
        for s in f["states"]:
            print(
                "  STATE", s["name"], [(t["event"], t["to"]) for t in s["transitions"]]
            )
            for a in s["actions"]:
                n = a["type"].split(".")[-1]
                print("     ", n, json.dumps(a["params"])[: 400 if verbose else 140])


if __name__ == "__main__":
    main()
