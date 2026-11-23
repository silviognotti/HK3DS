# Build a map of C# class -> serialized field names from the decompiled Assembly-CSharp.
import argparse
import glob
import json
import os
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from parallel_helpers import interruptible_main, parallel_executor

SRC = os.environ.get("HKDATA", "hkdata") + "/Assembly-CSharp"


def parse_file(path):
    found = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        text = handle.read()
    pattern = re.compile(
        r"^(?:public |internal )?(?:sealed |abstract )?(?:partial )?"
        r"class (\w+)(?:\s*:\s*([\w\.<>, ]+))?",
        re.M,
    )
    field_pattern = re.compile(
        r"^\t(?:\[[^\]]*\]\s*)*(?:(?:public|private|protected|internal)\s+)?"
        r"((?:static|const|readonly)\s+)?([\w\.\[\]<>,]+)\s+(\w+)\s*(?:=[^;]*)?;",
        re.M,
    )
    for match in pattern.finditer(text):
        name = match.group(1)
        base = (match.group(2) or "").split(",")[0].strip()
        fields = []
        for field in field_pattern.finditer(text, match.end()):
            if field.group(1) and field.group(1).strip() in ("static", "const"):
                continue
            fields.append(field.group(3))
        found[name] = {"base": base, "fields": fields}
    return found


def classes(workers=None):
    if workers is None:
        workers = min(16, (os.cpu_count() or 1) * 2)
    out = {}
    files = glob.glob(f"{SRC}/**/*.cs", recursive=True)
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for found in pool.map(parse_file, files):
            out.update(found)
    return out


def identify(keys, sigs, cache={}):
    """Best class for a MonoBehaviour's serialized keys (all keys must be fields of the class)."""
    ks = frozenset(key for key in keys if not key.startswith("m_"))
    if ks in cache:
        return cache[ks]
    best, score = None, -1
    if ks:
        for name, data in sigs.items():
            fields = set(data["fields"])
            if ks <= fields:
                candidate = len(ks) / (len(fields) + 1)
                if candidate > score:
                    best, score = name, candidate
    cache[ks] = best
    return best


if __name__ == "__main__":
    def run():
        parser = argparse.ArgumentParser()
        parser.add_argument("--workers", type=int, default=min(16, (os.cpu_count() or 1) * 2))
        args = parser.parse_args()
        result = classes(max(1, args.workers))
        output = os.path.join(os.path.dirname(__file__), "data", "cs_sigs.json")
        os.makedirs(os.path.dirname(output), exist_ok=True)
        with open(output, "w", encoding="utf-8") as handle:
            json.dump(result, handle)
        print(len(result), "classes")
        print(result.get("HealthManager", {}).get("fields", [])[:20])
        print(result.get("Walker"))
    raise SystemExit(interruptible_main(run))
