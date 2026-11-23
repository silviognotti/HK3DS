#!/usr/bin/env python3
"""PlayMaker FSM templates: PlayMakerFSM components with fsmTemplate run the template asset's states and actions with
their own variable values. The template assets (MonoBehaviour/*.asset) have no .meta, so their guids are matched here
from how the scenes use them: the component's variable names must equal the template's, then the name decides.
   python fsm_templates.py   -> $HKDATA/lookups/fsm_templates.json {guid: asset path in the export's ex/}
(make_lookups.py runs it; the guids are this export's own, issue #6)"""
import sys, os, re, json, glob, subprocess, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
from hkcommon import EX, LOOKUPS

KINDS = ('floatVariables', 'intVariables', 'boolVariables', 'stringVariables', 'vector2Variables', 'vector3Variables',
         'colorVariables', 'rectVariables', 'quaternionVariables', 'gameObjectVariables', 'objectVariables', 'materialVariables',
         'textureVariables', 'arrayVariables', 'enumVariables')
def sig(fsm):
    V = fsm.get('variables') or {}
    return tuple(sorted((k, str(x.get('name'))) for k in KINDS for x in (V.get(k) or [])))

def main():
    assets = {}
    for p in glob.glob(f'{EX}/MonoBehaviour/*.asset'):
        t = open(p, errors='ignore').read()
        if 'usedInTemplate' not in t: continue
        for k, (ty, v) in F.load_objs(p).items():
            if isinstance(v.get('fsm'), dict): assets[p] = (str(v.get('m_Name')), sig(v['fsm']))
    bysig = collections.defaultdict(list)
    for p, (n, s) in assets.items(): bysig[s].append(p)
    uses = collections.defaultdict(collections.Counter)    # guid -> Counter((component fsm name, sig))
    files = subprocess.run(['grep', '-rl', 'fsmTemplate: {fileID: 11400000', '--include=*.unity', '--include=*.prefab', EX], capture_output=True, text=True).stdout.split('\n')
    for f in files:
        if not f: continue
        try: objs = F.load_objs(f)
        except Exception: continue
        for k, (t, v) in objs.items():
            if t != 'MonoBehaviour' or not isinstance(v.get('fsm'), dict): continue
            g = (v.get('fsmTemplate') or {}).get('guid')
            if not g: continue
            uses[g][(str(v['fsm'].get('name')), sig(v['fsm']))] += 1
        if len(uses) >= 217 and sum(sum(c.values()) for c in uses.values()) > 3000: pass
    out = {}
    for g, cnt in uses.items():
        (name, s), _ = cnt.most_common(1)[0]
        cands = bysig.get(s, [])
        pick = None
        if len(cands) == 1: pick = cands[0]
        else:
            names = [n for (n, ss) in cnt]
            pool = cands or list(assets)
            for p in pool:
                if assets[p][0] in names: pick = p; break
        if pick: out[g] = os.path.relpath(pick, EX)
        else: print('unmatched', g, cnt.most_common(2), len(cands))
    os.makedirs(LOOKUPS, exist_ok=True)
    json.dump(out, open(os.path.join(LOOKUPS, 'fsm_templates.json'), 'w'), indent=1, sort_keys=True)
    print(len(out), 'of', len(uses), 'templates matched')
    return len(out), len(uses)

if __name__ == '__main__':
    main()
