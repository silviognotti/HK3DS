#!/usr/bin/env python3
"""Area Title Controllers of every scene -> romfs/text/area_titles.txt
One line per controller: room, Area Event, Display Right, Door Trigger, Only On Revisit, Unvisited Pause, Visited Pause,
Wait for Trigger, trigger box (x0 y0 x1 y1 or '-'). The runtime ports AreaTitleController.cs around these values.
   python area_titles.py <out file> [rooms...]"""

import sys, os
import time
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from parallel_helpers import interruptible_main, parallel_executor


def atomic_write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=os.path.dirname(path),
                                     prefix=f".{os.path.basename(path)}.", suffix=".tmp", delete=False) as h:
        h.write(text)
        h.flush()
        temporary = h.name
    os.replace(temporary, path)

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import build_room as BR

X = BR.X


def scan(path, room):
    objs = F.load_objs(path)
    P = F.Paths(objs)
    out = []
    for k, (t, v) in objs.items():
        if (
            t != "MonoBehaviour"
            or not isinstance(v.get("fsm"), dict)
            or v["fsm"].get("name") != "Area Title Controller"
        ):
            continue
        gid = int(v["m_GameObject"]["fileID"])
        go = objs.get(gid, (None, {}))[1]
        if str(go.get("m_IsActive", 1)) == "0":
            continue
        f = F.decode_fsm(v, P, "")
        V = f.get("variables") or {}

        def g(n, d):
            x = V.get(n)
            return d if not x or x.get("value") is None else x["value"]

        out.append(
            dict(
                path=P.path(gid),
                ev=g("Area Event", ""),
                right=bool(g("Display Right", False)),
                door=g("Door Trigger", "") or "",
                rev=bool(g("Only On Revisit", False)),
                up=float(g("Unvisited Pause", 2)),
                vp=float(g("Visited Pause", 2)),
                trig=bool(g("Wait for Trigger", False)),
            )
        )
    if out and any(o["trig"] for o in out):
        sc = X.Scene(X.load_scene(path))
        cols = X.colliders(sc)
        for o in out:
            c = next((c for c in cols if c["path"] == o["path"] and c["trigger"]), None)
            o["box"] = c["box"] if c else None
    return out


def main(outp, rooms=None):
    if not rooms:
        paths = [
            (n, BR.scene_path(n))
            for n in open(
                os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "all_rooms.txt")
            ).read().split()
        ]
    else:
        paths = [(n, BR.scene_path(n)) for n in rooms]
    paths = [(n, p) for n, p in paths if p]
    if os.path.isfile(outp):
        output_mtime = os.path.getmtime(outp)
        stale = any(os.path.getmtime(p) > output_mtime for _, p in paths)
        if not stale:
            print(f"SKIPPED: area_titles: {len(paths)} rooms up to date")
            return
    candidates = []
    for room, path in paths:
        try:
            text = open(path, "rb").read(2_000_000).decode("utf-8", "replace")
        except OSError:
            continue
        if "Area Title Controller" in text:
            candidates.append((room, path))
    paths = candidates
    def process(path_item):
        room, p = path_item
        try:
            res = scan(p, room)
        except Exception as e:
            print("fail", room, e)
            return room, []
        room_lines = []
        for o in res:
            b = o.get("box")
            bs = "%.2f %.2f %.2f %.2f" % tuple(b) if (o["trig"] and b) else "-"
            room_lines.append(
                "\t".join(
                    [
                        room,
                        o["ev"] or "-",
                        str(int(o["right"])),
                        o["door"] or "-",
                        str(int(o["rev"])),
                        "%.2f" % o["up"],
                        "%.2f" % o["vp"],
                        str(int(o["trig"])),
                        bs,
                    ]
                )
            )
        return room, room_lines

    lines_by_room = {}
    status_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "work", "logs", "gamedata", "area_titles")
    os.makedirs(status_dir, exist_ok=True)
    start = time.monotonic()
    completed = 0
    workers = min(16, (os.cpu_count() or 1) * 2)
    for room, _ in paths:
        atomic_write(os.path.join(status_dir, f"{room}.status"), "PENDING\n")
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for room, room_lines in pool.map(process, sorted(paths)):
            lines_by_room[room] = room_lines
            completed += 1
            atomic_write(os.path.join(status_dir, f"{room}.status"), "OK\n")
            atomic_write(os.path.join(status_dir, f"{room}.log"), "\n".join(room_lines))
            print(f"[area_titles {completed}/{len(paths)}] {room}: OK ({time.monotonic()-start:.1f}s)", flush=True)
    lines = []
    for room, _ in sorted(paths):
        lines.extend(lines_by_room.get(room, []))
    open(outp, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main, sys.argv[1], sys.argv[2:] or None))
