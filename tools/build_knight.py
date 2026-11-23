#!/usr/bin/env python3
"""The Knight's own objects and state machines (Knight_0.prefab: Spell Control, Superdash, Nail Arts, Dream Nail,
Surface Water, Map Control..., the Attacks / Effects / Spells children) as a persistent object tree.
The runtime maps the tree's root onto its HeroController stand-in.
   python build_knight.py <objdir>   -> <objdir>/_Knight.obj.json  (then pack_objs.py <objdir> <romfs> _Knight)
"""

import sys, os, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_objs as BO
from hkcommon import EX


def main(out):
    doc = BO.Doc(f"{EX}/GameObject/Knight_0.prefab")
    root = next(g for g in doc.go if doc.parent(g) is None)
    ex = BO.Exporter()
    ex.pending_clips = set()
    tree = ex.export_tree(doc, [root])
    tree["objects"][0]["x"] = tree["objects"][0]["y"] = 0
    ex.assign_clips(tree)
    tree["prefabs"] = ex.export_prefabs()
    tree["libs"] = {g: sorted(c) for g, c in ex.libs.items()}
    tree["sprites"] = sorted(ex.sprites)
    tree["room"] = "_Knight"
    json.dump(tree, open(f"{out}/_Knight.obj.json", "w"), separators=(",", ":"))
    print(
        "objects",
        len(tree["objects"]),
        "fsms",
        len(tree["fsms"]),
        "prefabs",
        sum(1 for v in tree["prefabs"].values() if v),
        "libs",
        len(tree["libs"]),
        "sprites",
        len(tree["sprites"]),
    )


if __name__ == "__main__":
    main(sys.argv[1])
