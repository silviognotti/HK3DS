#!/usr/bin/env python3
"""The lookup tables that go by asset guids, made from your own export (issue #6).

AssetRipper gives every exported asset a new random guid each time it exports (scripts keep theirs: those come from the
class name). So a table keyed by asset guids fits only the export it was made from, and with any other one the music
cues, ambience, FSM templates, lit and additive materials and a few sprites silently resolved to nothing. This finds
them again by name, in the export at $HKDATA, and writes $HKDATA/lookups/:
  audio_map.json      {"music": {MusicCue guid: ogg}, "atmos": {AtmosCue guid: loop}}       build_room.py
  music_cues.txt      MusicCue guid -> ogg, for the cues the game's scripts apply (romfs/text) build.sh gamedata
  fsm_templates.json  PlayMaker template guid -> its asset (fsm_templates.py)                fsm_extract.py
  export_ids.json     lit shaders, additive materials, the vignette and HeroLight sprites, the Trajan fonts
The converters read these before the copies in tools/data (hkcommon.lookup_path / export_ids).
   python make_lookups.py          (after index_export.py: tools/build.sh runs it as the 'lookups' step)"""
import sys, os, re, glob, json, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkcommon import EX, LOOKUPS, TOOLS_DATA, metamap, load_yaml_docs

problems = []
def note(msg, bad=False):
    print(('WARNING: ' if bad else '') + msg, flush=True)
    if bad: problems.append(msg)


def scene_guid(k):          # a script guid as .asset / .unity files spell it (build_objs._scene_guid)
    w = [k[i:i + 8] for i in range(0, 32, 8)]
    br = lambda x: ''.join(reversed([x[i:i + 2] for i in range(0, 8, 2)]))
    return w[0] + w[1][4:] + w[1][:4] + br(w[2]) + br(w[3])

_SCRIPTS = json.load(open(os.path.join(TOOLS_DATA, 'script_guids.json')))
def script_guids(cls):
    return {g for k, v in _SCRIPTS.items() if v == cls for g in (k, scene_guid(k))}

SCRIPT_RE = re.compile(r'm_Script: \{fileID: -?\d+, guid: ([0-9a-f]{32})')
NAME_RE = re.compile(r'^  m_Name: ?(.*)$', re.M)
def assets_of(cls):
    """(guid, path, m_Name) of every MonoBehaviour asset of a script class"""
    want = script_guids(cls)
    for g, p in sorted(metamap('MonoBehaviour').items(), key=lambda x: x[1]):
        try: t = open(p, encoding='utf-8', errors='replace').read(4000)
        except OSError: continue
        m = SCRIPT_RE.search(t)
        if m and m.group(1) in want:
            n = NAME_RE.search(t)
            yield g, p, (n.group(1).strip() if n else os.path.splitext(os.path.basename(p))[0])


def norm(s): return re.sub(r'[^a-z0-9]', '', s.lower())


# ---- music: every MusicCue, named the way build_music.py names its ogg ----
def music():
    import build_music as M
    cues = M.cues()                             # asset file name -> channel clips (cues with channelInfos)
    out = {}
    for g, p, name in assets_of('MusicCue'):
        stem = os.path.splitext(os.path.basename(p))[0]
        chans = cues.get(stem)
        out[g] = M.cue_file(stem) if chans and M.pick(chans) and norm(name) != 'none' else 'none'
    note(f'music: {len(out)} cues, {sum(v == "none" for v in out.values())} of them silent')
    if not out: note('no MusicCue assets found (ex/MonoBehaviour): the rooms will have no music', True)
    return out


# ---- ambience: AtmosCues by name -> the AudioManager's atmos loops (build_music.ATMOS_LOOPS) ----
# matched against the repository's table by the reporter of issue #6 (it gives the same 22 cues the same loops);
# the other cues are named after their own loop
ATMOS_NAMED = {
    'cave': 'cave_wind_loop', 'surfacebasement': 'cave_wind_loop',
    'surface': 'dirtmouth_wind_loop_a', 'surfaceinterior': 'dirtmouth_wind_loop_a', 'windtunnel': 'dirtmouth_wind_loop_a',
    'rainyindoors': 'ruins_rain_indoor_loop', 'distantrain': 'ruins_rain_indoor_loop',
    'rainyoutdoors': 'ruins_rain_outdoor_loop',
    'greenpath': 'green_path_atmos_loop', 'greenpathinterior': 'green_path_atmos_loop', 'queensgardens': 'green_path_atmos_loop',
    'fungus': 'fungal_wastes_atmos_loop',       # the game's own name for the Fungal Wastes (Fungus2_ rooms), #6
    # the two the name matching got wrong in the first build from another export (#6): with these the 22 cues get
    # exactly the loops of the repository's table
    'miscwind': 'cave_atmos_misc_3', 'waterwaysflowing': 'waterway_waterfall_medium',
    'none': 'none', 'silent': 'none',
}
def atmos_loop(name, loops):
    n = norm(name)
    if n in ATMOS_NAMED: return ATMOS_NAMED[n]
    for l in loops:
        if n and n in norm(l): return l             # FungalWastes -> fungal_wastes_atmos_loop
    words = [w.lower() for w in re.findall(r'[A-Z]?[a-z]+|[0-9]+', name) if len(w) > 2]
    best, score = None, 0
    for l in loops:                                 # most letters of the cue's words in the loop's name
        s = sum(len(w) for w in words if w in norm(l))
        if s > score: best, score = l, s
    if not best:                                    # else a word sharing its first 4 letters with one of the loop's
        for l in loops:
            if any(w[:4] == lw[:4] for w in words if len(w) >= 4 for lw in re.findall(r'[a-z]+', l.lower())): return l
    return best

def atmos():
    import build_music as M
    out = {}
    for g, p, name in assets_of('AtmosCue'):
        l = atmos_loop(name, M.ATMOS_LOOPS)
        if l: out[g] = l; note(f'atmos: {name} -> {l}')
        else: note(f'atmos: {name}: no loop with that name; rooms with it keep the ambience that is playing', True)
    if not out: note('no AtmosCue assets found (ex/MonoBehaviour): the rooms will have no ambience', True)
    return out


# ---- shaders and materials ----
LIT_SHADER_NAMES = {'spriteslit', 'hollowknightgrassdiffuse', 'spritesdiffusebeating'}
def lit_shaders():
    out = []
    for g, p in metamap('Shader').items():
        names = {norm(os.path.splitext(os.path.basename(p))[0])}
        try:
            m = re.search(r'^\s*Shader\s+"([^"]+)"', open(p, encoding='utf-8', errors='replace').read(2000), re.M)
            if m: names.add(norm(m.group(1)))
        except OSError: pass
        if names & LIT_SHADER_NAMES: out.append(g)
    note(f'lit shaders: {len(out)} of 3')
    if len(out) < 3: note('lit shaders: not all three found (Sprites/Lit, Hollow Knight/Grass-Diffuse, Sprites/Diffuse-Beating) '
                          'in ex/Shader: areas may lose their colour cast', True)
    return sorted(out)

# materials SpriteRenderers in the scenes use that the export left out (not in ex/Material): the game's additive sprite
# materials are the ones that hardly anything but glows, light effects, beams, caustics, haze and waterfall glows uses
ADDITIVE_USERS = re.compile(r'light|glow|beam|caustic|waterfall|haze|fog|flash|flame|shine|ray|sun|bloom|spark|orb|lumin', re.I)
DOC_RE = re.compile(r'^--- !u!(\d+) &(-?\d+)[^\n]*$', re.M)
def additive_materials():
    have = set(metamap('Material'))
    users = collections.defaultdict(collections.Counter)
    scenes = sorted(glob.glob(f'{EX}/Scenes/**/*.unity', recursive=True))
    for sc in scenes:
        try: txt = open(sc, encoding='utf-8', errors='replace').read()
        except OSError: continue
        parts = DOC_RE.split(txt)
        names, mats = {}, []
        for i in range(1, len(parts) - 2, 3):
            t, fid, body = parts[i], parts[i + 1], parts[i + 2]
            if t == '1':
                m = NAME_RE.search(body)
                if m: names[fid] = m.group(1).strip()
            elif t == '212':          # SpriteRenderer
                go = re.search(r'm_GameObject: \{fileID: (-?\d+)\}', body)
                ml = re.search(r'm_Materials:\s*\n((?:\s*- \{[^\n]*\}\s*\n)+)', body)
                if go and ml: mats += [(go.group(1), g) for g in re.findall(r'guid: ([0-9a-f]{32})', ml.group(1))]
        for go, g in mats:
            if g in have or g.startswith('0000000000000000'): continue     # exported, or Unity's built-in ones
            users[g][re.sub(r' \(\d+\)$', '', names.get(go, '?'))] += 1
    out = []
    for g, c in sorted(users.items(), key=lambda x: -sum(x[1].values())):
        total = sum(c.values()); hits = sum(n for k, n in c.items() if ADDITIVE_USERS.search(k))
        add = hits >= 0.6 * total
        if add: out.append(g)
        note(f'material {g[:8]}: {total} sprites, {hits} glow/light-like ({", ".join(k for k, _ in c.most_common(4))}) '
             f'-> {"additive" if add else "normal"}')
    note(f'additive materials: {len(out)} (the repository\'s export had 10) from {len(scenes)} scenes')
    if not out: note('no additive sprite materials found: glows and light effects get baked as plain sprites', True)
    return out


# ---- sprites and fonts ----
def sprite_named(stem):
    for g, p in metamap('Sprite').items():
        if os.path.basename(p) == stem + '.asset': return g
    return None

def herolight():
    """the sprite of the Knight prefab's HeroLight object (haze2)"""
    for p in sorted(glob.glob(f'{EX}/GameObject/Knight*.prefab'), key=lambda x: (os.path.basename(x) != 'Knight_0.prefab', x)):
        try: docs = load_yaml_docs(p)
        except (OSError, UnicodeDecodeError): continue
        gos = {fid for fid, (t, v) in docs.items() if t == 'GameObject' and isinstance(v, dict) and v.get('m_Name') == 'HeroLight'}
        for fid, (t, v) in docs.items():
            if t == 'SpriteRenderer' and isinstance(v, dict) and int((v.get('m_GameObject') or {}).get('fileID', 0) or 0) in gos:
                g = (v.get('m_Sprite') or {}).get('guid')
                if g: note(f'HeroLight sprite: from {os.path.basename(p)}'); return g
    g = sprite_named('haze2')
    if g: note('HeroLight sprite: haze2 (no HeroLight in the Knight prefab)')
    return g

def trajan_fonts():
    out = [(g, n) for g, p, n in assets_of('TMP_FontAsset') if 'trajan' in n.lower()]
    note('Trajan fonts: ' + (', '.join(n for _, n in out) or 'none'), not out)
    return sorted(g for g, _ in out)


def main():
    if not os.path.isdir(f'{EX}/MonoBehaviour'): sys.exit(f'no export at {EX} (HKDATA: the folder that holds ex/)')
    os.makedirs(LOOKUPS, exist_ok=True)
    import fsm_templates
    nt, nu = fsm_templates.main()
    if nt < 0.8 * nu: note(f'FSM templates: only {nt} of {nu} matched', True)
    mus, atm = music(), atmos()
    json.dump({'music': mus, 'atmos': atm}, open(f'{LOOKUPS}/audio_map.json', 'w'), indent=1, sort_keys=True)
    open(f'{LOOKUPS}/music_cues.txt', 'w').write(''.join(f'{g}\t{n}\n' for g, n in sorted(mus.items())))
    ids = {'lit_shaders': lit_shaders(), 'additive_materials': additive_materials(),
           'vignette': sprite_named('vignette_large_v01'), 'herolight': herolight(), 'trajan_fonts': trajan_fonts()}
    if not ids['vignette']: note('vignette_large_v01: no such sprite in ex/Sprite', True)
    if not ids['herolight']: note('the HeroLight sprite (haze2): not found', True)
    json.dump(ids, open(f'{LOOKUPS}/export_ids.json', 'w'), indent=1, sort_keys=True)
    print(f'written to {LOOKUPS}')
    if problems:
        print(f'\n{len(problems)} warning(s):'); [print('  ' + p) for p in problems]


if __name__ == '__main__':
    main()
