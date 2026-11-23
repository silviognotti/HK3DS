#!/usr/bin/env python3
"""Generate tools/data/particle_textures.json from exported particle materials."""

import argparse
import json
import os
import pickle
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


NAME_RE = re.compile(r"^\s*m_Name:\s*(.*)$", re.MULTILINE)
GUID_RE = re.compile(r"guid:\s*([0-9a-f]+)", re.IGNORECASE)


def parse_material(path):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    name_match = NAME_RE.search(text)
    if not name_match:
        return None
    name = name_match.group(1).strip()
    main_tex = re.search(
        r"_MainTex:\s*\n\s*m_Texture:\s*\{[^}]*guid:\s*([0-9a-f]+)",
        text,
    )
    if not main_tex:
        lines = text.splitlines()
        texture_guid = None
        in_main_tex = False
        for line in lines:
            if "_MainTex:" in line:
                in_main_tex = True
                continue
            if in_main_tex and "m_Texture:" in line:
                guid = GUID_RE.search(line)
                if guid:
                    texture_guid = guid.group(1).lower()
                break
            if in_main_tex and line.strip() and not line.startswith(" "):
                break
        if not texture_guid:
            return None
        return name, texture_guid
    if not main_tex:
        return None
    return name, main_tex.group(1).lower()


def make_map(hkdata, workers=None):
    ex_dir = hkdata / "ex"
    material_dir = ex_dir / "Material"
    if not material_dir.is_dir():
        raise FileNotFoundError(f"Material export not found: {material_dir}")

    paths = sorted(material_dir.glob("*.mat"))
    if workers is None:
        workers = min(16, (os.cpu_count() or 1) * 2)

    found = {}
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for result in pool.map(parse_material, paths):
            if result:
                name, texture_guid = result
                found[name] = texture_guid

    tex2sprites = {}
    tex_index = hkdata / "tex2sprites.pkl"
    if tex_index.is_file():
        with tex_index.open("rb") as handle:
            tex2sprites = pickle.load(handle)

    metamap = {}
    metamap_index = hkdata / "metamaps.pkl"
    if metamap_index.is_file():
        with metamap_index.open("rb") as handle:
            _, metamap = pickle.load(handle)

    result = {}
    sprite_dir = ex_dir / "Sprite"
    sprite_names = {
        path.stem
        for path in sprite_dir.glob("*.asset")
    } if sprite_dir.is_dir() else set()
    for material_name, texture_guid in found.items():
        candidates = tex2sprites.get(texture_guid) or []
        names = [candidate[0].strip() for candidate in candidates if candidate and candidate[0]]
        names = list(dict.fromkeys(names))
        if len(names) == 1:
            result[material_name] = names[0]
        elif names:
            result[material_name] = names[0]
        elif material_name in sprite_names:
            result[material_name] = material_name
        elif texture_guid in metamap:
            texture_path = metamap[texture_guid]
            result[material_name] = os.path.splitext(os.path.basename(texture_path))[0]

    if not result:
        raise RuntimeError("No particle texture mappings could be generated")
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("hkdata", nargs="?", default=os.environ.get("HKDATA", "hkdata"))
    parser.add_argument("--output", default=None)
    parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
    args = parser.parse_args()

    hkdata = Path(args.hkdata).expanduser().resolve()
    result = make_map(hkdata, max(1, args.workers))
    output = (
        Path(args.output).expanduser().resolve()
        if args.output
        else Path(__file__).resolve().parent / "data" / "particle_textures.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(output)
    print(f"Wrote {output}: {len(result)} particle texture mappings")


if __name__ == "__main__":
    raise SystemExit(main())
