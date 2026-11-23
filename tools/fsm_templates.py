#!/usr/bin/env python3
"""Map PlayMaker template GUIDs to their MonoBehaviour asset paths.

Usage:
  python fsm_templates.py [--quiet]
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import sys
from concurrent.futures import ProcessPoolExecutor, as_completed
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from parallel_helpers import interruptible_main, parallel_executor
from pathlib import Path
from typing import Iterable

import fsm_extract as F
from hkcommon import EX, ROOT

KINDS = (
    "floatVariables",
    "intVariables",
    "boolVariables",
    "stringVariables",
    "vector2Variables",
    "vector3Variables",
    "rectVariables",
    "quaternionVariables",
    "colorVariables",
    "gameObjectVariables",
    "objectVariables",
    "materialVariables",
    "textureVariables",
    "arrayVariables",
    "enumVariables",
)
TEMPLATE_MARKER = "fsmTemplate:"


def signature(fsm: dict) -> tuple:
    variables = fsm.get("variables") or {}
    return tuple(
        sorted(
            (kind, str(variable.get("name")))
            for kind in KINDS
            for variable in (variables.get(kind) or [])
            if isinstance(variable, dict)
        )
    )


def iter_asset_files(directory: str) -> Iterable[str]:
    for root, _, names in os.walk(directory):
        for name in names:
            if name.endswith(".unity") or name.endswith(".prefab"):
                yield os.path.join(root, name)


def status(message: str, quiet: bool) -> None:
    if not quiet:
        print(message, file=sys.stderr)


def scan_template_asset(path: str) -> tuple[str, str, tuple] | None:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            src = handle.read()
    except OSError:
        return None
    if "usedInTemplate" not in src:
        return None
    for _, (type_name, value) in F.load_objs_text(src).items():
        if type_name != "MonoBehaviour" or not isinstance(value, dict):
            continue
        fsm = value.get("fsm")
        if isinstance(fsm, dict):
            return path, str(value.get("m_Name") or ""), signature(fsm)
    return None


def scan_template_uses(path: str) -> tuple[dict[str, collections.Counter], int]:
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            src = handle.read()
    except OSError:
        return {}, 0
    if TEMPLATE_MARKER not in src or "11400000" not in src:
        return {}, 0
    found: dict[str, collections.Counter] = {}
    for _, (type_name, value) in F.load_objs_text(src).items():
        if type_name != "MonoBehaviour" or not isinstance(value, dict):
            continue
        fsm = value.get("fsm")
        if not isinstance(fsm, dict):
            continue
        template = value.get("fsmTemplate") or {}
        guid = template.get("guid")
        if guid:
            found.setdefault(str(guid), collections.Counter())[
                (str(fsm.get("name")), signature(fsm))
            ] += 1
    return found, 1


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Map PlayMaker template GUIDs to template assets"
    )
    parser.add_argument(
        "--quiet", action="store_true", help="only print the final summary"
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=max(1, min(16, os.cpu_count() or 1)),
        help="parallel worker processes (default: up to 16)",
    )
    args = parser.parse_args(list(argv) if argv is not None else None)
    worker_count = max(1, args.workers)

    template_assets: dict[str, tuple[str, tuple]] = {}
    mono_behaviour_dir = os.path.join(EX, "MonoBehaviour")
    asset_paths = (
        [
            os.path.join(mono_behaviour_dir, name)
            for name in os.listdir(mono_behaviour_dir)
            if name.endswith(".asset")
        ]
        if os.path.isdir(mono_behaviour_dir)
        else []
    )

    status(f"[1/3] scanning {len(asset_paths)} template candidates", args.quiet)
    with parallel_executor(ProcessPoolExecutor, max_workers=worker_count) as pool:
        futures = {pool.submit(scan_template_asset, path): path for path in asset_paths}
        completed = 0
        for future in as_completed(futures):
            completed += 1
            found = future.result()
            if found:
                path, name, fsm_signature = found
                template_assets[path] = (name, fsm_signature)
            if not args.quiet and completed % 100 == 0:
                status(f"  candidates: {completed}/{len(asset_paths)}", args.quiet)

    by_signature: dict[tuple, list[str]] = collections.defaultdict(list)
    for path, (_, fsm_signature) in template_assets.items():
        by_signature[fsm_signature].append(path)

    scene_files = list(iter_asset_files(EX))
    status(f"[2/3] scanning {len(scene_files)} scene/prefab files", args.quiet)
    uses: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    scanned = 0
    template_files = 0
    with parallel_executor(ProcessPoolExecutor, max_workers=worker_count) as pool:
        futures = {pool.submit(scan_template_uses, path): path for path in scene_files}
        completed = 0
        for future in as_completed(futures):
            completed += 1
            found, contains_template = future.result()
            scanned += 1
            template_files += contains_template
            for guid, counts in found.items():
                uses[guid].update(counts)
            if not args.quiet and completed % 100 == 0:
                status(
                    f"  files: {completed}/{len(scene_files)} ({template_files} with templates)",
                    args.quiet,
                )

    status(f"[3/3] matching {len(uses)} unique template GUIDs", args.quiet)
    output: dict[str, str] = {}
    unmatched: list[tuple[str, collections.Counter, int]] = []
    for guid, counts in sorted(uses.items()):
        (name, fsm_signature), _ = counts.most_common(1)[0]
        candidates = by_signature.get(fsm_signature, [])
        pick = candidates[0] if len(candidates) == 1 else None
        if pick is None:
            names = {used_name for used_name, _ in counts}
            pool = candidates or list(template_assets)
            for candidate in pool:
                if template_assets[candidate][0] in names:
                    pick = candidate
                    break
        if pick:
            output[guid] = os.path.join("hkdata", os.path.relpath(pick, ROOT)).replace(
                "\\", "/"
            )
            if not output[guid].startswith("../"):
                pass
        else:
            unmatched.append((guid, counts, len(candidates)))

    output_path = Path(__file__).resolve().parent / "data" / "fsm_templates.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(".json.tmp")
    temporary_path.write_text(
        json.dumps(output, indent=1, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    temporary_path.replace(output_path)

    for guid, counts, candidate_count in unmatched[:25]:
        print(
            f"unmatched {guid} {counts.most_common(2)} candidates={candidate_count}",
            file=sys.stderr,
        )
    if len(unmatched) > 25:
        print(f"unmatched ... {len(unmatched) - 25} more", file=sys.stderr)
    status(
        f"matched {len(output)} of {len(uses)} templates "
        f"(scanned {scanned} files, {len(template_assets)} assets)",
        args.quiet,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
