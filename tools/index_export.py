#!/usr/bin/env python3
"""Indexes of the AssetRipper export that the converters read (run once after exporting; HKDATA = the folder holding
the export's ex/):
  metamaps.pkl     (sprite guid -> 'Sprite/<name>.asset', texture guid -> 'Texture2D/<name>.png')
  tex2sprites.pkl  texture guid -> [(sprite name, [x, y, w, h] rect in the texture, sprite guid), ...]
  meta_AudioClip.pkl  audio clip guid -> the clip's file (build_music.py)
(uanim.py and match_textures.py / alpha_cache.py build their own caches the first time they run.)
   python index_export.py [out_dir]       (default: HKDATA)"""

import argparse
import glob
import os
import pickle
import re
from concurrent.futures import ThreadPoolExecutor
from hkcommon import ROOT, EX
from parallel_helpers import interruptible_main, parallel_executor


def guid_of(meta):
    m = re.search(r"^guid: ([0-9a-f]+)", open(meta, errors="replace").read(4000), re.M)
    return m.group(1) if m else None


def read_meta_guid(meta):
    with open(meta, "rb") as handle:
        text = handle.read(4000).decode("utf-8", "replace")
    match = re.search(r"^guid: ([0-9a-f]+)", text, re.M)
    return meta, match.group(1) if match else None


def indexed_guid_pairs(paths, workers):
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        return list(pool.map(read_meta_guid, paths))


def read_sprite(meta):
    meta, guid = read_meta_guid(meta)
    if not guid:
        return None
    path = meta[:-5]
    with open(path, "rb") as handle:
        source = handle.read().decode("utf-8", "replace")
    name = re.search(r"^\s*m_Name:[ \t]*(.*)$", source, re.M)
    rect = re.search(
        r"m_Rect:\s*\n\s*serializedVersion: \d+\s*\n\s*x: ([-0-9.e]+)"
        r"\s*\n\s*y: ([-0-9.e]+)\s*\n\s*width: ([-0-9.e]+)"
        r"\s*\n\s*height: ([-0-9.e]+)",
        source,
    )
    texture = re.search(
        r"^\s*texture: \{fileID: \d+, guid: ([0-9a-f]+)",
        source,
        re.M,
    )
    if not name or not texture:
        return None
    return guid, path, texture.group(1), name.group(1).strip() or "?", (
        [float(value) for value in rect.groups()] if rect else None
    )


def main(out, workers=None):
    import os
    if workers is None:
        workers = min(16, (os.cpu_count() or 1) * 2)
    spr, tex, t2s = {}, {}, {}
    texture_metas = glob.glob(f"{EX}/Texture2D/*.png.meta")
    for meta, g in indexed_guid_pairs(texture_metas, workers):
        if g:
            tex[g] = os.path.relpath(meta[:-5], EX)
    sprite_metas = glob.glob(f"{EX}/Sprite/*.asset.meta")
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for result in pool.map(read_sprite, sprite_metas):
            if not result:
                continue
            guid, path, texture_guid, name, rect = result
            spr[guid] = os.path.relpath(path, EX)
            t2s.setdefault(texture_guid, []).append((name, rect, guid))
    audio = {}
    audio_metas = glob.glob(f"{EX}/AudioClip/*.meta")
    for meta, g in indexed_guid_pairs(audio_metas, workers):
        if g:
            audio[g] = os.path.abspath(meta[:-5])
    os.makedirs(out, exist_ok=True)
    with open(f"{out}/meta_AudioClip.pkl", "wb") as handle:
        pickle.dump(audio, handle)
    with open(f"{out}/metamaps.pkl", "wb") as handle:
        pickle.dump((spr, tex), handle)
    with open(f"{out}/tex2sprites.pkl", "wb") as handle:
        pickle.dump(t2s, handle)
    print(
        len(spr),
        "sprites,",
        len(tex),
        "textures,",
        len(t2s),
        "textures with sprites,",
        len(audio),
        "audio clips",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("out_dir", nargs="?", default=ROOT)
    parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
    args = parser.parse_args()
    raise SystemExit(interruptible_main(main, args.out_dir, max(1, args.workers)))
