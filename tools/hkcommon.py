# Shared helpers for reading the AssetRipper export of Hollow Knight.
import re, os, glob, pickle, math, functools
from concurrent.futures import ThreadPoolExecutor
from parallel_helpers import interruptible_main, parallel_executor
from pathlib import Path

import yaml
from dotenv import load_dotenv

root_folder = Path(__file__).parent.parent

if (env_file := (root_folder / ".env")).exists():
    load_dotenv(env_file)

Loader = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
ROOT = Path(os.environ.get("HKDATA", "hkdata"))
ROOT = (
    (ROOT if ROOT.is_absolute() else root_folder / ROOT)
)
if not os.path.exists(ROOT):
    raise Exception(f"HKData not found at {ROOT}")
EX = f"{ROOT}/ex"
KNIGHT_ATLAS = os.environ.get(
    "KNIGHT_ATLAS", f"{ROOT}/hs/Texture2D/atlas0_325.png"
)

@functools.lru_cache(maxsize=128)
def _load_yaml_docs_cached(path, size, mtime_ns):
    del size, mtime_ns
    src = open(path, encoding="utf-8").read()
    parts = re.split(r"^--- !u!(\d+) &(-?\d+).*$", src, flags=re.M)
    objs = {}
    for i in range(1, len(parts), 3):
        try:
            d = yaml.load(parts[i + 2], Loader=Loader)
        except Exception:
            continue
        if d:
            ((typ, val),) = d.items()
            objs[int(parts[i + 1])] = (typ, val)
    return objs


def load_yaml_docs(path):
    try:
        stat = os.stat(path)
    except OSError:
        return {}
    return _load_yaml_docs_cached(path, stat.st_size, stat.st_mtime_ns)


@functools.lru_cache(None)
def metamap(folder):
    cache = f"{ROOT}/meta_{folder}.pkl"
    if os.path.exists(cache):
        return pickle.load(open(cache, "rb"))
    g = {}
    metas = glob.glob(f"{EX}/{folder}/*.meta")

    def read_meta(meta):
        with open(meta, "rb") as handle:
            text = handle.read(2000).decode("utf-8", "replace")
        match = re.search(r"guid: ([0-9a-f]+)", text)
        if not match:
            return None, None
        return match.group(1), meta[:-5]

    workers = min(16, (os.cpu_count() or 1) * 2)
    with parallel_executor(ThreadPoolExecutor, max_workers=workers) as pool:
        for guid, path in pool.map(read_meta, metas):
            if guid:
                g[guid] = path
    with open(cache, "wb") as handle:
        pickle.dump(g, handle)
    return g


@functools.lru_cache(None)
def hs_texmatch():
    p = f"{ROOT}/hs_texmatch.pkl"
    return pickle.load(open(p, "rb")) if os.path.exists(p) else {}


def texture_path(guid):
    t = metamap("Texture2D").get(guid)
    if t:
        return t
    return hs_texmatch().get(guid)


def f(v):
    try:
        return float(v)
    except:
        return 0.0
