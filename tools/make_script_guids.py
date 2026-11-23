#!/usr/bin/env python3
"""Generate tools/data/script_guids.json from the HKDATA Assembly-CSharp export."""

import argparse
import hashlib
import json
import os
import re
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from parallel_helpers import interruptible_main, parallel_executor

CLASS_RE = re.compile(
    r"^\s*(?:public\s+|internal\s+|protected\s+|private\s+)?"
    r"(?:abstract\s+|sealed\s+|static\s+|partial\s+)*"
    r"(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)",
    re.MULTILINE,
)
NAMESPACE_RE = re.compile(r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)", re.MULTILINE)
FILE_NAMESPACE_RE = re.compile(
    r"^\s*namespace\s+([A-Za-z_][A-Za-z0-9_.]*)\s*;", re.MULTILINE
)


def unity_guid(assembly, namespace, class_name):
    """Unity MD5 script GUID calculation (Assembly-CSharp + namespace + class)."""
    digest = bytearray(
        hashlib.md5((assembly + namespace + class_name).encode("utf-8")).digest()
    )

    digest[0:4] = reversed(digest[0:4])
    digest[4:6] = reversed(digest[4:6])
    digest[6:8] = reversed(digest[6:8])
    for i, value in enumerate(digest):
        digest[i] = ((value << 4) & 0xFF) | (value >> 4)

    result = ""
    for offset in range(0, 16, 4):
        word = int.from_bytes(digest[offset : offset + 4], "little")
        result += "".join(
            "0123456789abcdef"[(word >> (shift * 4)) & 0xF] for shift in range(8)
        )
    return result


def brace_namespace(text):
    """Return a namespace from a block or file-scoped namespace declaration."""
    match = FILE_NAMESPACE_RE.search(text)
    if match:
        return match.group(1)

    namespaces = list(NAMESPACE_RE.finditer(text))
    if namespaces:
        declaration = namespaces[0]
        namespace = declaration.group(1)
        if namespace:
            return namespace
    return ""


def find_classes(source_dir, workers=None):
    classes = set()
    paths = sorted(source_dir.rglob("*.cs"))

    def parse(path):
        text = path.read_text(encoding="utf-8", errors="replace")
        namespace = brace_namespace(text)
        assembly = "Assembly-CSharp"
        if "Assembly-CSharp-firstpass" in str(path):
            assembly = "Assembly-CSharp-firstpass"
        found = set()
        for match in CLASS_RE.finditer(text):
            class_name = match.group(1)
            found.add((assembly, namespace, class_name))
        return found

    if workers is None:
        workers = min(16, (os.cpu_count() or 1) * 2)
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for found in pool.map(parse, paths):
            classes.update(found)

    return sorted(classes)


def make_map(hkdata, workers=None):
    source_dir = hkdata / "Assembly-CSharp"
    if not source_dir.is_dir():
        raise FileNotFoundError(
            f"Decompiled Assembly-CSharp export not found: {source_dir}"
        )

    result = {}
    for assembly, namespace, class_name in find_classes(source_dir, workers):
        guid = unity_guid(assembly, namespace, class_name)
        result[guid] = class_name

    # Keep a known GUID that has no usable serialized fields and is easier to
    # identify directly than through the C# signature matcher.
    result.setdefault("e667676c", "AlertRange")

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Create tools/data/script_guids.json from the HKDATA export."
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
        help="output JSON path (default: tools/data/script_guids.json)",
    )
    parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
    args = parser.parse_args()

    hkdata = Path(args.hkdata).expanduser().resolve()
    result = make_map(hkdata, max(1, args.workers))

    output = (
        Path(args.output).expanduser().resolve()
        if args.output
        else Path(__file__).resolve().parent / "data" / "script_guids.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")

    print(f"Wrote {output}: {len(result)} script GUID entries")


if __name__ == "__main__":
    raise SystemExit(interruptible_main(main))
