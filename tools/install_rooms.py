#!/usr/bin/env python3
"""Parallel install of built Hollow Knight rooms."""

import argparse
import os
import shutil
import subprocess
import sys
import traceback
import time
import tempfile
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from pathlib import Path

from parallel_helpers import interruptible_main, parallel_executor

sys.path.insert(0, str(Path(__file__).resolve().parent))
import camlocks
import lyr_v2
import masks
import patch_room_audio
import zwrap


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


def atomic_write(path, text):
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


def already_installed(room, room_build, romfs):
    source = Path(room_build)
    destination = Path(romfs) / "rooms"
    room_file = destination / f"{room}.room"
    if not room_file.is_file():
        return False
    if not (destination / f"{room}.cam").is_file():
        return False
    source_room = source / f"{room}.room"
    if source_room.is_file() and room_file.stat().st_mtime_ns < source_room.stat().st_mtime_ns:
        return False
    for suffix in ("lyr", "ents.spr", "enemies.spr"):
        source_path = source / f"{room}.{suffix}" if suffix == "lyr" else source / f"{room}_{suffix}"
        destination_path = destination / source_path.name
        if source_path.is_file():
            if not destination_path.is_file():
                return False
            if destination_path.stat().st_mtime_ns < source_path.stat().st_mtime_ns:
                return False
    return True


def install_one(room, room_build, romfs, tools, status_dir, index):
    output = []
    status_dir = Path(status_dir)
    atomic_write(status_dir / f"{room}.status", "RUNNING\n")
    room_log = status_dir / f"{room}.log"
    atomic_write(room_log, "running\n")

    def phase(message):
        output.append(message)
        atomic_write(room_log, "\n".join(output) + "\n")

    room_build = Path(room_build)
    romfs_rooms = Path(romfs) / "rooms"
    romfs_rooms.mkdir(parents=True, exist_ok=True)

    source_room = room_build / f"{room}.room"
    if source_room.is_file():
        phase("patch room audio")
        patch_room_audio.patch_room(room_build, room)
        shutil.copy2(source_room, romfs_rooms / source_room.name)

    for suffix in (".lyr", "_ents.spr", "_enemies.spr"):
        source = room_build / f"{room}{suffix}"
        if source.is_file():
            shutil.copy2(source, romfs_rooms / source.name)

    lyr = romfs_rooms / f"{room}.lyr"
    if lyr.is_file():
        phase("convert layer")
        lyr_v2.convert_layer(str(lyr))

    for path in (
        romfs_rooms / f"{room}.room",
        lyr,
        romfs_rooms / f"{room}_ents.spr",
        romfs_rooms / f"{room}_enemies.spr",
    ):
        if path.is_file():
            phase(f"wrap {path.name}")
            zwrap.wrap_file(str(path))

    phase("camera post-processing")
    try:
        camlocks.export(room, str(romfs_rooms), quiet=True)
    except Exception as error:
        return index, room, False, "".join(
            traceback.format_exception(type(error), error, error.__traceback__)
        )

    phase("mask post-processing")
    try:
        masks.export(room, str(romfs_rooms))
    except Exception as error:
        return index, room, False, "".join(
            traceback.format_exception(type(error), error, error.__traceback__)
        )

    mask_sheet = romfs_rooms / f"{room}_msk.spr"
    if mask_sheet.is_file():
        zwrap.wrap_file(str(mask_sheet))

    return index, room, True, "\n".join(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--room-build", required=True)
    parser.add_argument("--romfs", required=True)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("rooms", nargs="+")
    args = parser.parse_args()

    tools = Path(__file__).resolve().parent
    room_build = os.path.abspath(args.room_build)
    romfs = os.path.abspath(args.romfs)
    jobs = max(1, args.jobs)
    if jobs > len(args.rooms):
        jobs = max(1, len(args.rooms))

    failures = []
    status_dir = tools.parent / "work" / "logs" / "install"
    status_dir.mkdir(parents=True, exist_ok=True)
    completed_count = 0
    terminal_width = shutil.get_terminal_size((80, 20)).columns
    future_started = {}
    pending_rooms = [
        room
        for room in args.rooms
        if not already_installed(room, room_build, romfs)
    ]
    skipped_count = len(args.rooms) - len(pending_rooms)

    for room in args.rooms:
        if room not in pending_rooms:
            atomic_write(status_dir / f"{room}.status", "SKIPPED\n")
            atomic_write(status_dir / f"{room}.log", "already installed\n")
            print(f"[install] {room}: SKIPPED", flush=True)

    def report_progress(index, room, ok, output):
        nonlocal completed_count
        room_log = status_dir / f"{room}.log"
        atomic_write(room_log, output + ("\n" if output else ""))
        status = "OK" if ok else "FAILED"
        atomic_write(status_dir / f"{room}.status", status + "\n")
        completed_count += 1
        room_elapsed = (
            time.monotonic() - future_started.pop(room)
            if room in future_started
            else 0.0
        )
        line = (
            f"[install {completed_count}/{len(pending_rooms)}]"
            f" {room}: {status} ({room_elapsed:.1f}s)"
        )
        print(line[:terminal_width], flush=True)

    with parallel_executor(
        ThreadPoolExecutor,
        max_workers=jobs,
        thread_name_prefix="install-room",
    ) as pool:
        for room in pending_rooms:
            atomic_write(status_dir / f"{room}.status", "PENDING\n")
            atomic_write(status_dir / f"{room}.log", "pending\n")
        futures = {}
        room_queue = [
            (index, room)
            for index, room in enumerate(args.rooms)
            if room in pending_rooms
        ]
        completed = {}
        next_queue = 0

        def submit_next():
            nonlocal next_queue
            if next_queue >= len(room_queue):
                return False
            index, room = room_queue[next_queue]
            next_queue += 1
            future = pool.submit(
                install_one,
                room,
                room_build,
                romfs,
                tools,
                status_dir,
                index,
            )
            futures[future] = (index, room)
            future_started[room] = time.monotonic()
            atomic_write(status_dir / f"{room}.status", "RUNNING\n")
            atomic_write(status_dir / f"{room}.log", "running\n")
            # print(f"== {room}", flush=True)
            return True

        while len(futures) < jobs:
            if not submit_next():
                break

        while futures:
            done, _ = wait(
                futures,
                return_when=FIRST_COMPLETED,
            )
            for future in done:
                index, room = futures.pop(future)
                try:
                    index, room, ok, output = future.result()
                except BaseException as error:
                    output = "".join(
                        traceback.format_exception(type(error), error, error.__traceback__)
                    )
                    ok = False
                report_progress(index, room, ok, output)
                completed[index] = (room, ok, output)
                submit_next()

    if not pending_rooms:
        print(f"Skipped {skipped_count} already-installed rooms")
        return 0

    for index in sorted(completed):
        room, ok, output = completed[index]
        if output:
            print(output)
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

    result = run(
        [sys.executable, str(tools / "fill_gate_targets.py"),
         str(Path(romfs) / "rooms"), str(room_build)],
        cwd=tools,
    )
    if result.stdout.strip():
        print(result.stdout.strip())
    if result.returncode:
        return result.returncode

    result = run(
        [sys.executable, str(tools / "patch_gate_conditions.py"),
         str(Path(romfs) / "rooms"), *args.rooms],
        cwd=tools,
    )
    if result.stdout.strip():
        print(result.stdout.strip())
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
