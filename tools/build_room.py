#!/usr/bin/env python3
"""Build everything for rooms (scenes): collision+entities (.room), parallax art (.lyr), entity images (_ents.spr),
enemy sprite sheets (_enemies.spr).  Room ids are the game's scene names (Tutorial_01, Town, Crossroads_01 ...).

Usage: build_room.py <out_dir> <SceneName> [SceneName ...]
       build_room.py <out_dir> --bfs Tutorial_01 --max 20 --allow Tutorial_,Town,Crossroads_

HKRM v2 format:
  'HKRM' u16 version=2, u16 w, u16 h, u16 nents, f32 spawnx, f32 spawny, char music[32], char ambience[32], char title[32], char subtitle[32]
  solid bits: h rows (bottom-up) of (w+7)/8 bytes
  ents: u8 kind, u8 ai, u16 pad, f32 x, y, w, h, s16 hp, s16 geo, char name[32], char target[32], char extra[96]
"""
import sys, os, struct, json, re, math, glob, functools, collections
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import hk_extract_room as X
import bake_scene as B
from build_sprites import SpriteSet
from tk2d import load_library, collection_by_guid
from hkcommon import *

KIND = {k: i for i, k in enumerate(X.ALL_KINDS)}

# area -> (music, ambience, area title)
AREAS = [
    ('Room_Town_Stag_Station', 'dirtmouth', 'dirtmouth_wind_loop_a', ''),
    ('Room_shop', 'dirtmouth', '', ''), ('Room_Bretta', 'dirtmouth', '', ''), ('Room_mapper', 'dirtmouth', '', ''),
    ('Room_Charm_Shop', 'crossroads', '', ''), ('Room_Mender_House', 'crossroads', '', ''),
    ('Room_temple', 'crossroads', 'cave_wind_loop', ''), ('Room_Final_Boss_Atrium', '', 'cave_wind_loop', ''),
    ('Tutorial_', '', 'cave_wind_loop', "King's Pass"),
    ('Town', 'dirtmouth', 'dirtmouth_wind_loop_a', 'Dirtmouth'),
    ('Crossroads_', 'crossroads', 'cave_wind_loop', 'Forgotten Crossroads'),
    ('Fungus1_', 'greenpath', 'green_path_atmos_loop', 'Greenpath'),
    ('Fungus2_', 'fungal_wastes', 'fungal_wastes_atmos_loop', 'Fungal Wastes'),
    ('Cliffs_', '', 'cave_wind_loop', "Howling Cliffs"),
]
MUSIC_SRC = {'title': 'Title', 'dirtmouth': 'Dirtmouth 1', 'crossroads': 'S19 Crossroads Main', 'greenpath': 'S5 Green Path Main',
             'fungal_wastes': 'S25 Fungal Wastes MAIN', 'cave_wind_loop': 'cave_wind_loop', 'dirtmouth_wind_loop_a': 'dirtmouth_wind_loop_a',
             'green_path_atmos_loop': 'green_path_atmos_loop', 'fungal_wastes_atmos_loop': 'fungal_wastes_atmos_loop'}
NOBAKE = bool(os.environ.get('NOBAKE'))
SKIP_ENEMIES = ('Mender Bug', 'Grub', 'Hatcher Baby', 'Bench')

@functools.lru_cache(None)
def scene_path(name):
    for p in [f'{EX}/Scenes/{name}.unity'] + glob.glob(f'{EX}/*/{name}.unity') + glob.glob(f'{EX}/*/*/{name}.unity'):
        if os.path.exists(p): return p
    # deeper folders (AssetRipper 2 puts Godhome's boss scenes in Scenes/Gods_Glory/Bosses/), issue #6
    for p in glob.glob(f'{EX}/Scenes/**/{glob.escape(name)}.unity', recursive=True): return p
    return None

MAPZONES = ['NONE', 'TEST_AREA', 'KINGS_PASS', 'CLIFFS', 'TOWN', 'CROSSROADS', 'GREEN_PATH', 'ROYAL_GARDENS', 'FOG_CANYON', 'WASTES',
            'DEEPNEST', 'HIVE', 'BONE_FOREST', 'PALACE_GROUNDS', 'MINES', 'RESTING_GROUNDS', 'CITY', 'DREAM_WORLD', 'COLOSSEUM', 'ABYSS',
            'ROYAL_QUARTER', 'WHITE_PALACE', 'SHAMAN_TEMPLE', 'WATERWAYS', 'QUEENS_STATION', 'OUTSKIRTS', 'KINGS_STATION', 'MAGE_TOWER',
            'TRAM_UPPER', 'TRAM_LOWER', 'FINAL_BOSS', 'SOUL_SOCIETY', 'ACID_LAKE', 'NOEYES_TEMPLE', 'MONOMON_ARCHIVE', 'MANTIS_VILLAGE',
            'RUINED_TRAMWAY', 'DISTANT_VILLAGE', 'ABYSS_DEEP', 'ISMAS_GROVE', 'WYRMSKIN', 'LURIENS_TOWER', 'LOVE_TOWER', 'GLADE',
            'BLUE_LAKE', 'PEAK', 'JONI_GRAVE', 'OVERGROWN_MOUND', 'CRYSTAL_MOUND', 'BEASTS_DEN', 'GODS_GLORY', 'GODSEEKER_WASTE']
AUDIO_MAP = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'audio_map.json')))

def area_of(name):
    """(music cue, atmos loop, map zone) from the scene's SceneManager: '-' keeps what is playing, 'none' is silence."""
    p = scene_path(name)
    txt = open(p, errors='ignore').read() if p else ''
    def cue(k):
        m = re.search(r'^  ' + k + r': \{fileID: \d+(?:, guid: ([0-9a-f]+))?', txt, re.M)
        return m.group(1) if m and m.group(1) else None
    mg, ag = cue('musicCue'), cue('atmosCue')
    music = AUDIO_MAP['music'].get(mg, '-') if mg else '-'
    amb = AUDIO_MAP['atmos'].get(ag, '-') if ag else '-'
    z = re.search(r'^  mapZone: (\d+)', txt, re.M)
    zone = MAPZONES[int(z.group(1))] if z and int(z.group(1)) < len(MAPZONES) else ''
    return music, amb, zone

def transitions(sc):
    out = {}
    for k, (t, v) in sc.objs.items():
        if t == 'MonoBehaviour' and 'targetScene' in v:
            g = sc.go.get(v['m_GameObject']['fileID'])
            if g and not v.get('targetScene') and 'isADoor' in v:
                out.setdefault(str(g['m_Name']), ('', ''))       # doors enabled by script: keep them, link later
            if g and v.get('targetScene'):
                ts, ep = str(v['targetScene']), str(v.get('entryPoint') or '')
                # some doors have the two fields swapped (Crossroads_01 door1: targetScene 'bot1', entryPoint 'Town')
                if not scene_path(ts) and ep and scene_path(ep): ts, ep = ep, ts
                out[str(g['m_Name'])] = (ts, ep)
    return out

def enemies(sc, cols):
    """Every active object with a HealthManager becomes an enemy."""
    out = []
    comps = collections.defaultdict(list)
    for k, (t, v) in sc.objs.items():
        if isinstance(v, dict) and isinstance(v.get('m_GameObject'), dict): comps[v['m_GameObject']['fileID']].append((t, v))
    for gid, cl in comps.items():
        hm = next((v for t, v in cl if t == 'MonoBehaviour' and 'hp' in v and 'smallGeoDrops' in v), None)
        if not hm or gid not in sc.go2tr: continue
        g = sc.go[gid]; name = str(g['m_Name'])
        if not g.get('m_IsActive', 1) or X.base(name).startswith(SKIP_ENEMIES): continue
        # inactive parents
        t = sc.go2tr[gid]; act = True; anc = []
        while t in sc.tr:
            pg = sc.go[sc.tr[t]['m_GameObject']['fileID']]
            if not pg.get('m_IsActive', 1): act = False
            anc.append(str(pg['m_Name']))
            t = sc.tr[t]['m_Father']['fileID']
        if not act: continue
        W = sc.world(sc.go2tr[gid])
        # wave spawns for scripted arena fights ("Spitter Summon", "Hatcher Summon") stack up at one spawn spot inside
        # terrain until the battle releases them; battles aren't simulated yet, so leave them out
        if any('Summon' in n for n in anc[1:]): continue
        lib = next((v for t, v in cl if t == 'MonoBehaviour' and isinstance(v.get('library'), dict)), None)
        if not lib or not lib['library'].get('guid'): continue
        rb = next((v for t, v in cl if t == 'Rigidbody2D'), None)
        grav = float(rb.get('m_GravityScale', 1)) if rb else 0.0
        # the enemy's own collider (matched by object, not name: many enemies share a name like "Spitter")
        box = next((c['box'] for c in cols if c.get('gid') == gid and c['layer'] == 11 and not c['trigger']), None)
        if box is None: box = next((c['box'] for c in cols if c.get('gid') == gid and not c['trigger']), None)
        if box is None: box = next((c['box'] for c in cols if c.get('gid') == gid), None)
        if box is None: box = [W['x'] - 0.5, W['y'] - 0.5, W['x'] + 0.5, W['y'] + 0.5]
        geo = int(hm.get('smallGeoDrops') or 0) + 5 * int(hm.get('mediumGeoDrops') or 0) + 25 * int(hm.get('largeGeoDrops') or 0)
        hp = int(hm['hp'])
        if name.startswith('False Knight'): ai = 4
        elif name.startswith('Acid Flyer'): ai = 5; hp = 999; geo = 0     # Duranda: hovers in place, can't be killed, you pogo on it
        elif re.search(r'Fly|Buzzer|Bouncer|Spitter|Mosquito|Hatcher|Ghost|Jelly|Flyer|Bee|Bat|Squit|Moss Flyer|Lazy Flyer|Angry', name): ai = 1
        elif grav == 0 and not name.startswith(('Climber', 'Crawler')): ai = 3          # stationary (Baldurs, Mawlek...)
        elif re.search(r'Zombie|Husk|Barger|Hornhead|Leaper|Mosscreep|Mossman|Moss Knight|Hornet', name): ai = 2
        else: ai = 0
        if name == 'Head' or name.startswith('FK '): continue
        cx, cy = (box[0] + box[2]) / 2, (box[1] + box[3]) / 2
        out.append(dict(kind='enemy', name=name, x=cx, y=cy, w=box[2] - box[0], h=box[3] - box[1], hp=hp, geo=min(geo, 60),
                        ai=ai, lib=lib['library']['guid'], pox=W['x'] - cx, poy=W['y'] - cy))
    return out

ROLE_CANDIDATES = {
    'walk': ['Walk', 'walk', 'Run', 'Fly', 'Chase', 'Idle', 'idle'],
    'idle': ['Idle', 'idle', 'Fly', 'Walk', 'walk'],
    'chase': ['Chase', 'Run', 'Fly', 'Walk', 'walk'],
    'turn': ['Turn', 'turn', 'TurnToFly'],
    'dair': ['Death Air', 'Death', 'Death Air Old'],
    'dland': ['Death Land', 'Death Air', 'Death'],
    'attack': ['Attack Lunge', 'Attack', 'Startle'],
}

def entity_images(ent_items, S):
    ss = SpriteSet(1.0)
    for name, items in ent_items.items():
        rendered = []
        for it in sorted(items, key=lambda it: (it['layer'], it['order'], -it['W']['z'])):
            r = B.render_item(it, S, 1.0)
            if r: rendered.append((r, it['add']))
        if not rendered: continue
        x0 = min(l for (im, l, t), a in rendered); y0 = min(t for (im, l, t), a in rendered)
        x1 = max(l + im.width for (im, l, t), a in rendered); y1 = max(t + im.height for (im, l, t), a in rendered)
        w, h = int(math.ceil(x1 - x0)), int(math.ceil(y1 - y0))
        if w * h > 1024 * 1024 or w <= 0 or h <= 0: continue
        cv = {0: np.zeros((h, w, 4), np.float32)}
        for (im, l, t), add in rendered: B.composite(cv, 0, im, l, t, add, (x0, y0))
        a = cv[0]; al = np.clip(a[..., 3:4], 0, 1)
        rgb = np.where(al > 1e-4, a[..., :3] / np.maximum(al, 1e-4), 0)
        img = Image.fromarray((np.clip(np.concatenate([rgb, al], -1), 0, 1) * 255 + .5).astype(np.uint8), 'RGBA')
        bb = img.getbbox()
        if not bb: continue
        img = img.crop(bb); x0 += bb[0]; y0 += bb[1]
        if img.width > 1000 or img.height > 1000: continue
        ss.add_named(name, img, int(round(x0)), int(round(y0)))
    return ss

RES = 4   # collision cells per world unit

DYNAMIC = ('Tute Door', 'Geo Rock', 'Chest', 'Collapser', 'Break Floor', 'Break Wall', 'Breakable', 'Hallownest_Main_Gate',
           'Health Cocoon', 'Stalactite', 'Cocoon Plant', 'Mr Mushroom', 'Shiny', 'Hero', 'Knight', 'Elevator', 'Lift',
           # scripted blockers: open/close with fights, levers, tolls or story flags. Not simulated yet, so left open
           # (baking them in walled off doorways, e.g. the Raising Pillar in Crossroads_06).
           'Raising Pillar', 'Battle Gate', 'Battle Scene', 'Battle Music', 'Blocker', 'Terrain Block', 'Toll Gate',
           'Dream Gate', 'One Way Wall', 'Hatcher Cage', 'Grub Bottle', 'sliding_wall', 'Tram Door', 'Cage Closed',
           'Metal Gate', 'Fk Break Wall', 'Vine Platform',
           # walls switched on by story flags (playerdata_activation FSMs; e.g. the infected Crossroads)
           'full_wall_left', 'sliding_wall')

def fine_grid(W, H, grid, cols):
    from PIL import ImageDraw
    t = np.array(grid, bool)[::-1]                       # image rows top-down
    G = np.repeat(np.repeat(t, RES, 0), RES, 1)
    im = Image.fromarray((G * 255).astype(np.uint8), 'L')
    d = ImageDraw.Draw(im)
    HH = H * RES
    def P(p): return (p[0] * RES, HH - p[1] * RES)
    for c in cols:
        if c['layer'] != 8 or c['trigger'] or not c['active'] or not c.get('paths'): continue
        segs = c['path'].split('/')
        if any('TileMap' in s for s in segs) or any(X.base(s).startswith(('Roof Collider',) + DYNAMIC) for s in segs): continue
        if c['path'].startswith('_Transition'): continue
        for pth in c['paths']:
            if len(pth) < 2: continue
            if c['type'] == 'EdgeCollider2D': d.line([P(p) for p in pth], fill=255, width=2)
            elif len(pth) >= 3: d.polygon([P(p) for p in pth], fill=255)
    a = np.asarray(im) > 127
    return a[::-1]                                         # back to bottom-up rows

def polygon_rects(c, res=RES, maxn=200):
    """Cover a polygon/edge collider with axis-aligned boxes (thorn vines are long diagonal polygons; their
    bounding box would turn whole corridors into hazard). Returns [(x0, y0, x1, y1)] in world units."""
    from PIL import ImageDraw
    bx = c['box']
    for r in (res, res / 2):
        W = int(np.ceil((bx[2] - bx[0]) * r)) + 2; H = int(np.ceil((bx[3] - bx[1]) * r)) + 2
        im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
        def P(p): return ((p[0] - bx[0]) * r + 1, H - 1 - (p[1] - bx[1]) * r)
        for pth in c['paths'] or []:
            if c['type'] == 'EdgeCollider2D' or len(pth) < 3: d.line([P(p) for p in pth], fill=255, width=2)
            else: d.polygon([P(p) for p in pth], fill=255)
        a = np.asarray(im)[::-1] > 127              # row 0 = bottom
        runs = {}                                    # (x0, x1) -> [y0, y1] open rect
        rects = []
        for y in range(H):
            row = a[y]; cur = set(); x = 0
            while x < W:
                if row[x]:
                    x0 = x
                    while x < W and row[x]: x += 1
                    cur.add((x0, x))
                else: x += 1
            for k in list(runs):
                if k not in cur: rects.append((k[0], runs[k][0], k[1], runs[k][1])); del runs[k]
            for k in cur:
                if k in runs: runs[k][1] = y + 1
                else: runs[k] = [y, y + 1]
        rects += [(k[0], v[0], k[1], v[1]) for k, v in runs.items()]
        if len(rects) <= maxn or r == res / 2:
            return [(bx[0] + (x0 - 1) / r, bx[1] + y0 / r, bx[0] + (x1 - 1) / r, bx[1] + y1 / r) for x0, y0, x1, y1 in rects]

def split_spikes(ents, cols):
    out = []
    for e in ents:
        if e['kind'] != 'spikes': out.append(e); continue
        c = next((c for c in cols if c['layer'] == 17 and c['type'] in ('PolygonCollider2D', 'EdgeCollider2D') and
                  abs((c['box'][0] + c['box'][2]) / 2 - e['x']) < 1e-3 and abs((c['box'][1] + c['box'][3]) / 2 - e['y']) < 1e-3 and
                  abs(c['box'][2] - c['box'][0] - e['w']) < 1e-3), None)
        if not c: out.append(e); continue
        for x0, y0, x1, y1 in polygon_rects(c):
            out.append(dict(e, x=(x0 + x1) / 2, y=(y0 + y1) / 2, w=x1 - x0, h=y1 - y0))
    return out

def respawn_links(sc):
    """GameObject id -> (x, y) of the HazardRespawnMarker its component points at (hazard triggers and gates)."""
    out = {}
    for fid, (t, v) in sc.objs.items():
        if t != 'MonoBehaviour' or 'respawnMarker' not in v: continue
        ref = (v.get('respawnMarker') or {}).get('fileID')
        o = sc.objs.get(ref)
        if not o: continue
        mg = o[1].get('m_GameObject', {}).get('fileID') if o[0] == 'MonoBehaviour' else (ref if ref in sc.go else None)
        if mg in sc.go2tr:
            W = sc.world(sc.go2tr[mg]); out[v['m_GameObject']['fileID']] = (W['x'], W['y'])
    return out

def gate_infos(sc):
    """TransitionPoint data by gate name: entry offset and flags (1 always enter right, 2 left, 4 hard land, 8 don't walk out)."""
    out = {}
    for fid, (t, v) in sc.objs.items():
        if t != 'MonoBehaviour' or 'entryOffset' not in v: continue
        g = sc.go.get(v['m_GameObject']['fileID'])
        if not g: continue
        eo = v.get('entryOffset') or {}
        fl = (1 if int(v.get('alwaysEnterRight', 0) or 0) else 0) | (2 if int(v.get('alwaysEnterLeft', 0) or 0) else 0) | \
             (4 if int(v.get('hardLandOnExit', 0) or 0) else 0) | (8 if int(v.get('dontWalkOutOfDoor', 0) or 0) else 0)
        out[str(g['m_Name'])] = (float(eo.get('x', 0)), float(eo.get('y', 0)), fl)
    return out


def build(room, out_dir, S=16, wanted=None):
    scene = scene_path(room)
    if not scene: print('no scene', room); return None
    sc = X.Scene(X.load_scene(scene))
    try: W, H, grid = X.extract_tilemap(sc)
    except SystemExit: print('no tilemap', room); return None
    cols = X.colliders(sc)
    # objects the runtime runs (tools/build_objs.py -> data/uoroots/<room>.txt) bring their own colliders and behaviour:
    # leave them out of the static collision grid and of the hand-made entity kinds (gates and respawn points stay)
    ur = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'uoroots', f'{room}.txt')
    uroots = [l.strip() for l in open(ur) if l.strip()] if os.path.exists(ur) else []
    def under(p): return any(p == r or p.startswith(r + '/') for r in uroots)
    cols_static = [c for c in cols if not under(c['path'])]
    KEEP = ('gate', 'hazard_respawn', 'hazard_respawn_marker', 'respawn_marker')
    ents = [e for e in X.classify(sc, cols) if e['kind'] in KEEP] + \
           [e for e in X.classify(sc, cols_static) if e['kind'] not in KEEP + ('enemy', 'solid', 'platform')]
    ents = split_spikes(ents, cols)
    fg = fine_grid(W, H, grid, cols_static)
    trans = transitions(sc)
    have = {e['name'] for e in ents if e['kind'] == 'gate'}
    for gname in trans:
        if gname in have: continue
        c = next((c for c in cols if str(c['name']) == gname and c['trigger']), None) or next((c for c in cols if str(c['name']) == gname), None)
        if not c: continue
        bx = c['box']
        # disabled exits (e.g. Crossroads_01 top1) are kept as arrival points only: they never trigger a transition
        ents.append(dict(kind='gate', name=gname, x=(bx[0] + bx[2]) / 2, y=(bx[1] + bx[3]) / 2, w=bx[2] - bx[0], h=bx[3] - bx[1],
                         noexit=not c['active']))
    links = respawn_links(sc)
    ginfo = gate_infos(sc)
    def link_for(e, layer_ok=lambda c: True):
        for c in cols:
            if not layer_ok(c): continue
            b = c['box']
            if abs((b[0] + b[2]) / 2 - e['x']) < 1e-3 and abs((b[1] + b[3]) / 2 - e['y']) < 1e-3 and c['gid'] in links: return links[c['gid']]
        g = next((gid for gid, go in sc.go.items() if str(go['m_Name']) == e['name'] and gid in links), None)
        return links.get(g)
    for e in ents:
        e['hp'] = 0; e['geo'] = 0; e['target'] = ''; e['ai'] = 0; e['extra'] = ''
        if e['kind'] in ('hazard_respawn', 'gate'):
            m = link_for(e)
            if m: e['extra'] = 'R %.2f %.2f' % m       # where HK puts you after a hazard (HazardRespawnMarker)
        if e['kind'] == 'gate' and e['name'] in ginfo:
            ox, oy, fl = ginfo[e['name']]
            if not e['extra']: e['extra'] = 'R %.2f %.2f' % (e['x'], e['y'])
            e['extra'] += ' O %.2f %.2f F %d' % (ox, oy, fl)   # TransitionPoint entryOffset + flags
        if e['kind'] == 'vine':
            # drop distance from the game's own landing collider; vine hit box from the vine sprite's collider
            land = next((c for c in cols if c['path'] == e['name'] + '/Activated Plat' and c['layer'] == 8), None)
            vine = next((c for c in cols if c['path'].startswith(e['name'] + '/') and c['layer'] == 19), None)
            top = e['y'] + e['h'] / 2
            e['target'] = '%.3f' % ((top - land['box'][3]) if land else 0.0)
            vb = vine['box'] if vine else [e['x'] - 0.3, top, e['x'] + 0.3, top + 5]
            e['extra'] = '%.2f %.2f %.2f %.2f' % tuple(vb)
        if e['kind'] == 'breakable_wall': e['hp'] = 3
        elif e['kind'] == 'geo_rock': e['hp'] = 5; e['geo'] = 5
        elif e['kind'] == 'chest': e['hp'] = 1; e['geo'] = 30
        elif e['kind'] == 'collapse_floor': e['hp'] = 3
        elif e['kind'] == 'gate':
            t = trans.get(e['name'])
            if t and t[0] and not e.get('noexit') and (wanted is None or t[0] in wanted): e['target'] = f'{t[0]}:{t[1]}'
    # benches (rest + save)
    for gid, g in sc.go.items():
        if str(g['m_Name']).startswith('RestBench') and gid in sc.go2tr and g.get('m_IsActive', 1):
            Wb = sc.world(sc.go2tr[gid])
            ents.append(dict(kind='bench', name=str(g['m_Name']), x=Wb['x'], y=Wb['y'], w=2.0, h=1.5, hp=0, geo=0, target='', ai=0, extra=''))
    ens = enemies(sc, cols)
    # boss scenes (<room>_boss) load additively at runtime as live objects (tools/additive.py, build_objs.py)
    es = SpriteSet(0.25)
    libkeys = {}
    for en in ens:
        lp = metamap('GameObject').get(en['lib'])
        if not lp: continue
        key = os.path.splitext(os.path.basename(lp))[0][:20]
        if key not in libkeys:
            lib = load_library(lp)
            if not lib: continue
            roles = {role: next((c for c in cands if c in lib), '') for role, cands in ROLE_CANDIDATES.items()}
            clips = set(v for v in roles.values() if v)
            if en['ai'] == 4: clips = set(k for k in lib if k)       # bosses use their whole move set
            if not clips: continue
            try: es.add_library(lp, clips, key + ':')
            except Exception as ex: print('  enemy lib failed', key, ex); continue
            libkeys[key] = roles
        if key not in libkeys: continue
        roles = libkeys[key]
        en['target'] = '%.3f %.3f' % (en['pox'], en['poy'])
        en['extra'] = key + '|' + '|'.join(roles[r] for r in ('walk', 'idle', 'chase', 'turn', 'dair', 'dland', 'attack'))
        ents.append(en)
    if NOBAKE:
        if es.frames: es.write(f'{out_dir}/{room}_enemies.spr')
    else:
        if es.frames: es.write(f'{out_dir}/{room}_enemies.spr')
        import masks as MK
        B.SKIP_PATHS = [p for g, p, pers in MK.mask_roots(scene)]; B.MASKS_AT_RUNTIME = True
        # objects the runtime draws itself (enemies, NPCs, interactables: tools/build_objs.py writes data/uoroots/<room>.txt)
        ur = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'uoroots', f'{room}.txt')
        if os.path.exists(ur): B.SKIP_PATHS += [l.strip() for l in open(ur) if l.strip()]
        out, ent_items, _ = B.bake(scene, out_dir, room, S=S, bounds=(0, 0, W, H), preview=True)
        B.SKIP_PATHS = []; B.MASKS_AT_RUNTIME = False
        ss = entity_images(ent_items, S)
        if ss.frames: ss.write(f'{out_dir}/{room}_ents.spr')
    music, amb, title = area_of(room)
    sp = (34.5, 78.0) if room == 'Tutorial_01' else None
    if sp is None:
        g = [e for e in ents if e['kind'] == 'gate']
        sp = (g[0]['x'], g[0]['y']) if g else (W / 2, H / 2)
    with open(f'{out_dir}/{room}.room', 'wb') as fp:
        fp.write(b'HKRM')
        fp.write(struct.pack('<HHHHff', 3, W, H, len(ents), sp[0], sp[1]))
        for s in (music, amb, title, ''):
            fp.write(struct.pack('32s', s.encode()[:31]))
        fp.write(struct.pack('<HH', RES, 0))
        fp.write(np.packbits(fg, axis=1, bitorder='little').tobytes())
        for e in ents:
            fp.write(struct.pack('<BBHffffhh32s32s96s', KIND[e['kind']], e.get('ai', 0), 0, e['x'], e['y'], e['w'], e['h'],
                                 e['hp'], e['geo'], e['name'].encode()[:31], e['target'].encode()[:31], e['extra'].encode()[:95]))
    json.dump(dict(w=W, h=H, ents=ents, transitions=trans), open(f'{out_dir}/{room}.json', 'w'), indent=1, default=str)
    print(room, W, H, dict(collections.Counter(e['kind'] for e in ents)), 'enemy libs:', list(libkeys), flush=True)
    return trans

def bfs(start, maxn, allow):
    order, seen, q = [], {start}, [start]
    while q and len(order) < maxn:
        r = q.pop(0)
        p = scene_path(r)
        if not p: continue
        order.append(r)
        sc = X.Scene(X.load_scene(p))
        for gname, (tgt, entry) in sorted(transitions(sc).items()):
            if tgt not in seen and any(tgt.startswith(a) for a in allow) and scene_path(tgt):
                seen.add(tgt); q.append(tgt)
    return order

if __name__ == '__main__':
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    args = sys.argv[2:]
    if args and args[0] == '--bfs':
        start = args[1]; maxn = int(args[args.index('--max') + 1]) if '--max' in args else 20
        allow = args[args.index('--allow') + 1].split(',') if '--allow' in args else ['Tutorial_', 'Town', 'Crossroads_']
        rooms = bfs(start, maxn, allow)
        print('rooms:', rooms, flush=True)
        json.dump(rooms, open(f'{out}/rooms.json', 'w'))
    else:
        rooms = args
    wanted = set(json.load(open(f'{out}/rooms.json'))) if os.path.exists(f'{out}/rooms.json') else set(rooms)
    wanted |= set(rooms)
    skip_existing = os.environ.get('SKIP_EXISTING')
    for r in rooms:
        if skip_existing and os.path.exists(f'{out}/{r}.room'): continue
        try: build(r, out, wanted=wanted)
        except Exception as ex:
            import traceback; traceback.print_exc(); print('FAILED', r, ex, flush=True)
