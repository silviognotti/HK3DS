#!/usr/bin/env python3
"""Walls the Knight can't cling to: terrain colliders on objects with a NonSlider component.
python nonslider.py <romfs> <scene list>  -> romfs/text/nonslider.txt ("<scene> x0 y0 x1 y1" per collider path segment box)
"""

import sys, os
import time
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from parallel_helpers import interruptible_main, parallel_executor


def atomic_write(path, text):
    from pathlib import Path
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=str(path.parent),
                                     prefix=f".{path.name}.", suffix=".tmp", delete=False) as h:
        h.write(text)
        h.flush()
        temporary = h.name
    os.replace(temporary, path)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_room as BR
import build_objs as BO


def main(romfs, lst):
    rooms = open(lst).read().split()
    output = os.path.join(romfs, "text", "nonslider.txt")
    os.makedirs(os.path.dirname(output), exist_ok=True)
    if os.path.isfile(output):
        output_mtime = os.path.getmtime(output)
        stale = False
        for room in rooms:
            scene = BR.scene_path(room, quiet=True)
            if scene and os.path.getmtime(scene) > output_mtime:
                stale = True
                break
        if not stale:
            status_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "logs", "gamedata", "nonslider")
            os.makedirs(status_dir, exist_ok=True)
            for room in rooms:
                atomic_write(os.path.join(status_dir, f"{room}.status"), "SKIPPED\n")
                atomic_write(os.path.join(status_dir, f"{room}.log"), "output is up to date\n")
            print(f"SKIPPED: nonslider: {len(rooms)} rooms up to date")
            return

    def scan(r):
        p = BR.scene_path(r, quiet=True)
        if not p:
            return []
        try:
            scene_text = open(p, "rb").read(2_000_000).decode("utf-8", "replace")
        except OSError:
            return []
        if "NonSlider" not in scene_text:
            return []
        try:
            sc = BR.X.Scene(BR.X.load_scene(p))
        except Exception as e:
            return []
        ns = set()
        for k, (t, v) in sc.objs.items():
            if t == "MonoBehaviour" and BO.classify_mb(v) == "NonSlider":
                ns.add(v["m_GameObject"]["fileID"])
        if not ns:
            return []
        try:
            cols = BR.X.colliders(sc)
        except Exception as e:
            return []
        result = []
        for c in cols:
            if c["gid"] not in ns or c["trigger"] or not c["active"]:
                continue
            for pth in c["paths"] or []:
                n = len(pth)
                for i in range(n if c["type"] != "EdgeCollider2D" else n - 1):
                    (x0, y0), (x1, y1) = pth[i], pth[(i + 1) % n]
                    if abs(x1 - x0) < 0.3:  # wall segments
                        result.append(
                            f"{r} {min(x0,x1)-0.05:.2f} {min(y0,y1):.2f} {max(x0,x1)+0.05:.2f} {max(y0,y1):.2f}"
                        )
        return result

    status_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "logs", "gamedata", "nonslider")
    os.makedirs(status_dir, exist_ok=True)
    out_by_room = {}
    start = time.monotonic()
    completed = 0
    workers = min(8, os.cpu_count() or 1)
    for r in rooms:
        atomic_write(os.path.join(status_dir, f"{r}.status"), "PENDING\n")
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        futures = {pool.submit(scan, r): r for r in rooms}
        for future in as_completed(futures):
            room = futures[future]
            out_by_room[room] = future.result()
            completed += 1
            elapsed = time.monotonic() - start
            result = out_by_room[room]
            atomic_write(os.path.join(status_dir, f"{room}.status"), "OK\n")
            atomic_write(os.path.join(status_dir, f"{room}.log"), "\n".join(result))
            print(f"[nonslider {completed}/{len(rooms)}] {room}: OK ({elapsed:.1f}s)", flush=True)
    out = []
    for room in rooms:
        out.extend(out_by_room.get(room, []))
    open(output, "w").write("\n".join(out) + "\n")
    print(len(out), "non-slider wall segments")


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main, sys.argv[1], sys.argv[2]))
