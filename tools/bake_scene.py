#!/usr/bin/env python3
"""Bake a Hollow Knight scene's SpriteRenderers into parallax layers of 3DS tiles.

Usage: bake_scene.py <Scene.unity> <out_dir> <room_name> [--exclude name,name] [--ppu 16]

Output: <out_dir>/<room>.lyr  (binary, read by source/room.c)
        <out_dir>/<room>_preview.png
Also returns entity images for objects listed in ENTITY_ROOTS (baked separately).
"""
import sys, os, re, math, struct, json, functools, collections
import numpy as np
from PIL import Image, ImageChops
Image.MAX_IMAGE_PIXELS = None
sys.path.insert(0, os.path.dirname(__file__))
from hkcommon import *
from texpack import encode, FMT_RGBA4, FMT_RGBA8, FMT_RGB565, premul_resize
import grade as GR

CAM_D = 38.1          # HK camera distance from the gameplay plane (z=0)
TILE = 128           # layer tiles (the game unpacks them into a cache of 128x128 textures)
# parallax groups: (min factor, max factor, resolution multiplier). The foreground silhouettes are baked at full
# resolution: at half, their dark shading and highlight rims were lost and they came out as flat, jagged black blobs.
FRONT_RES = 1.0
GROUPS = [(0.0, 0.45, 0.5), (0.45, 0.6, 0.5), (0.6, 0.72, 0.5), (0.72, 0.82, 0.75), (0.82, 0.9, 0.75), (0.9, 0.9744, 1.0),
          (1.04, 1.1, 1.0), (1.1, 1.18, 1.0), (1.18, 1.3, 1.0), (1.3, 1.5, 1.0),
          (1.5, 1.75, FRONT_RES), (1.75, 2.1, FRONT_RES), (2.1, 2.6, FRONT_RES), (2.6, 3.4, FRONT_RES), (3.4, 6.0, 0.5)]   # near-camera silhouettes: finer bands so each keeps its own parallax (F > 6 is skipped)
PLANE_BACK = (0.9744, 1.0)   # merged into the gameplay plane behind the player (z < 1: the terrain tiles go behind it, TILE_SORT_Z)
PLANE_FRONT = (1.0, 1.04)  # merged into the gameplay plane in front of the player
# sprites in this sorting layer or above (glows, haze, lamp light) are drawn by Unity after everything in the lower
# sorting layers whatever their depth, over the foreground silhouettes too (which they tint): they go into 'over' layers
# of their own that the game draws last. Baked by depth among the rest, the silhouettes covered them as black blobs.
OVER_SORTING_LAYER = 13

# Soft art (fog, haze, glows, light shafts, darkeners: sprites with no hard edges) is baked apart from the sharp art,
# at half resolution and in 32-bit colour. Stored at 4 bits a channel a faint fog has one or two alpha steps: it came
# out banded, grainy from the dithering, and cut off by hard staircase edges where its alpha fell to zero (and the
# tiles where it was faintest were dropped). Half resolution costs nothing on art this smooth and keeps it small.
SOFT_NAMES = re.compile(r'fog|haze|mist|glow|beam|shaft|dark|fader|shade|shadow|vignette|cloud|blur|smoke|steam|gradient|ray|light(?!house|ning)', re.I)
SOFT_RES = 0.5
# Far layers (parallax below FAR_F) are stored the same way, 32-bit at half resolution: about the memory they took at 4
# bits and three-quarter resolution, without the dithering grain and banding. Behind the scene's BlurPlane the game blurs
# the background (LightBlurredBackground): those layers get a light blur too.
FAR_F = 0.82
FAR_BLUR = 1.1          # gaussian sigma in screen pixels
def blur_plane_z(objs_path):
    t = open(objs_path, errors='ignore').read()
    i = t.find('m_Name: BlurPlane')
    if i < 0: return None
    j = t.rfind('--- !u!1 &', 0, i); gid = re.search(r'&(\d+)', t[j:j + 30]).group(1)
    m = re.search(r'Transform:\n(?:.*\n){0,6}?  m_GameObject: \{fileID: ' + gid + r'\}\n(?:.*\n){0,4}?  m_LocalPosition: \{x: [-\d.e]+, y: [-\d.e]+, z: ([-\d.e]+)\}', t)
    return float(m.group(1)) if m else None
def gauss_blur(arr, sigma, sigma_x=None):
    """premultiplied float RGBA blur (separable); sigma_x: across, when the layer has more texels across (HD)"""
    if sigma <= 0.05: return arr
    out = arr
    for ax in (0, 1):
        sg = sigma if ax == 0 or sigma_x is None else sigma_x
        rad = int(math.ceil(sg * 3)); x = np.arange(-rad, rad + 1); k = np.exp(-x * x / (2 * sg * sg)); k /= k.sum()
        pad = [(0, 0)] * 3; pad[ax] = (rad, rad)
        p = np.pad(out, pad, mode='edge')
        acc = np.zeros_like(out)
        for i, w in enumerate(k):
            sl = [slice(None)] * 3; sl[ax] = slice(i, i + out.shape[ax]); acc += w * p[tuple(sl)]
        out = acc
    return out
_soft_cache = {}
def is_soft(it):
    """Additive or fog-like by name, and no hard alpha edges in the sprite (strong alpha steps per covered pixel)."""
    nm = it['path'].split('/')[-1]
    if not (it['add'] or SOFT_NAMES.search(nm)): return False
    rd = it['sprite'].get('m_RD') or {}
    key = (str((rd.get('texture') or {}).get('guid')), str(rd.get('textureRect')))
    if key not in _soft_cache:
        v = False
        r = sprite_image(it['sprite'])
        if r is not None:
            al = np.asarray(r[0].convert('RGBA'))[..., 3].astype(np.float32) / 255
            if al.size >= 16:
                strong = (np.abs(np.diff(al, axis=1)) > 0.25).sum() + (np.abs(np.diff(al, axis=0)) > 0.25).sum()
                v = 1000.0 * strong / max(1, (al > 0.02).sum()) < 10
        _soft_cache[key] = v
    return _soft_cache[key]

# objects whose sprites become separate entity images instead of being baked into the room
ENTITY_PREFIXES = ('Tute Door', 'Geo Rock', 'Chest', 'Health Cocoon', 'Collapser', 'Break Floor', 'Break Wall',
                   'Hallownest_Main_Gate', 'Mr Mushroom', 'Crawler', 'Buzzer', 'Stalactite', 'Cocoon Plant',
                   'Tut_tablet', 'Breakable', 'Shiny Item', 'Hazard Respawn', 'Hero', 'Knight', 'Vine Platform')
# (black_fader / darkeners gradients and mask_container covers are scenery: they're baked, or drawn as runtime masks
#  by masks.py. Only faders an FSM drives are left out, see collect)
SKIP_PREFIXES = ('grimm_tents', 'divine_tent', 'Grimm', 'Vignette Cutout', 'BlurPlane', 'Tutorial Blanker', 'Credits', 'ultrawide', 'Darkness',
                 'Area Title', 'Interact Reminder', 'Focus_prompt', 'Lighting')
FADER_PREFIXES = ('black_fader', 'white_fader')

def base(n): return re.sub(r'\s*\(\d+\)$', '', n)

class SceneT:
    def __init__(self, objs):
        self.objs = objs
        self.go = {k: v for k, (t, v) in objs.items() if t == 'GameObject'}
        self.tr = {k: v for k, (t, v) in objs.items() if t in ('Transform', 'RectTransform')}
        self.go2tr = {v['m_GameObject']['fileID']: k for k, v in self.tr.items()}
        self._w = {}
    def world(self, tid):
        if tid in self._w: return self._w[tid]
        t = self.tr[tid]
        F = lambda d: {k: f(v) for k, v in d.items()}
        p, s, q = F(t['m_LocalPosition']), F(t['m_LocalScale']), F(t['m_LocalRotation'])
        a = math.atan2(2 * (q['w'] * q['z'] + q['x'] * q['y']), 1 - 2 * (q['y'] ** 2 + q['z'] ** 2))
        # rotations around x/y (rare in 2D scenes) -> flip detection via sign of cos
        fx = 1.0
        if abs(q['y']) > 0.7 or abs(q['x']) > 0.7: fx = -1.0
        act = bool(self.go[t['m_GameObject']['fileID']].get('m_IsActive', 1))
        par = t['m_Father']['fileID']
        # the local 2D linear part (rotation, then scale) as a matrix: composing these is exact. Composing angle and
        # per-axis scales separately is only right when no parent with an unequal x / y scale is turned against its
        # child: under King's Pass's Secret Mask (turned 90 degrees, scaled 15 x 6.4) the child masks came out 73 units
        # wide instead of 31 and covered the opening shaft. Where the two agree the old values are kept as they were.
        ca0, sa0 = math.cos(a), math.sin(a)
        L = (ca0 * s['x'] * fx, -sa0 * s['y'], sa0 * s['x'] * fx, ca0 * s['y'])
        if par in self.tr:
            P = self.world(par)
            ca, sa = math.cos(P['a']), math.sin(P['a'])
            lx, ly = p['x'] * P['sx'], p['y'] * P['sy']
            r = dict(x=P['x'] + ca * lx - sa * ly, y=P['y'] + sa * lx + ca * ly, z=P['z'] + p['z'] * abs(P['sz']),
                     a=P['a'] + a, sx=P['sx'] * s['x'] * fx, sy=P['sy'] * s['y'], sz=P['sz'] * s['z'],
                     active=P['active'] and act, top=P['top'], names=P['names'] + [str(self.go[t['m_GameObject']['fileID']]['m_Name'])])
            PM = P['M']
            M = (PM[0] * L[0] + PM[1] * L[2], PM[0] * L[1] + PM[1] * L[3], PM[2] * L[0] + PM[3] * L[2], PM[2] * L[1] + PM[3] * L[3])
            tx, ty = P['tx'] + PM[0] * p['x'] + PM[1] * p['y'], P['ty'] + PM[2] * p['x'] + PM[3] * p['y']
            co, so = math.cos(r['a']), math.sin(r['a'])
            old = (co * r['sx'], -so * r['sy'], so * r['sx'], co * r['sy'])
            tol = 1e-4 * (1 + max(abs(v) for v in M))
            if max(abs(u - v) for u, v in zip(old, M)) > tol or abs(tx - r['x']) > 1e-3 or abs(ty - r['y']) > 1e-3:
                # the exact transform, as rotation then scale (a shear, from turning against an unequal scale at
                # an angle other than a right angle, is dropped)
                na = math.atan2(M[2], M[0]); nsx = math.hypot(M[0], M[2])
                nsy = -math.sin(na) * M[1] + math.cos(na) * M[3]
                r.update(x=tx, y=ty, a=na, sx=nsx, sy=nsy, fixed=True)
            r['M'] = M; r['tx'] = tx; r['ty'] = ty
        else:
            n = str(self.go[t['m_GameObject']['fileID']]['m_Name'])
            r = dict(x=p['x'], y=p['y'], z=p['z'], a=a, sx=s['x'] * fx, sy=s['y'], sz=s['z'], active=act, top=n, names=[n],
                     M=L, tx=p['x'], ty=p['y'])
        self._w[tid] = r
        return r

@functools.lru_cache(None)
def sprite_asset(guid):
    p = metamap('Sprite').get(guid)
    if not p: return None
    d = load_yaml_docs(p)
    for t, v in d.values():
        if t == 'Sprite': return v
    return None

@functools.lru_cache(maxsize=12)
def load_tex(path):
    im = Image.open(path)
    return im.convert('RGBA')

@functools.lru_cache(None)
def material_name(guid):
    p = metamap('Material').get(guid)
    return os.path.basename(p) if p else ''

# The game's own sprite material (Crossroads_Material, on nearly all scenery) and its swaying-grass variants are lit:
# Unity multiplies them by the scene's ambient light (SceneManager.defaultColor, SetLighting). Sprites-Default and the
# additive/glow materials aren't. Without it every area lost its colour cast (Greenpath came out lavender, not teal).
# Materials the export left out, identified by what uses them (haze, glows, light beams, caustics, flames, spell orbs,
# HeroLights, waterfall glows, flashes): the game's additive sprite materials. Guessing from sprite names missed sprites
# like Greenpath's far 'green_fog (10)', which then covered the room as a grey veil instead of adding a teal glow.
ADDITIVE_MATERIALS = {'3568007dc8719774da20a4b13d8cd951', '1a920692695ab0a45b364d712dd96861', 'd33d56ffcd4b00c41b11dc5430240cb5',
                      '8cea8082f6ba92e4fa89db2e362e1996', 'a468361d3fcde364a8cc47a9eb1f9b45', 'a386ca5fca315de40acd56e1966342e2',
                      'b22d93483c9302f4a9e2de49c757dbbf', '1092315befb296c4dbf62bcc08fa966d', '3aa61abb3b7f7aa43b5f556d35217e3f',
                      '7f89a24ce5bf04240bd1f8bb2192906b'}
LIT_SHADERS = {'af03469dd3490ec49adbc7cb6fbb95c4', 'd66f83432c026994cba4390ea7538143', 'aa384301be115094b97aec44438c90c3'}
# (the guids above are the repository's export's: make_lookups.py finds this export's own, issue #6)
ADDITIVE_MATERIALS = set(export_ids().get('additive_materials') or ADDITIVE_MATERIALS)
LIT_SHADERS = set(export_ids().get('lit_shaders') or LIT_SHADERS)
@functools.lru_cache(None)
def material_lit(guid):
    p = metamap('Material').get(guid)
    if not p or not os.path.exists(p): return False
    m = re.search(r'm_Shader: \{fileID: \d+, guid: (\w+)', open(p, errors='ignore').read())
    return bool(m) and m.group(1) in LIT_SHADERS

def scene_ambient(scene_path):
    """RenderSettings.ambientLight as SceneManager.SetLighting sets it: defaultColor * lerp(1, defaultIntensity, 0.5)"""
    t = open(scene_path, errors='ignore').read()
    i = t.find('\n  defaultColor:')
    if i < 0: return None
    m = re.search(r'defaultColor: \{r: ([-\d.e]+), g: ([-\d.e]+), b: ([-\d.e]+)', t[i:i + 200])
    k = re.search(r'defaultIntensity: ([-\d.e]+)', t[i:i + 300])
    if not m: return None
    mix = 1 + ((float(k.group(1)) if k else 1) - 1) * 0.5
    return tuple(float(m.group(j)) * mix for j in (1, 2, 3))

def sprite_image(sp):
    """Returns (PIL RGBA image of the full untrimmed sprite rect region that has pixels, offset_x, offset_y within rect, ppu, pivot px)."""
    rd = sp['m_RD']
    tg = rd['texture'].get('guid')
    tp = texture_path(tg)
    if not tp: return None
    tex = load_tex(tp)
    W, H = tex.size
    tr = rd.get('textureRect') or {}
    x, y, w, h = f(tr.get('x')), f(tr.get('y')), f(tr.get('width')), f(tr.get('height'))
    if w < 1 or h < 1: return None
    box = (int(math.floor(x)), int(H - math.ceil(y + h)), int(math.ceil(x + w)), int(H - math.floor(y)))
    im = tex.crop(box)
    sraw = int(rd.get('settingsRaw') or 0)
    packed = sraw & 1
    rot = (sraw >> 2) & 0xF
    if packed:
        if rot == 1: im = im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        elif rot == 2: im = im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        elif rot == 3: im = im.transpose(Image.Transpose.ROTATE_180)
        elif rot == 4: im = im.transpose(Image.Transpose.ROTATE_270)
    off = rd.get('textureRectOffset') or {'x': 0, 'y': 0}
    rect = sp['m_Rect']
    RW, RH = f(rect['width']), f(rect['height'])
    piv = sp.get('m_Pivot') or {'x': 0.5, 'y': 0.5}
    ppu = f(sp.get('m_PixelsToUnits') or 100)
    im = tight_mask(sp, im, f(off['x']), f(off['y']), f(piv['x']) * RW, f(piv['y']) * RH, ppu)
    return im, f(off['x']), f(off['y']), RW, RH, f(piv['x']) * RW, f(piv['y']) * RH, ppu

def sprite_mesh(sp):
    """The sprite's own mesh (Unity 'Tight' sprites): list of triangles [(x, y) * 3] in units around the pivot, or None."""
    rd = sp.get('m_RD') or {}
    vd = rd.get('m_VertexData') or {}
    nv = int(vd.get('m_VertexCount') or 0)
    hx = vd.get('_typelessdata'); ib = rd.get('m_IndexBuffer')
    if nv <= 4 or not hx or not ib: return None      # a plain quad covers the whole rect
    ch = vd.get('m_Channels') or []
    if not ch or int(ch[0].get('dimension') or 0) != 3 or int(ch[0].get('format') or 0) != 0 or int(ch[0].get('stream') or 0) != 0: return None
    try:
        d = bytes.fromhex(hx); idx = bytes.fromhex(ib)
        pts = [struct.unpack_from('<2f', d, i * 12) for i in range(nv)]
        ii = struct.unpack('<%dH' % (len(idx) // 2), idx)
    except Exception:
        return None
    if any(k >= nv for k in ii): return None
    return [(pts[ii[t]], pts[ii[t + 1]], pts[ii[t + 2]]) for t in range(0, len(ii) - 2, 3)]

def tight_mask(sp, im, offx, offy, pivx, pivy, ppu):
    """Atlases pack other sprites into a tight sprite's empty corners; Unity only draws the sprite's mesh, so clear
    the pixels outside it (otherwise neighbours bleed in, e.g. a black blob over the Crossroads hot spring)."""
    tris = sprite_mesh(sp)
    if not tris: return im
    from PIL import ImageDraw, ImageChops
    w, h = im.size
    m = Image.new('L', (w, h), 0); dr = ImageDraw.Draw(m)
    for t in tris:
        poly = [(x * ppu + pivx - offx, h - (y * ppu + pivy - offy)) for x, y in t]
        dr.polygon(poly, fill=255, outline=255)
    if m.getbbox() is None: return im      # mesh doesn't line up with the image: leave it alone
    im = im.copy(); a = im.getchannel('A'); im.putalpha(ImageChops.multiply(a, m))
    return im

MASKS_AT_RUNTIME = False
SKIP_PATHS = []        # object paths left out of the bake (secret masks, drawn and faded at runtime)

def collect(scene_path, exclude=()):
    objs = load_yaml_docs(scene_path)
    sc = SceneT(objs)
    fsm_go = {v['m_GameObject']['fileID'] for k, (t, v) in objs.items() if t == 'MonoBehaviour' and isinstance(v.get('fsm'), dict) and isinstance(v.get('m_GameObject'), dict)}
    def fsm_driven(tid):
        while tid in sc.tr:
            if sc.tr[tid]['m_GameObject']['fileID'] in fsm_go: return True
            tid = sc.tr[tid]['m_Father']['fileID']
        return False
    items = []
    for k, (t, v) in objs.items():
        if t != 'SpriteRenderer' or not v.get('m_Enabled', 1): continue
        gid = v['m_GameObject']['fileID']
        if gid not in sc.go2tr: continue
        W = sc.world(sc.go2tr[gid])
        if not W['active']: continue
        names = W['names']
        if any(base(n).startswith(SKIP_PREFIXES) for n in names): continue
        if any(base(n).startswith(FADER_PREFIXES) for n in names) and fsm_driven(sc.go2tr[gid]): continue   # faded by a script (a boss arena dimming, a prompt)
        if SKIP_PATHS:
            pth = '/'.join(names)
            if any(pth == q or pth.startswith(q + '/') for q in SKIP_PATHS): continue
        sg = (v.get('m_Sprite') or {}).get('guid')
        if not sg: continue
        sp = sprite_asset(sg)
        if not sp: continue
        if SKIP_PATHS is not None and str(sp.get('m_Name') or '').startswith('msk') and MASKS_AT_RUNTIME: continue   # secret masks (masks.py)
        col = v.get('m_Color') or {}
        rgba = (f(col.get('r', 1)), f(col.get('g', 1)), f(col.get('b', 1)), f(col.get('a', 1)))
        if rgba[3] <= 0.01: continue
        mats = v.get('m_Materials') or []
        mname = material_name(mats[0].get('guid')) if mats else ''
        if 'Darkness' in mname: continue
        additive = any(s in mname.lower() for s in ('additive', 'add', 'glow', 'light')) or (bool(mats) and mats[0].get('guid') in ADDITIVE_MATERIALS)
        if not mname:   # material not in the export: the game's light/haze sprites use additive materials
            spn = str(sp.get('m_Name') or '').lower()
            if spn.startswith(('light_effect', 'haze', 'glow', 'light_ray', 'lightray', 'light_beam', 'light_shaft', 'shaft')) or 'glow' in spn or 'caustic' in spn or 'beam' in spn or 'waterfall' in spn: additive = True   # black-backed light textures
        ent = None
        for n in names[:3]:
            if base(n).startswith(ENTITY_PREFIXES) or base(n) in exclude: ent = names[min(len(names) - 1, names.index(n))]; break
        if names[0] in ('_Enemies',) and len(names) > 1: ent = names[1]
        lit = bool(mats) and not additive and material_lit(mats[0].get('guid'))
        items.append(dict(sprite=sp, W=W, rgba=rgba, add=additive, lit=lit, flipx=int(v.get('m_FlipX') or 0),
                          flipy=int(v.get('m_FlipY') or 0), layer=int(v.get('m_SortingLayer') or 0),
                          order=int(v.get('m_SortingOrder') or 0), ent=ent, name=names[-1], path='/'.join(names),
                          fsmd=fsm_driven(sc.go2tr[gid])))
    return sc, items

# The terrain itself: the scene's tk2d TileMap renders every solid tile as a black square (its only tile, in the
# 'atlas0' tileset; 'TileMap Render Data' chunks, at z=0). That black is what fills the ground, walls and ceilings
# wherever no scenery covers them. Without it the background showed through the ground and the edge art floated in
# front of it like loose pieces. It sorts among the plane's sprites by distance to the camera (big chunks, so their
# centres are mostly further away than the edge art laid over them): drawn after art more than TILE_SORT_Z behind
# the plane, before the rest.
TILE_SORT_Z = 1.0
BAKE_TILEMAP = os.environ.get('BAKE_TILEMAP', '1') == '1'
def tilemap_item(sc):
    import hk_extract_room as X
    # named 'TileMap Render Data', or after its scene's template ('Town-TileMap Render Data', 'Template-...'): missing those
    # left Dirtmouth and 30-odd other rooms without their black ground
    if not any(t == 'GameObject' and 'TileMap Render Data' in str(v.get('m_Name', '')) for t, v in sc.objs.values()): return None
    try: W, H, grid = X.extract_tilemap(sc)
    except SystemExit: return None
    g = np.array(grid, bool)
    if not g.any(): return None
    ox = oy = 0.0
    for t, v in sc.objs.values():
        if t == 'MonoBehaviour' and 'spriteCollectionKey' in v and 'layers' in v:
            gid = v['m_GameObject']['fileID']
            if gid in sc.go2tr: w = sc.world(sc.go2tr[gid]); ox, oy = w['x'], w['y']
            break
    return dict(grid=g, sprite=None, W=dict(x=ox, y=oy, z=TILE_SORT_Z, a=0.0, sx=1.0, sy=1.0, names=['TileMap']), rgba=(0, 0, 0, 1),
                add=False, lit=False, flipx=0, flipy=0, layer=0, order=0, ent=None, name='TileMap', path='TileMap Render Data', fsmd=False)

# HD art (HK_XD=2): layers with twice the texels across, for the 800x240 top screen. Every sprite is rendered from its
# source texture straight at that width (not upscaled from the standard bake), so the extra columns are real detail
XD = int(os.environ.get('HK_XD', '1') or 1)

def render_grid(it, S, F_layer):
    g = it['grid']; H, W = g.shape; k = S * F_layer
    a = (g[::-1] * 255).astype(np.uint8)                           # image rows top-down
    al = Image.fromarray(a).resize((max(1, int(round(W * k * XD))), max(1, int(round(H * k)))), Image.NEAREST)
    img = Image.new('RGBA', al.size, (0, 0, 0, 0)); img.putalpha(al)
    ox, oy = it['W']['x'], it['W']['y']
    return img, ox * k * XD, -(oy + H) * k

def render_item(it, S, F_layer):
    """Render one sprite into (PIL image, left, top) in layer pixel space (y down from layer origin top)."""
    if it.get('grid') is not None: return render_grid(it, S, F_layer)
    r = sprite_image(it['sprite'])
    if r is None: return None
    im, ox, oy, RW, RH, px, py, ppu = r
    W = it['W']
    z = W['z']
    fpar = CAM_D / max(1.0, CAM_D + z)
    scale_world = 1.0 / ppu                    # texture px -> world units
    sx, sy = W['sx'] * (-1 if it['flipx'] else 1), W['sy'] * (-1 if it['flipy'] else 1)
    # local corners of the trimmed image in world units (relative to pivot), before rotation
    w_px, h_px = im.size
    x0 = (ox - px) * scale_world; y0 = (oy - py) * scale_world
    x1 = x0 + w_px * scale_world; y1 = y0 + h_px * scale_world
    # scale factor from texture px to layer px
    k = S * fpar * scale_world
    tw, th = max(1, int(round(w_px * k * abs(sx)))), max(1, int(round(h_px * k * abs(sy))))
    if tw * th > 6000 * 6000 or tw < 1 or th < 1: return None
    ang = W['a']; rot = abs(ang) > 1e-3
    # a huge rotated sprite in HD (a blurry foreground shape) is rotated at standard detail and then stretched across,
    # since rotating it square at XD took gigabytes (Deepnest_33's foreground was killed for memory)
    big = XD > 1 and rot and tw * th * XD * XD > 5000 * 5000
    if XD > 1 and not big:      # HD: XD times the columns from the source (a rotated sprite: square at XD, rotated, then squashed)
        dw, dh = tw * XD, (th * XD if rot else th)
        img = premul_resize(im, dw, dh) if (dw, dh) != im.size else im
    else:
        img = premul_resize(im, tw, th) if (tw, th) != im.size else im
    if sx < 0: img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if sy < 0: img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    # local rect after scale (world units, relative to anchor)
    lx0, lx1 = sorted((x0 * sx, x1 * sx)); ly0, ly1 = sorted((y0 * sy, y1 * sy))
    if rot:
        deg = math.degrees(ang)
        img = img.rotate(deg, resample=Image.BICUBIC, expand=True)
        if big: img = premul_resize(img, img.width * XD, img.height)
        elif XD > 1: img = premul_resize(img, img.width, max(1, int(round(img.height / XD))))
        # rotate rect center
        cxl, cyl = (lx0 + lx1) / 2, (ly0 + ly1) / 2
        ca, sa = math.cos(ang), math.sin(ang)
        rcx, rcy = cxl * ca - cyl * sa, cxl * sa + cyl * ca
    else:
        rcx, rcy = (lx0 + lx1) / 2, (ly0 + ly1) / 2
    # tint
    r_, g_, b_, a_ = it['rgba']
    if (r_, g_, b_, a_) != (1, 1, 1, 1):
        a8 = np.asarray(img); o8 = np.empty_like(a8); mul = np.array([r_, g_, b_, a_], np.float32)
        for r0 in range(0, a8.shape[0], 256):      # (in bands of rows: a huge HD sprite's float copy took gigabytes)
            o8[r0:r0 + 256] = np.clip(a8[r0:r0 + 256].astype(np.float32) * mul, 0, 255).astype(np.uint8)
        img = Image.fromarray(o8, 'RGBA')
    # anchor in layer space: world anchor * F_layer; local offsets scaled by fpar
    ax, ay = W['x'] * F_layer, W['y'] * F_layer
    cx = (ax + rcx * fpar) * S * XD
    cy = (ay + rcy * fpar) * S
    return img, cx - img.width / 2, -cy - img.height / 2   # y down (negative world y)

SPLIT_ADDITIVE = os.environ.get('BAKE_SPLIT_ADD', '1') == '1'

def composite(canvas_dict, key, img, left, top, additive, origin):
    """canvas_dict[key] -> numpy float premultiplied RGBA canvas; origin=(ox,oy) layer px of canvas[0,0]."""
    cv = canvas_dict[key]
    H, Wd = cv.shape[:2]
    x0 = int(round(left - origin[0])); y0 = int(round(top - origin[1]))
    a8 = np.asarray(img)
    h, w = a8.shape[:2]
    sx0, sy0 = max(0, -x0), max(0, -y0)
    dx0, dy0 = max(0, x0), max(0, y0)
    dx1, dy1 = min(Wd, x0 + w), min(H, y0 + h)
    if dx1 <= dx0 or dy1 <= dy0: return
    src = a8[sy0:sy0 + (dy1 - dy0), sx0:sx0 + (dx1 - dx0)].astype(np.float32) / 255.0      # (only the part on the canvas)
    al = src[..., 3:4]
    dst = cv[dy0:dy1, dx0:dx1]
    if additive:
        dst[..., :3] += src[..., :3] * al
        # additive glow doesn't add coverage on its own layer, but keep a little so it shows over empty areas
        dst[..., 3:4] = np.maximum(dst[..., 3:4], al * 0.6)
    else:
        dst[..., :3] = src[..., :3] * al + dst[..., :3] * (1 - al)
        dst[..., 3:4] = al + dst[..., 3:4] * (1 - al)

def bake(scene_path, out_dir, room, S=16, bounds=None, exclude=(), fmt=FMT_RGBA4, preview=True):
    sc, items = collect(scene_path, exclude)
    tm = tilemap_item(sc) if BAKE_TILEMAP else None
    if tm is not None: items.append(tm)
    grade = GR.scene_grade(scene_path)
    if GR.identity(grade): grade = None
    amb = scene_ambient(scene_path)
    if amb:
        for it in items:
            if it.get('lit'): it['rgba'] = (it['rgba'][0] * amb[0], it['rgba'][1] * amb[1], it['rgba'][2] * amb[2], it['rgba'][3])
    bz = blur_plane_z(scene_path)
    # sort: sorting layer, order, then far-to-near
    items.sort(key=lambda it: (it['layer'], it['order'], -it['W']['z']))
    if bounds is None: bounds = (0, 0, 210, 90)
    bx0, by0, bx1, by1 = bounds
    # bucket by parallax factor
    def fpar(z): return CAM_D / max(1.0, CAM_D + z)
    layers = collections.OrderedDict()
    ents = collections.defaultdict(list)
    for it in items:
        if it['ent']:
            ents[it['ent']].append(it); continue
        z = it['W']['z']
        fp = fpar(z)
        if abs(z) < 0.6: key = 1.0 if z >= 0 else 1.0001     # gameplay plane: behind / in front of the player
        elif PLANE_BACK[0] <= fp < PLANE_BACK[1]: key = 1.0
        elif PLANE_FRONT[0] <= fp < PLANE_FRONT[1]: key = 1.0001
        else:
            key = None
            for gi, (lo, hi, res) in enumerate(GROUPS):
                if lo <= fp < hi: key = (gi, res); break
            if key is None: continue
        if it['layer'] >= OVER_SORTING_LAYER: key = ('o', key)
        layers.setdefault(key, []).append(it)
    out = []
    def inner(k): return k[1] if isinstance(k, tuple) and k[0] == 'o' else k
    def kord(k):
        if inner(k) is not k: return 100 + kord(inner(k))      # over layers after all the others, far to near
        if k == 1.0: return 1.0
        if k == 1.0001: return 1.00005
        lo, hi, res = GROUPS[k[0]]
        return (lo + hi) / 2 + (0.0001 if lo >= 1.0 else 0)
    for key in sorted(layers, key=kord):
        ik = inner(key); over = ik is not key
        if ik in (1.0, 1.0001): F = 1.0; res = 1.0
        else:
            fs = [CAM_D / max(1.0, CAM_D + it['W']['z']) for it in layers[key]]
            F = float(np.median(fs)); res = GROUPS[ik[0]][2]
        far = ik not in (1.0, 1.0001) and GROUPS[ik[0]][0] < FAR_F
        if far: res = min(res, SOFT_RES)
        Sres = S * res
        # layer extents in layer px (world bounds * F, plus margin for screen at edges)
        # camera can see +-13 units around its center; the camera center lies within bounds
        margin = 14
        lx0 = (bx0 * F - margin) * Sres * XD; lx1 = (bx1 * F + margin) * Sres * XD
        ly0 = -(by1 * F + margin) * Sres; ly1 = -(by0 * F - margin) * Sres
        Wd = int(math.ceil((lx1 - lx0) / TILE)) * TILE
        Ht = int(math.ceil((ly1 - ly0) / TILE)) * TILE
        if Wd * Ht > 40_000_000 * XD:   # a near-camera group would need a gigantic canvas: skip it (it would fill the screen anyway)
            print(f'layer F={F:.3f} skipped (canvas {Wd}x{Ht})', flush=True); continue
        # additive sprites (glows, light shafts, haze) go to a layer of their own that the game draws with additive
        # blending; baked into the normal layer they turned into a grey veil over whatever was behind them. Soft
        # sprites (see SOFT_NAMES) go to soft layers: behind the band's sharp art or in front of it, by their depth.
        front = (ik == 1.0001) or (ik != 1.0 and GROUPS[ik[0]][0] >= 1.0)
        rs = min(res, SOFT_RES); Ss = S * rs
        sx0 = (bx0 * F - margin) * Ss * XD; sx1 = (bx1 * F + margin) * Ss * XD
        sy0 = -(by1 * F + margin) * Ss; sy1 = -(by0 * F - margin) * Ss
        sW = int(math.ceil((sx1 - sx0) / TILE)) * TILE; sH = int(math.ceil((sy1 - sy0) / TILE)) * TILE
        sharp = [it for it in layers[key] if not (SPLIT_ADDITIVE and it['add']) and not is_soft(it) and it.get('grid') is None]
        zmid = float(np.median([it['W']['z'] for it in sharp])) if sharp else 0.0
        canv = {0: np.zeros((Ht, Wd, 4), np.float32)}
        cans = {}      # 'tiles' the terrain, 'back' / 'front' soft (alpha), 'add' additive soft, 'addx' additive sharp
        cnt = collections.Counter()
        for it in layers[key]:
            if it.get('grid') is not None: kind = 'tiles'
            elif SPLIT_ADDITIVE and it['add']: kind = 'add' if is_soft(it) else 'addx'
            elif far: kind = 'sharp'
            elif is_soft(it): kind = 'back' if it['W']['z'] > zmid else 'front'
            else: kind = 'sharp'
            if kind in ('sharp', 'addx', 'tiles'):
                r = render_item(it, Sres, F); dims = (Ht, Wd); org = (lx0, ly0)
            else:
                r = render_item(it, Ss, F); dims = (sH, sW); org = (sx0, sy0)
            if r is None: continue
            img, left, top = r
            if kind == 'sharp': composite(canv, 0, img, left, top, False, org)
            else:
                if kind not in cans: cans[kind] = {0: np.zeros(dims + (4,), np.float32)}
                composite(cans[kind], 0, img, left, top, kind in ('add', 'addx'), org)
            cnt[kind] += 1
        def unpremul(cv):
            al = np.clip(cv[..., 3:4], 0, 1)
            rgb = np.where(al > 1e-4, cv[..., :3] / np.maximum(al, 1e-4), 0)
            return np.clip(rgb, 0, 1), al
        def graded(rgb):
            return GR.apply(rgb, grade) if grade is not None else rgb
        # in strips of rows: a whole near layer's temporaries (an HD foreground is 16000 px across) ran out of memory
        def by_rows(cv, f):
            out = np.empty(cv.shape[:2] + (4,), np.uint8)
            for r0 in range(0, cv.shape[0], 256): out[r0:r0 + 256] = f(cv[r0:r0 + 256])
            return out
        def alpha_layer(cv, **kw):
            def f(c):
                rgb, al = unpremul(c)
                return (np.concatenate([graded(rgb), al], -1) * 255 + 0.5).astype(np.uint8)
            return by_rows(cv, f)
        def add_layer(cv):
            def f(c):
                light = np.clip(c[..., :3], 0, 1)                  # premultiplied sum: what gets added to the screen
                if grade is not None: light = np.clip(GR.apply(light, grade) - GR.apply(np.zeros((1, 1, 3)), grade), 0, 1)
                on = (light.max(-1, keepdims=True) > 0.5 / 255.0).astype(np.float32)
                return (np.concatenate([light * on, on], -1) * 255 + 0.5).astype(np.uint8)
            return by_rows(cv, f)
        common = dict(F=F, front=front, over=over)
        # the terrain goes first, under the plane's haze as well: the soft layers are drawn apart from the sharp art,
        # and terrain baked into the sharp layer covered the haze the game draws over it, cutting glows off square
        if 'tiles' in cans: out.append(dict(common, x0=lx0, y0=ly0, img=alpha_layer(cans['tiles'][0]), count=1, res=res))
        if 'back' in cans: out.append(dict(common, x0=sx0, y0=sy0, img=alpha_layer(cans['back'][0]), count=cnt['back'], res=rs, soft=True))
        if cnt['sharp']:
            cv = canv[0]
            if far and bz is not None and float(np.median([it['W']['z'] for it in sharp] or [0])) > bz:
                cv = gauss_blur(cv, FAR_BLUR * res, FAR_BLUR * res * XD)      # sigma in layer px: screen px * (layer px per screen px)
            out.append(dict(common, x0=lx0, y0=ly0, img=alpha_layer(cv), count=cnt['sharp'], res=res, soft=far))
        if 'front' in cans: out.append(dict(common, x0=sx0, y0=sy0, img=alpha_layer(cans['front'][0]), count=cnt['front'], res=rs, soft=True))
        if 'addx' in cans: out.append(dict(common, x0=lx0, y0=ly0, img=add_layer(cans['addx'][0]), count=cnt['addx'], res=res, add=True, soft=True))
        if 'add' in cans: out.append(dict(common, x0=sx0, y0=sy0, img=add_layer(cans['add'][0]), count=cnt['add'], res=rs, add=True, soft=True))
        n, na = cnt['sharp'] + cnt['tiles'], cnt['add'] + cnt['addx']
        print(f'layer F={F:.3f} res={res} front={front}{" over" if over else ""} sprites={n} soft={cnt["back"]}+{cnt["front"]} additive={na} size={Wd}x{Ht}', flush=True)
        del canv, cans; cv = None
        for L in out:
            if 'tiles' not in L: L['tiles'], L['tx'], L['ty'], L['lf'] = encode_tiles(L['img'], fmt, L.get('soft'))
            if not preview: L['img'] = None
    write_layers(out_dir, room, out, S, fmt)
    if preview: write_preview(out_dir, room, out, S, bounds)
    return out, ents, sc

def dither4(arr):
    """Ordered dithering before 4-bit quantization to hide banding."""
    bayer = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) / 16.0 - 0.5
    h, w = arr.shape[:2]
    t = np.tile(bayer, (h // 4 + 1, w // 4 + 1))[:h, :w][..., None]
    a = arr.astype(np.float32) + t * 17.0
    return np.clip(a, 0, 255).astype(np.uint8)

def rle16(data):
    """RLE over 16-bit units: [u16 count|0x8000 for run][value] or [u16 count][literals...]."""
    v = np.frombuffer(data, '<u2')
    n = len(v); out = []
    # find run boundaries
    change = np.nonzero(np.diff(v))[0] + 1
    starts = np.concatenate([[0], change]); ends = np.concatenate([change, [n]])
    lit = []
    def flush_lit():
        i = 0
        while i < len(lit):
            chunk = lit[i:i + 0x7FFF]; out.append(np.array([len(chunk)], '<u2')); out.append(np.array(chunk, '<u2')); i += 0x7FFF
        lit.clear()
    for s_, e_ in zip(starts, ends):
        L = e_ - s_
        if L >= 3:
            flush_lit()
            while L > 0:
                c = min(L, 0x7FFF); out.append(np.array([0x8000 | c, v[s_]], '<u2')); L -= c
        else:
            lit.extend(v[s_:e_].tolist())
    flush_lit()
    return b''.join(o.tobytes() for o in out)

def encode_tiles(src, fmt, soft):
    """a layer image -> (tiles, tx, ty, format): each tile RLE-encoded, or one colour, or left out when empty"""
    lf = FMT_RGBA8 if soft else fmt          # soft art: 32-bit, no dithering
    H, W = src.shape[:2]
    tx, ty = W // TILE, H // TILE
    tiles = []
    for j in range(ty):
        band = src[j * TILE:(j + 1) * TILE]          # a row of tiles at a time (the dither's float copy of a whole layer was big)
        arr = dither4(band) if lf == FMT_RGBA4 else band
        for i in range(tx):
            t = arr[:, i * TILE:(i + 1) * TILE]
            # at 4 bits an alpha under 9/255 is 0: such a tile would be stored and drawn showing nothing
            if t[..., 3].max() < (1 if lf == FMT_RGBA8 else 9): continue
            # one colour, judged before dithering (a dark colour dithers between two 4-bit levels)
            t0 = src[j * TILE:(j + 1) * TILE, i * TILE:(i + 1) * TILE]
            mn, mx = t0.reshape(-1, 4).min(0), t0.reshape(-1, 4).max(0)
            if (mx - mn).max() <= 6 and mn[3] >= 250:
                c = ((mn.astype(int) + mx.astype(int)) // 2).astype(np.uint8)
                tiles.append((i, j, (int(c[0]), int(c[1]), int(c[2]), 255))); continue
            tiles.append((i, j, rle16(encode(Image.fromarray(t, 'RGBA'), lf))))
    return tiles, tx, ty, lf

def write_layers(out_dir, room, layers, S, fmt):
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, f'{room}.lyr')
    with open(path, 'wb') as fp:
        fp.write(b'HKLY')
        fp.write(struct.pack('<HHH', len(layers), TILE, S | (XD << 8 if XD > 1 else 0)))     # HD: density in the high byte
        for L in layers:
            if 'tiles' in L: tiles, tx, ty, lf = L['tiles'], L['tx'], L['ty'], L['lf']
            else: tiles, tx, ty, lf = encode_tiles(L['img'], fmt, L.get('soft'))
            fp.write(struct.pack('<ffffHHHBB', L['F'], L['x0'], L['y0'], L['res'], tx, ty, len(tiles), lf, (1 if L['front'] else 0) | (2 if L.get('add') else 0) | (4 if L.get('over') else 0)))
            for i, j, data in tiles:
                if isinstance(data, tuple):   # solid colour tile
                    fp.write(struct.pack('<HHI', i, j, 0)); fp.write(bytes(data))
                else:
                    fp.write(struct.pack('<HHI', i, j, len(data))); fp.write(data)
    print('wrote', path, os.path.getsize(path) // 1024, 'KB')

def write_preview(out_dir, room, layers, S, bounds, cam=None):
    bx0, by0, bx1, by1 = bounds
    W, H = int((bx1 - bx0) * S / 2), int((by1 - by0) * S / 2)
    pv = Image.new('RGBA', (W, H), (10, 10, 14, 255))
    # composite each layer as seen with camera = center of each region (approx: draw at F=1 positions)
    for L in layers:
        if L.get('add'): continue
        im = Image.fromarray(L['img'], 'RGBA')
        F = L['F']; R = L.get('res', 1.0)
        sc = 0.5 / F / R
        im2 = im.resize((max(1, int(im.width * sc)), max(1, int(im.height * sc))), Image.BILINEAR)
        ox = (L['x0'] / F / R) * 0.5 - bx0 * S / 2
        oy = (L['y0'] / F / R) * 0.5 + by1 * S / 2
        pv.alpha_composite(im2, (int(ox), int(oy))) if ox >= 0 and oy >= 0 else pv.paste(im2, (int(ox), int(oy)), im2)
    pv.save(os.path.join(out_dir, f'{room}_preview.png'))

if __name__ == '__main__':
    scene, out_dir, room = sys.argv[1:4]
    bake(scene, out_dir, room)
