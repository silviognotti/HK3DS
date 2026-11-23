#!/usr/bin/env python3
"""SceneManager.darknessLevel of every scene -> romfs/text/darkness.txt ("<scene> <level>", nonzero only).
python darkness.py <romfs> <scene list>"""

import sys, os, re, time, tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from parallel_helpers import interruptible_main, parallel_executor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_room as BR

def atomic_write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=os.path.dirname(path),
                                     prefix=f".{os.path.basename(path)}.", suffix=".tmp", delete=False) as h:
        h.write(text); h.flush(); temporary=h.name
    os.replace(temporary, path)


def scan(r):
    p = BR.scene_path(r)
    if not p:
        return r, None
    m = re.search(r"^  darknessLevel: (-?\d+)", open(p, errors="ignore").read(), re.M)
    if m and int(m.group(1)) != 0:
        return r, f"{r} {m.group(1)}"
    return r, None


def main(romfs, lst):
    rooms = open(lst).read().split()
    output = f"{romfs}/text/darkness.txt"
    os.makedirs(os.path.dirname(output), exist_ok=True)
    if os.path.isfile(output):
        output_mtime = os.path.getmtime(output)
        stale = any(
            (p := BR.scene_path(r, quiet=True))
            and os.path.getmtime(p) > output_mtime
            for r in rooms
        )
        if not stale:
            print(f"SKIPPED: darkness: {len(rooms)} rooms up to date")
            return
    status_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "logs", "gamedata", "darkness")
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
            r = futures[future]
            _, value = future.result()
            out_by_room[r] = value
            completed += 1
            atomic_write(os.path.join(status_dir, f"{r}.status"), "OK\n")
            print(f"[darkness {completed}/{len(rooms)}] {r}: OK ({time.monotonic()-start:.1f}s)", flush=True)
    out = [out_by_room[r] for r in rooms if out_by_room.get(r)]
    os.makedirs(f"{romfs}/text", exist_ok=True)
    open(output, "w").write("\n".join(out) + "\n")
    print(len(out), "dark scenes")


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main, sys.argv[1], sys.argv[2]))
