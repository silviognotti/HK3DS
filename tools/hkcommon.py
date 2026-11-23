# Shared helpers for reading the AssetRipper export of Hollow Knight.
import re, os, glob, pickle, math, functools, json
import yaml
Loader = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
ROOT = os.environ.get('HKDATA', 'hkdata')
EX = f'{ROOT}/ex'
# the Knight's tk2d atlas, which the export doesn't link to his sprite collection: the texture dump's atlas0_325
# (pixel for pixel the same as the atlas this was first built from)
KNIGHT_ATLAS = os.environ.get('KNIGHT_ATLAS', f'{ROOT}/hs/Texture2D/atlas0_325.png')

# Tables keyed by asset guids fit only the export they were made from: AssetRipper gives assets new random guids on
# every export (scripts keep theirs, which come from the class name). make_lookups.py makes them from your own export
# into $HKDATA/lookups/; the copies in tools/data are the ones the repository was built with, used when there are none.
LOOKUPS = f'{ROOT}/lookups'
TOOLS_DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data')
def lookup_path(name):
    p = os.path.join(LOOKUPS, name)
    return p if os.path.exists(p) else os.path.join(TOOLS_DATA, name)

@functools.lru_cache(None)
def export_ids():
    """make_lookups.py's export_ids.json: lit shaders, additive materials, the vignette and HeroLight sprites, fonts"""
    p = os.path.join(LOOKUPS, 'export_ids.json')
    return json.load(open(p)) if os.path.exists(p) else {}

def load_yaml_docs(path):
    src = open(path, encoding='utf-8').read()
    parts = re.split(r'^--- !u!(\d+) &(-?\d+).*$', src, flags=re.M)
    objs = {}
    for i in range(1, len(parts), 3):
        try:
            d = yaml.load(parts[i + 2], Loader=Loader)
        except Exception:
            continue
        if d:
            (typ, val), = d.items()
            objs[int(parts[i + 1])] = (typ, val)
    return objs

@functools.lru_cache(None)
def metamap(folder):
    cache = f'{ROOT}/meta_{folder}.pkl'
    if os.path.exists(cache): return pickle.load(open(cache, 'rb'))
    g = {}
    for m in glob.glob(f'{EX}/{folder}/*.meta'):
        r = re.search(r'guid: ([0-9a-f]+)', open(m).read(2000))
        if r: g[r.group(1)] = m[:-5]
    pickle.dump(g, open(cache, 'wb'))
    return g

@functools.lru_cache(None)
def hs_texmatch():
    p = f'{ROOT}/hs_texmatch.pkl'
    return pickle.load(open(p, 'rb')) if os.path.exists(p) else {}

def texture_path(guid):
    t = metamap('Texture2D').get(guid)
    if t: return t
    return hs_texmatch().get(guid)

def f(v):
    try: return float(v)
    except: return 0.0
