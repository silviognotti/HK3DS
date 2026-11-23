#!/usr/bin/env python3
"""SceneAdditiveLoadConditional components (boss scenes and other additive loads) -> romfs/text/additive.txt
One line per load: base<TAB>json (scene, alt, bool, boolValue, int, intValue, isIntValue, extraBool [[name, value]],
extraInt [[name, value, otherName, testType]], pbi [id, sceneName] or null, door).
   python additive.py <out file>"""

import sys, os, json, subprocess

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
from hkcommon import EX

files = [
    p
    for p in subprocess.run(
        ["grep", "-rl", "sceneNameToLoad:", "--include=*.unity", EX],
        capture_output=True,
        text=True,
    ).stdout.split("\n")
    if p
]
rows = []


def s(x):
    return "" if x is None else str(x)


for p in sorted(files):
    base = os.path.basename(p)[:-6]
    for k, (t, v) in F.load_objs(p).items():
        if (
            t != "MonoBehaviour"
            or "sceneNameToLoad" not in v
            or not v.get("sceneNameToLoad")
        ):
            continue
        if str(v.get("m_Enabled", 1)) == "0":
            continue
        pb = v.get("persistentBoolData") or {}
        d = dict(
            scene=s(v["sceneNameToLoad"]),
            alt=s(v.get("altSceneNameToLoad")),
            bool=s(v.get("needsPlayerDataBool")),
            boolValue=int(v.get("playerDataBoolValue") or 0),
            int=s(v.get("needsPlayerDataInt")),
            intValue=int(v.get("playerDataIntValue") or 0),
            isIntValue=int(v.get("isIntValue") or 0),
            extraBool=[
                [s(b.get("playerDataBool")), int(b.get("value") or 0)]
                for b in (v.get("extraBoolTests") or [])
            ],
            extraInt=[
                [
                    s(b.get("playerDataInt")),
                    int(b.get("value") or 0),
                    s(b.get("otherPlayerDataInt")),
                    int(b.get("testType") or 0),
                ]
                for b in (v.get("extraIntTests") or [])
            ],
            pbi=(
                [s(pb.get("id")), s(pb.get("sceneName"))]
                if int(v.get("usePersistentBoolItem") or 0)
                else None
            ),
            door=s(v.get("doorTrigger")),
        )
        rows.append(base + "\t" + json.dumps(d, separators=(",", ":")))
open(sys.argv[1], "w").write("\n".join(rows) + "\n")
print(len(rows), "additive loads")
print("\n".join(r[:160] for r in rows))
