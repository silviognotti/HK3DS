# Resolve texture GUIDs with no .meta (hksprites export) -> PNG path.
import argparse
import collections
import glob
import os
import pickle
import re
import struct
import sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np

from parallel_helpers import interruptible_main, parallel_executor


ROOT = os.environ.get("HKDATA", "hkdata")
spr, tex = pickle.load(open(f"{ROOT}/metamaps.pkl", "rb"))
by = pickle.load(open(f"{ROOT}/tex2sprites.pkl", "rb"))
AC = pickle.load(open(f"{ROOT}/hs_alpha4.pkl", "rb"))
hs = sorted(glob.glob(f"{ROOT}/hs/Texture2D/*.png"))
byname = collections.defaultdict(list)
for path in hs:
    byname[os.path.splitext(os.path.basename(path))[0]].append(path)


def pngsize(path):
    with open(path, "rb") as handle:
        header = handle.read(24)
    return struct.unpack(">II", header[16:24])


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("scenes", nargs="*", help="scene or prefab files to target")
    parser.add_argument("--all", action="store_true", help="resolve every missing texture")
    parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
    args = parser.parse_args(argv)

    prev = (
        pickle.load(open(f"{ROOT}/hs_texmatch.pkl", "rb"))
        if os.path.exists(f"{ROOT}/hs_texmatch.pkl")
        else {}
    )
    missing = {key: value for key, value in by.items() if key not in tex and key not in prev}
    need = set()
    for scene in ([] if args.all else args.scenes):
        with open(scene, "r", encoding="utf-8", errors="replace") as handle:
            refs = set(re.findall(
                r"(?:m_Sprite|sprite): \{fileID: \d+, guid: ([0-9a-f]+)",
                handle.read(),
            ))
        for texture_guid, entries in missing.items():
            if any(entry[2] in refs for entry in entries):
                need.add(texture_guid)
    if args.all:
        need = set(missing)

    result = {}
    pinned = {
        "3a648cd54cdac654aa48bedbcc89b9b4":
            "sactx-0-1024x1024-DXT5_BC3-Menu-bf604a84"
    }
    for texture_guid, name in pinned.items():
        if texture_guid in need and name in byname:
            result[texture_guid] = byname[name][0]

    def resolve_texture(texture_guid):
        entries = missing[texture_guid]
        rects = [rect for _, rect, _ in entries if rect]
        names = [name for name, _, _ in entries]
        if not rects:
            return texture_guid, None, (len(entries), names[:3], None, None)
        if len(entries) == 1 and names[0] in byname:
            path = byname[names[0]][0]
            return texture_guid, path, (1, names[:3], 1.0, path)

        max_x = max(rect[0] + rect[2] for rect in rects)
        max_y = max(rect[1] + rect[3] for rect in rects)
        best = None
        for path, (width, height, alpha) in AC.items():
            if width < max_x - 1 or height < max_y - 1:
                continue
            if len(entries) > 1 and not (width / 2 < max_x + 2 and height / 2 < max_y + 2):
                continue
            if len(entries) == 1 and (width > max_x * 1.3 + 8 or height > max_y * 1.3 + 8):
                continue
            inside_values = []
            cover = np.zeros_like(alpha)
            for x, y, rect_width, rect_height in rects:
                x0 = int(x / 4)
                y0 = int((height - (y + rect_height)) / 4)
                x1 = int((x + rect_width) / 4) + 1
                y1 = int((height - y) / 4) + 1
                sub = alpha[max(0, y0):y1, max(0, x0):x1]
                if sub.size:
                    inside_values.append(sub.mean())
                cover[max(0, y0):y1, max(0, x0):x1] = True
            if not inside_values:
                continue
            inside = np.mean(inside_values)
            outside = alpha[~cover].mean() if (~cover).any() else 0
            ring = []
            for x, y, rect_width, rect_height in rects[:300]:
                x0 = int(x / 4) - 1
                y0 = int((height - (y + rect_height)) / 4) - 1
                x1 = int((x + rect_width) / 4) + 1
                y1 = int((height - y) / 4) + 1
                slices = (
                    alpha[max(0, y0), max(0, x0):x1] if y0 >= 0 else None,
                    alpha[min(alpha.shape[0] - 1, y1), max(0, x0):x1] if y1 < alpha.shape[0] else None,
                    alpha[max(0, y0):y1, max(0, x0)] if x0 >= 0 else None,
                    alpha[max(0, y0):y1, min(alpha.shape[1] - 1, x1)] if x1 < alpha.shape[1] else None,
                )
                for ring_slice in slices:
                    if ring_slice is not None and ring_slice.size:
                        ring.append(ring_slice.mean())
            ring_value = np.mean(ring) if ring else 0
            score = inside - outside - 0.5 * ring_value
            if best is None or score > best[0]:
                best = (score, path)
        resolved = best[1] if best and best[0] > 0.3 else None
        score = best[0] if best else None
        return texture_guid, resolved, (len(entries), names[:3], score, resolved)

    with parallel_executor(ThreadPoolExecutor, max_workers=max(1, args.workers)) as pool:
        for texture_guid, resolved, details in pool.map(resolve_texture, sorted(need - set(result))):
            if resolved:
                result[texture_guid] = resolved
            count, names, score, path = details
            print(
                texture_guid,
                count,
                names,
                "->",
                os.path.basename(path) if path else None,
                round(score, 3) if score is not None else None,
                flush=True,
            )
    prev.update(result)
    with open(f"{ROOT}/hs_texmatch.pkl", "wb") as handle:
        pickle.dump(prev, handle)
    print("resolved", len(result), "of", len(need))
    return 0


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main, sys.argv[1:]))
