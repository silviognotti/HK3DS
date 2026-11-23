#!/usr/bin/env python3
"""Verify assets referenced by the HK3DS runtime are present in a ROMFS directory."""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path


def read_lines(path: Path) -> list[str]:
    if not path.is_file():
        return []
    return [line.strip() for line in path.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]


def is_nonempty(path: Path) -> bool:
    try:
        return path.is_file() and path.stat().st_size > 0
    except OSError:
        return False


def literal_runtime_assets(source_dir: Path) -> set[str]:
    assets: set[str] = set()
    pattern = re.compile(r"""["'](?:romfs:/)([^"'\\n]+)["']""")
    paths = list(source_dir.rglob("*.c")) + list(source_dir.rglob("*.h"))
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in pattern.findall(text):
            relative = match
            if "%" in relative:
                continue
            if "<" in relative or "{" in relative or "*" in relative:
                continue
            assets.add(relative.lstrip("/"))
    return assets


def main() -> int:
    parser = argparse.ArgumentParser(description="Check a built HK3DS ROMFS for missing runtime assets")
    parser.add_argument("romfs", nargs="?", default="romfs", help="ROMFS directory (default: romfs)")
    parser.add_argument("--rooms", default=None, help="room list file (default: tools/data/all_rooms.txt)")
    args = parser.parse_args()

    repository = Path(__file__).resolve().parent.parent
    romfs = (Path(args.romfs).expanduser().resolve()
             if os.path.isabs(args.romfs)
             else (repository / args.romfs).resolve())
    rooms_path = (
        Path(args.rooms).expanduser().resolve()
        if args.rooms
        else repository / "tools" / "data" / "all_rooms.txt"
    )

    required_files = {
        "gfx/knight.spr",
        "gfx/enemies.spr",
        "gfx/hud.spr",
        "gfx/ui.spr",
        "gfx/dialog.spr",
        "gfx/font_menu.spr",
        "gfx/font_big.spr",
        "gfx/font_text.spr",
        "gfx/stagmenu.spr",
        "gfx/icons.spr",
        "gfx/areas.spr",
        "gfx/map.spr",
        "gfx/vignette.a8",
        "gfx/herolight.a8",
        "gfx/journal_icons.spr",
        "rooms/_Knight.obj",
        "rooms/_zones.ptc",
        "rooms/_zones_ptc.spr",
        "text/version.txt",
        "text/map.txt",
        "text/darkness.txt",
        "text/nonslider.txt",
        "text/pantheons.txt",
        "text/gg_doors.txt",
        "text/additive.txt",
        "text/area_titles.txt",
        "text/herolight.txt",
        "text/journal_list.txt",
        "text/gg_statues.txt",
        "text/music_cues.txt",
        "text/ptczones.txt",
    }
    for relative in literal_runtime_assets(repository / "source"):
        if relative and "*" not in relative and "%" not in relative:
            required_files.add(relative)

    missing: list[str] = []
    for relative in sorted(required_files):
        if not is_nonempty(romfs / relative):
            missing.append(relative)

    rooms = read_lines(rooms_path)
    for room in rooms:
        if (
            room.endswith("_boss")
            or room.endswith("_boss_defeated")
            or room.endswith("_preload")
            or room in {
                "GG_Boss_Door_Entrance",
                "GG_Entrance_Cutscene",
                "GG_Unlock",
            }
        ):
            continue
        room_file = romfs / "rooms" / f"{room}.room"
        if not is_nonempty(room_file):
            missing.append(f"rooms/{room}.room")
        for suffix in ("lyr", "cam"):
            path = romfs / "rooms" / f"{room}.{suffix}"
            if suffix == "lyr" and not path.is_file():
                continue
            if suffix == "cam" and not path.is_file():
                continue

    if not (romfs / "text").is_dir():
        missing.append("text/")
    if not (romfs / "gfx").is_dir():
        missing.append("gfx/")
    if not (romfs / "rooms").is_dir():
        missing.append("rooms/")
    if not (romfs / "sfx").is_dir():
        missing.append("sfx/")

    missing = sorted(set(missing))
    if missing:
        print(f"ROMFS check failed: {romfs}")
        for relative in missing:
            print(f"  MISSING {relative}")
        return 1

    print(
        f"ROMFS check passed: {romfs} "
        f"({len(rooms)} rooms, {sum(1 for p in romfs.rglob('*') if p.is_file())} files)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
