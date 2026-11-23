#!/usr/bin/env python3
"""Threaded packing of exported Hollow Knight room objects."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
import traceback
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from parallel_helpers import interruptible_main, parallel_executor


def atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=f".{path.name}.",
        suffix=".tmp",
        delete=False,
    ) as handle:
        handle.write(text)
        handle.flush()
        temporary = Path(handle.name)
    temporary.replace(path)


def run(command, cwd=None):
    return subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        check=False,
    )


def already_packed(objdir: Path, romfs: Path, room: str) -> bool:
    obj_json = objdir / f"{room}.obj.json"
    room_obj = romfs / "rooms" / f"{room}.obj"
    room_sprites = romfs / "rooms" / f"{room}_objs.spr"
    if not obj_json.is_file() or not room_obj.is_file():
        return False
    if room_obj.stat().st_mtime_ns < obj_json.stat().st_mtime_ns:
        return False
    if room_sprites.exists():
        if room_sprites.stat().st_mtime_ns < obj_json.stat().st_mtime_ns:
            return False
    return True


def pack_one(room, objdir, romfs, tools, status_dir, index):
    status_dir = Path(status_dir)
    output = []
    atomic_write(status_dir / f"{room}.status", "RUNNING\n")
    atomic_write(status_dir / f"{room}.log", "running\n")

    def phase(message):
        output.append(message)
        atomic_write(status_dir / f"{room}.log", "\n".join(output) + "\n")

    phase("pack objects")
    result = run(
        [sys.executable, str(tools / "pack_objs.py"), str(objdir), str(romfs), room],
        cwd=tools,
    )
    if result.stdout.strip():
        output.append(result.stdout.strip())
    if result.returncode:
        return index, room, False, "\n".join(output)

    sprite_sheet = Path(romfs) / "rooms" / f"{room}_objs.spr"
    if sprite_sheet.is_file():
        phase("wrap object sprite sheet")
        result = run(
            [sys.executable, str(tools / "zwrap.py"), str(sprite_sheet)],
            cwd=tools,
        )
        if result.stdout.strip():
            output.append(result.stdout.strip())
        if result.returncode:
            return index, room, False, "\n".join(output)

    phase("add object particles")
    result = run(
        [sys.executable, str(tools / "obj_particles.py"), str(romfs), room],
        cwd=tools,
    )
    if result.stdout.strip():
        output.append(result.stdout.strip())
    if result.returncode:
        return index, room, False, "\n".join(output)

    room_obj = Path(romfs) / "rooms" / f"{room}.obj"
    if room_obj.is_file():
        phase("build per-frame colliders")
        result = run(
            [sys.executable, str(tools / "tk_colliders.py"), str(room_obj)],
            cwd=tools,
        )
        if result.stdout.strip():
            output.append(result.stdout.strip())
        if result.returncode:
            return index, room, False, "\n".join(output)

    return index, room, True, "\n".join(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdir", required=True)
    parser.add_argument("--romfs", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("rooms", nargs="+")
    args = parser.parse_args()

    tools = Path(__file__).resolve().parent
    objdir = Path(args.objdir).resolve()
    romfs = Path(args.romfs).resolve()
    jobs = max(1, min(args.jobs, len(args.rooms)))

    status_dir = tools.parent / "work" / "logs" / "pack"
    status_dir.mkdir(parents=True, exist_ok=True)
    pending = [
        room
        for room in args.rooms
        if not already_packed(objdir, romfs, room)
    ]
    skipped_count = len(args.rooms) - len(pending)

    for room in args.rooms:
        if room not in pending:
            atomic_write(status_dir / f"{room}.status", "SKIPPED\n")
            atomic_write(status_dir / f"{room}.log", "already packed\n")
            print(f"[pack] {room}: SKIPPED", flush=True)

    if not pending:
        print(f"Skipped {skipped_count} already-packed rooms")
        return 0

    futures = {}
    queue = list(enumerate(args.rooms))
    queue = [(index, room) for index, room in queue if room in pending]
    completed = {}
    next_queue = 0
    completed_count = 0
    future_started = {}

    def submit_next(pool):
        nonlocal next_queue
        if next_queue >= len(queue):
            return False
        index, room = queue[next_queue]
        next_queue += 1
        future = pool.submit(
            pack_one,
            room,
            objdir,
            romfs,
            tools,
            status_dir,
            index,
        )
        futures[future] = (index, room)
        future_started[room] = time.monotonic()
        return True

    with parallel_executor(
        ThreadPoolExecutor,
        max_workers=jobs,
        thread_name_prefix="pack-room",
    ) as pool:
        for room in pending:
            atomic_write(status_dir / f"{room}.status", "PENDING\n")
            atomic_write(status_dir / f"{room}.log", "pending\n")
        while len(futures) < jobs:
            if not submit_next(pool):
                break
        while futures:
            done, _ = wait(futures, return_when=FIRST_COMPLETED)
            for future in done:
                index, room = futures.pop(future)
                try:
                    index, room, ok, output = future.result()
                except BaseException as error:
                    output = "".join(
                        traceback.format_exception(type(error), error, error.__traceback__)
                    )
                    ok = False
                completed_count += 1
                status = "OK" if ok else "FAILED"
                atomic_write(status_dir / f"{room}.status", status + "\n")
                atomic_write(status_dir / f"{room}.log", output + ("\n" if output else ""))
                elapsed = time.monotonic() - future_started.pop(room)
                print(
                    f"[pack {completed_count}/{len(pending)}] "
                    f"{room}: {status} ({elapsed:.1f}s)",
                    flush=True,
                )
                completed[index] = (room, ok, output)
                submit_next(pool)

    failures = []
    for index in sorted(completed):
        room, ok, output = completed[index]
        if ok:
            print(f"Done with {room}")
        else:
            failures.append((room, output))
    if failures:
        for room, output in failures:
            print(f"\nFAILED: {room}", file=sys.stderr)
            if output:
                print(output, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
