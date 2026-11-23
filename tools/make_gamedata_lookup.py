#!/usr/bin/env python3
"""Generate export-derived gamedata lookup text files."""

import argparse
import json
import os
import re
import sys
import pickle
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def guid_from_meta(path):
    try:
        match = re.search(
            r"^guid:\s*([0-9a-f]+)",
            path.read_text(encoding="utf-8", errors="replace")[:4096],
            re.M,
        )
        return match.group(1).lower() if match else None
    except OSError:
        return None


def make_gg_statues(hkdata):
    ex_dir = hkdata / "ex"
    result = {}
    extra_names = {
        "Nosk Hornet": ("statueStateNoskHornet", "GG_Nosk_Hornet"),
        "Nailmasters": ("statueStateNailmasters", "GG_Nailmasters"),
        "Nightmare Grimm": ("statueStateNightmareGrimm", "GG_Grimm_Nightmare"),
        "Gruz Mother": ("statueStateGruzMother", "GG_Gruz_Mother"),
        "Mantis Lords Extra": ("statueStateMantisLordsExtra", "GG_Mantis_Lords_V"),
        "Crystal Guardian 1": ("statueStateCrystalGuardian1", "GG_Crystal_Guardian"),
    }
    for suffix, (state, scene) in extra_names.items():
        if any(suffix.lower() in path.stem.lower() for path in ex_dir.rglob("* Boss Scene.asset")):
            result[state] = scene
    result.setdefault("statueStateGruzMother", "GG_Gruz_Mother")
    result.setdefault("statueStateNailmasters", "GG_Nailmasters")
    result.setdefault("statueStateMantisLordsExtra", "GG_Mantis_Lords_V")
    result.setdefault("statueStateCrystalGuardian1", "GG_Crystal_Guardian")
    paths = sorted(ex_dir.rglob("* Boss Scene.asset"))
    for path in paths:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        scene_match = re.search(r"sceneName:\s*([A-Za-z0-9_]+)", text)
        if not scene_match:
            continue
        name = path.stem
        state_names = {
            "Gruz Mother": "GruzMother",
            "Vengefly": "Vengefly",
            "Brooding Mawlek": "BroodingMawlek",
            "False Knight": "FalseKnight",
            "Failed Champion": "FailedChampion",
            "Hornet 1": "Hornet1",
            "Hornet 2": "Hornet2",
            "Mega Moss Charger": "MegaMossCharger",
            "Mantis Lords": "MantisLords",
            "Oblobbles": "Oblobbles",
            "Grey Prince Zote": "GreyPrince",
            "Broken Vessel": "BrokenVessel",
            "Lost Kin": "LostKin",
            "Nosk": "Nosk",
            "Flukemarm": "Flukemarm",
            "Collector": "Collector",
            "Watcher Knights": "WatcherKnights",
            "Soul Master": "SoulMaster",
            "Soul Tyrant": "SoulTyrant",
            "God Tamer": "GodTamer",
            "Crystal Guardian 1": "CrystalGuardian1",
            "Crystal Guardian 2": "CrystalGuardian2",
            "Uumuu": "Uumuu",
            "Dung Defender": "DungDefender",
            "White Defender": "WhiteDefender",
            "Hive Knight": "HiveKnight",
            "Traitor Lord": "TraitorLord",
            "Grimm": "Grimm",
            "Nightmare Grimm": "NightmareGrimm",
            "Hollow Knight": "HollowKnight",
            "Elder Hu": "ElderHu",
            "Galien": "Galien",
            "Markoth": "Markoth",
            "Marmu": "Marmu",
            "No Eyes": "NoEyes",
            "Xero": "Xero",
            "Gorb": "Gorb",
            "Radiance": "Radiance",
            "Sly": "Sly",
            "Nailmasters": "Nailmasters",
            "Mage Knight": "MageKnight",
            "Paintmaster": "Paintmaster",
            "Zote": "Zote",
            "Nosk Hornet": "NoskHornet",
            "Mantis Lords Extra": "MantisLordsExtra",
        }
        scene = scene_match.group(1)
        for suffix, state in state_names.items():
            if suffix.lower() in name.lower():
                result["statueState" + state] = scene
                break
    return result


def make_music_cues(hkdata):
    audio_map = Path(__file__).resolve().parent / "data" / "audio_map.json"
    if not audio_map.is_file():
        return {}
    data = json.loads(audio_map.read_text(encoding="utf-8"))
    music = data.get("music") or {}
    return {guid: name for guid, name in music.items()}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("hkdata", nargs="?", default=os.environ.get("HKDATA", "hkdata"))
    args = parser.parse_args()
    hkdata = Path(args.hkdata).expanduser().resolve()
    data_dir = Path(__file__).resolve().parent / "data"
    data_dir.mkdir(parents=True, exist_ok=True)

    statues = make_gg_statues(hkdata)
    music = make_music_cues(hkdata)

    statue_path = data_dir / "gg_statues.txt"
    statue_path.write_text(
        "".join(f"{key} {value}\n" for key, value in sorted(statues.items())),
        encoding="utf-8",
    )

    music_path = data_dir / "music_cues.txt"
    music_path.write_text(
        "".join(f"{guid} {name}\n" for guid, name in sorted(music.items())),
        encoding="utf-8",
    )
    print(
        f"Wrote {statue_path}: {len(statues)} statues; "
        f"{music_path}: {len(music)} cues"
    )


if __name__ == "__main__":
    raise SystemExit(main())
