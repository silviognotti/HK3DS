#!/usr/bin/env python3
"""Godhome pantheons: Boss Sequence Tier N assets -> romfs/text/pantheons.txt
one line per pantheon: <tier> <achievementKey> <scene> <scene> ...   (python pantheons.py <romfs>)
"""

import sys, os, re, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkcommon import EX, metamap

mm = dict(metamap("MonoBehaviour"))


def guids(txt, key):
    m = re.search(r"^  %s:\n((?:  - .*\n)+)" % key, txt, re.M)
    return re.findall(r"guid: ([0-9a-f]+)", m.group(1)) if m else []


out = []
for t in range(1, 6):
    a = open(f"{EX}/Resources/gg/Boss Sequence Tier {t}.asset").read()
    ach = re.search(r"achievementKey: (\S*)", a)
    ach = ach.group(1) if ach else ""
    scenes = []
    for g in guids(a, "bossScenes"):
        p = mm.get(g)
        if not p:
            print("missing", g)
            continue
        s = re.search(r"sceneName: (\S+)", open(p).read())
        scenes.append(s.group(1) if s else "?")
    end = re.search(r"customEndScene: (\S+)", a)
    out.append(f'{t} {ach or "-"} {end.group(1) if end else "-"} ' + " ".join(scenes))
    print(out[-1])
open(f"{sys.argv[1]}/text/pantheons.txt", "w").write("\n".join(out) + "\n")
