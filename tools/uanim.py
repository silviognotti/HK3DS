#!/usr/bin/env python3
"""Unity Animator clips (AnimationClip assets) for objects driven by a Mecanim Animator rather than tk2d: effects such as
the orange splat of a boss's death burst (it flips through its sprites, tints orange and fades out in 0.35 s).

The exported data has no .meta files for controllers and clips, so an Animator's controller can't be followed by guid.
A clip is found instead from what it animates: the SpriteRenderer's sprite appears in the clip's sprite curve, and the
clip's name matches the object's name ("Splat Explode Orange" -> splat_explode_orange) when several clips share sprites.

   find_clip(object name, sprite guid) -> {'len', 'loop', 'cv': [{'p': path, 'a': attr, 'k': [[t, v], ...]}],
                                            'spr': [[t, sprite guid], ...] (when the clip flips sprites)} or None
"""

import os, re, glob, pickle, functools
from concurrent.futures import ThreadPoolExecutor
from parallel_helpers import interruptible_main, parallel_executor
from hkcommon import EX, ROOT

ATTRS = {
    ("m_Color.r", "212"): "r",
    ("m_Color.g", "212"): "g",
    ("m_Color.b", "212"): "b",
    ("m_Color.a", "212"): "a",
    ("m_IsActive", "1"): "act",
    ("m_Enabled", "212"): "en",
}


def parse_clip(path):
    txt = open(path, encoding="utf-8", errors="replace").read()
    name = re.search(r"m_Name: (.*)", txt).group(1).strip()
    m = re.search(r"m_StopTime: ([-0-9.e]+)", txt)
    length = float(m.group(1)) if m else 0
    m = re.search(r"m_LoopTime: (\d)", txt)
    loop = bool(int(m.group(1))) if m else False
    cv, spr = [], []
    # float curves: each "- serializedVersion: 2\n    curve:" block up to its attribute/path/classID
    fc = (
        txt.split("m_FloatCurves:", 1)[1].split("m_PPtrCurves:", 1)[0]
        if "m_FloatCurves:" in txt
        else ""
    )
    for blk in re.split(r"\n  - serializedVersion: 2\n", fc)[1:]:
        keys = [
            [float(t), float(v)]
            for t, v in re.findall(r"time: ([-0-9.e]+)\n\s+value: ([-0-9.e]+)", blk)
        ]
        a = re.search(r"\n    attribute: (\S+)", blk)
        p = re.search(r"\n    path: ?(.*)", blk)
        c = re.search(r"\n    classID: (\d+)", blk)
        if not (a and c):
            continue
        code = ATTRS.get((a.group(1), c.group(1)))
        if code is None:
            if c.group(1) == "4" and a.group(1).startswith("m_LocalScale."):
                code = "s" + a.group(1)[-1]
            elif c.group(1) == "4" and a.group(1).startswith("m_LocalPosition."):
                code = "p" + a.group(1)[-1]
        if code and keys:
            cv.append(
                {
                    "p": (p.group(1).strip() if p else ""),
                    "a": code,
                    "k": [[round(t, 4), round(v, 4)] for t, v in keys],
                }
            )
    # m_ScaleCurves / m_PositionCurves (vector curves on transforms)
    for sect, pre in (("m_ScaleCurves:", "s"), ("m_PositionCurves:", "p")):
        if sect not in txt:
            continue
        body = txt.split(sect, 1)[1]
        body = body.split("\n  m_", 1)[0]
        for blk in re.split(r"\n  - (?:serializedVersion: 2\n    )?curve:\n", body)[1:]:
            ks = re.findall(
                r"time: ([-0-9.e]+)\n\s+value: \{x: ([-0-9.e]+), y: ([-0-9.e]+), z: ([-0-9.e]+)\}",
                blk,
            )
            p = re.search(r"\n    path: ?(.*)", blk)
            if not ks:
                continue
            for i, ax in ((1, "x"), (2, "y")):
                cv.append(
                    {
                        "p": (p.group(1).strip() if p else ""),
                        "a": pre + ax,
                        "k": [
                            [round(float(k[0]), 4), round(float(k[i]), 4)] for k in ks
                        ],
                    }
                )
    # sprite flipbook: the m_PPtrCurves block whose attribute is m_Sprite
    if "m_PPtrCurves:" in txt:
        pp = txt.split("m_PPtrCurves:", 1)[1].split("m_SampleRate", 1)[0]
        for blk in re.split(r"\n  - (?:serializedVersion: 2\n    )?curve:\n", pp)[1:]:
            if "attribute: m_Sprite" not in blk:
                continue
            p = re.search(r"\n    path: ?(.*)", blk)
            if p and p.group(1).strip():
                continue
            spr = [
                [round(float(t), 4), g]
                for t, g in re.findall(
                    r"time: ([-0-9.e]+)\n\s+value: \{fileID: \d+, guid: ([0-9a-f]+)",
                    blk,
                )
            ]
    return {"name": name, "len": round(length, 4), "loop": loop, "cv": cv, "spr": spr}


@functools.lru_cache(None)
def index():
    cache = f"{ROOT}/uanim_index.pkl"
    if os.path.exists(cache):
        return pickle.load(open(cache, "rb"))
    clips = []
    paths = sorted(glob.glob(f"{EX}/AnimationClip/*.anim"))

    def parse(path):
        try:
            return parse_clip(path)
        except Exception:
            return None

    workers = min(16, (os.cpu_count() or 1) * 2)
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for clip in pool.map(parse, paths):
            if clip is not None:
                clips.append(clip)
    with open(cache, "wb") as handle:
        pickle.dump(clips, handle)
    return clips


def norm(s):
    return re.sub(r"[^a-z0-9]", "", str(s).lower())


def find_clip(objname, sprite_guid):
    """The clip that animates this object, or None (unknown: the renderer keeps its initial look)."""
    cands = [
        c for c in index() if sprite_guid and any(g == sprite_guid for t, g in c["spr"])
    ]
    on = norm(objname)
    if cands:
        best = [c for c in cands if norm(c["name"]) == on] or [
            c for c in cands if norm(c["name"]) in on or on in norm(c["name"])
        ]
        if best:
            return best[0]
        return cands[0] if len(cands) == 1 else None
    exact = [c for c in index() if norm(c["name"]) == on]
    return exact[0] if len(exact) == 1 else None


def to_comp(clip):
    c = {"k": "uan", "len": clip["len"], "loop": int(clip["loop"]), "cv": clip["cv"]}
    if clip["spr"]:
        c["spr"] = clip["spr"]
    return c


if __name__ == "__main__":
    import sys

    c = find_clip(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    print(
        c
        and {
            k: (v if k != "cv" else [(x["p"], x["a"], len(x["k"])) for x in v])
            for k, v in c.items()
        }
    )
