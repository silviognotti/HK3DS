#!/usr/bin/env python3
"""Build tools/data/audio_map.json from an AssetRipper export."""

import argparse
import json
import os
import re
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from parallel_helpers import interruptible_main, parallel_executor

MUSIC_CUE_SCRIPT = "400403b587ef0def1b40de34c0f1ebec"
GUID_RE = re.compile(r"^guid:\s*([0-9a-fA-F]{32})", re.MULTILINE)
GUID_REF_RE = re.compile(r"guid:\s*([0-9a-fA-F]{32})", re.IGNORECASE)


def cue_name(name):
    return name.lower().replace(" ", "_")


def read_guid(meta_path):
    match = GUID_RE.search(meta_path.read_text(errors="replace")[:4096])
    return match.group(1).lower() if match else None


def audio_name(asset_path):
    name = asset_path.name
    if name.endswith(".ogg"):
        name = name[:-4]
    elif name.endswith(".wav"):
        name = name[:-4]
    elif name.endswith(".asset"):
        name = name[:-6]
    return cue_name(name)


def resolve_asset(ex_dir, meta_path):
    asset = Path(str(meta_path)[:-5])
    if not asset.exists():
        return None
    return asset


def make_map(hkdata, workers=None):
    ex_dir = hkdata / "ex"
    if not ex_dir.is_dir():
        raise FileNotFoundError(f"AssetRipper export not found: {ex_dir}")

    guid_assets = {}
    audio_names = {}
    music = {}
    meta_paths = list(ex_dir.rglob("*.meta"))

    def scan_meta(meta_path):
        guid = read_guid(meta_path)
        if not guid:
            return None
        asset = resolve_asset(ex_dir, meta_path)
        if asset is None:
            return None
        try:
            relative = asset.relative_to(ex_dir)
        except ValueError:
            return None
        if relative.parts and relative.parts[0] == "AudioClip":
            return guid, asset, "audio", None
        if relative.parts and relative.parts[0] == "MonoBehaviour":
            text = asset.read_text(errors="replace")
            if MUSIC_CUE_SCRIPT in text and "channelInfos" in text:
                return guid, asset, "music", cue_name(asset.stem)
        return guid, asset, None, None

    if workers is None:
        workers = min(16, (os.cpu_count() or 1) * 2)
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for result in pool.map(scan_meta, meta_paths):
            if not result:
                continue
            guid, asset, kind, cue = result
            guid_assets[guid] = asset
            if kind == "audio":
                audio_names[guid] = audio_name(asset)
            elif kind == "music":
                music[guid] = cue

    atmos = {}
    atmos.update(audio_names)

    # Some AtmosCue GUIDs point to MonoBehaviour assets instead of AudioClip
    # assets. Resolve those assets through their AudioClip GUID references.
    def resolve_atmos(item):
        guid, asset = item
        try:
            relative = asset.relative_to(ex_dir)
        except ValueError:
            return None
        if not relative.parts or relative.parts[0] != "MonoBehaviour":
            return None
        text = asset.read_text(errors="replace")
        candidates = []
        for ref_guid in GUID_REF_RE.findall(text):
            ref_guid = ref_guid.lower()
            if ref_guid in audio_names and audio_names[ref_guid] not in candidates:
                candidates.append(audio_names[ref_guid])
        return (guid, candidates[0]) if len(candidates) == 1 else None

    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for result in pool.map(resolve_atmos, guid_assets.items()):
            if result:
                guid, name = result
                atmos[guid] = name

    return {"music": music, "atmos": atmos}


def main():
    parser = argparse.ArgumentParser(
        description="Create tools/data/audio_map.json from the HKDATA AssetRipper export."
    )
    parser.add_argument(
        "hkdata",
        nargs="?",
        default=os.environ.get("HKDATA", "hkdata"),
        help="HKDATA folder (default: $HKDATA or ./hkdata)",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help="output JSON path (default: tools/data/audio_map.json beside this script)",
    )
    parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
    args = parser.parse_args()

    hkdata = Path(args.hkdata).expanduser().resolve()
    result = make_map(hkdata, max(1, args.workers))

    output = (
        Path(args.output).expanduser().resolve()
        if args.output
        else Path(__file__).resolve().parent / "data" / "audio_map.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")

    print(
        f"Wrote {output}: "
        f"{len(result['music'])} music GUIDs, "
        f"{len(result['atmos'])} atmos GUIDs"
    )


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
