#!/usr/bin/env python3
"""Actions used by the exported FSMs that the 3DS runtime doesn't implement (fsm_actions.c ACTS list).
python missing_actions.py <objdir> [filter substring of root name]"""

import sys, os, re, json, glob, collections

src = open(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "source", "fsm_actions.c"
    )
).read()
known = set(re.findall(r"X\((\w+)\)", src))
miss = collections.Counter()
where = collections.defaultdict(set)
flt = sys.argv[2] if len(sys.argv) > 2 else None
for p in sorted(glob.glob(f"{sys.argv[1]}/*.obj.json")):
    t = json.load(open(p))
    room = os.path.basename(p)[:-9]
    trees = [t] + [v for v in t["prefabs"].values() if v]
    for tr in trees:
        roots = {}
        for i, o in enumerate(tr["objects"]):
            r = i
            while tr["objects"][r]["p"] != -1:
                r = tr["objects"][r]["p"]
            roots[i] = tr["objects"][r]["n"]
        fown = {}
        for i, o in enumerate(tr["objects"]):
            for c in o["c"]:
                if c["k"] == "fsm":
                    fown[c["i"]] = roots[i]
        for fi, f in enumerate(tr["fsms"]):
            rn = fown.get(fi, "?")
            if flt and flt not in rn:
                continue
            for s in f["states"]:
                for a in s["actions"]:
                    n = a["type"].split(".")[-1]
                    if n not in known:
                        miss[n] += 1
                        where[n].add(f'{room}:{rn}/{f["fsm"]}')
for n, c in miss.most_common():
    w = sorted(where[n])
    print(f'{c:5d} {n:36s} {", ".join(w[:4])}{" ..." if len(w) > 4 else ""}')
