#!/usr/bin/env python3
"""Unity ParticleSystems -> HK3DS particle emitters.

   python particles.py <romfs/rooms> Room [Room ...]

Writes <Room>.ptc (JSON: the room's scenery emitters, already placed in the world) and <Room>_ptc.spr (their
textures, small: particles are soft blobs drawn at a few dozen pixels). compile_ps() is also what objects use, so an
emitter on an enemy or a breakable has the same fields with its transform taken from the object at run time.

Every MinMaxCurve becomes [n, lo0..lo(n-1), hi0..hi(n-1)]: the value at time t (0..1) is lerp(lo(t), hi(t), random),
which covers constants, curves, two curves and two constants. Gradients the same with RGBA samples."""
import sys, os, json, math, functools
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkcommon import load_yaml_docs, metamap, texture_path, f
import bake_scene as B
from build_sprites import SpriteSet
from texpack import FMT_RGBA8

N = 8                   # samples per curve
TEX_MAX = 64            # particle textures: at most this many px a side (per sheet tile)
GRAV = -9.81            # Physics.gravity.y: particles use 3D gravity

# ---------------- curves ----------------
def curve_eval(c, t):
    keys = (c or {}).get('m_Curve') or []
    if not keys: return 0.0
    ks = [(f(k['time']), f(k['value']), f(k.get('inSlope', 0)), f(k.get('outSlope', 0))) for k in keys]
    if t <= ks[0][0]: return ks[0][1]
    if t >= ks[-1][0]: return ks[-1][1]
    for (t0, v0, _, o0), (t1, v1, i1, _) in zip(ks, ks[1:]):
        if t0 <= t <= t1:
            dt = t1 - t0
            if dt <= 1e-6: return v1
            if not (math.isfinite(o0) and math.isfinite(i1)): return v0      # stepped
            s = (t - t0) / dt; s2, s3 = s * s, s * s * s
            return (2 * s3 - 3 * s2 + 1) * v0 + (s3 - 2 * s2 + s) * o0 * dt + (-2 * s3 + 3 * s2) * v1 + (s3 - s2) * i1 * dt
    return ks[-1][1]

def mmc(c, k=1.0, default=0.0):
    """MinMaxCurve -> [n, lo..., hi...] (times k)."""
    if not isinstance(c, dict): return [1, default * k, default * k]
    st = int(f(c.get('minMaxState', 0))); s = f(c.get('scalar', default))
    if st == 0: return [1, s * k, s * k]
    if st == 3: return [1, f(c.get('minScalar', s)) * k, s * k]
    ts = [i / (N - 1) for i in range(N)]
    hi = [s * curve_eval(c.get('maxCurve'), t) * k for t in ts]
    lo = [s * curve_eval(c.get('minCurve'), t) * k for t in ts] if st == 2 else hi
    if max(hi + lo) - min(hi + lo) < 1e-6: return [1, hi[0], hi[0]]
    return [N] + [round(v, 4) for v in lo] + [round(v, 4) for v in hi]

def mmc_const(c, default=0.0):
    """A MinMaxCurve's typical value (for rates and delays): the mean over its range."""
    a = mmc(c, 1.0, default); n = a[0]
    return sum(a[1:]) / (2 * n)

def grad_eval(g, t):
    g = g or {}
    nc = int(f(g.get('m_NumColorKeys', 2))) or 1; na = int(f(g.get('m_NumAlphaKeys', 2))) or 1
    fixed = int(f(g.get('m_Mode', 0))) == 1
    ck = [(f(g.get(f'ctime{i}', 0)) / 65535, g.get(f'key{i}') or {}) for i in range(nc)]
    ak = [(f(g.get(f'atime{i}', 0)) / 65535, f((g.get(f'key{i}') or {}).get('a', 1))) for i in range(na)]
    def ev(keys, get):
        if t <= keys[0][0]: return get(keys[0][1])
        for (t0, a), (t1, b) in zip(keys, keys[1:]):
            if t0 <= t <= t1:
                if fixed: return get(b)
                s = (t - t0) / (t1 - t0) if t1 > t0 else 1
                va, vb = get(a), get(b)
                return [x + (y - x) * s for x, y in zip(va, vb)] if isinstance(va, list) else va + (vb - va) * s
        return get(keys[-1][1])
    rgb = ev(ck, lambda k: [f(k.get('r', 1)), f(k.get('g', 1)), f(k.get('b', 1))])
    a = ev(ak, lambda v: v)
    return rgb + [a]

def col(c):
    return [f(c.get('r', 1)), f(c.get('g', 1)), f(c.get('b', 1)), f(c.get('a', 1))] if isinstance(c, dict) else [1, 1, 1, 1]

def mmg(g):
    """MinMaxGradient -> [n, randT, lo rgba..., hi rgba...]. randT=1: evaluate at a random time (RandomColor mode)."""
    if not isinstance(g, dict): return [1, 0, 1, 1, 1, 1, 1, 1, 1, 1]
    st = int(f(g.get('minMaxState', 0)))
    r4 = lambda v: [round(x, 3) for x in v]
    if st == 0: c = col(g.get('maxColor')); return [1, 0] + r4(c) + r4(c)
    if st == 2: return [1, 0] + r4(col(g.get('minColor'))) + r4(col(g.get('maxColor')))
    ts = [i / (N - 1) for i in range(N)]
    hi = [grad_eval(g.get('maxGradient'), t) for t in ts]
    lo = [grad_eval(g.get('minGradient'), t) for t in ts] if st == 3 else hi
    out = [N, 1 if st == 4 else 0]
    for v in lo: out += r4(v)
    for v in hi: out += r4(v)
    return out

# ---------------- transforms ----------------
def quat_mat(q):
    x, y, z, w = f(q.get('x', 0)), f(q.get('y', 0)), f(q.get('z', 0)), f(q.get('w', 1))
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])

def euler_mat(e):        # Unity euler degrees, applied Z then X then Y
    ex, ey, ez = (math.radians(f(e.get(k, 0))) for k in ('x', 'y', 'z'))
    cx, sx, cy, sy, cz, sz = math.cos(ex), math.sin(ex), math.cos(ey), math.sin(ey), math.cos(ez), math.sin(ez)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]]); Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]); Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Ry @ Rx @ Rz

def trs(p, q, s):
    M = np.eye(4)
    M[:3, :3] = quat_mat(q) @ np.diag([f(s.get('x', 1)), f(s.get('y', 1)), f(s.get('z', 1))])
    M[:3, 3] = [f(p.get('x', 0)), f(p.get('y', 0)), f(p.get('z', 0))]
    return M

class Mats:
    """World matrices of a scene's (or prefab's) transforms."""
    def __init__(self, tr):
        self.tr = tr; self.m = {}
    def world(self, tid):
        if tid in self.m: return self.m[tid]
        t = self.tr[tid]
        L = trs(t['m_LocalPosition'], t['m_LocalRotation'], t['m_LocalScale'])
        par = t['m_Father']['fileID']
        if isinstance(par, str): par = int(par)       # build_objs.Doc keeps fileIDs as text: a parent lookup by text never matched
        M = self.world(par) @ L if par in self.tr else L
        self.m[tid] = M
        return M

# ---------------- materials / textures ----------------
ADDITIVE_BUILTIN = {200, 201, 202}       # Legacy Particles/Additive, ~Additive-Multiply, Additive (Soft)
TINTED_BUILTIN = {200, 203, 206}         # shaders that multiply by 2 * _TintColor

@functools.lru_cache(None)
def material(guid):
    """(texture guid, additive, tint rgba, name)"""
    p = metamap('Material').get(guid)
    if not p: return None
    d = load_yaml_docs(p)
    m = next((v for k, (t, v) in d.items() if t == 'Material'), None)
    if not m: return None
    name = str(m.get('m_Name') or '')
    sh = m.get('m_Shader') or {}
    fid = int(f(sh.get('fileID', 0)))
    props = m.get('m_SavedProperties') or {}
    tex = None
    te = props.get('m_TexEnvs') or {}
    if isinstance(te, list): te = {k: v for d2 in te for k, v in d2.items()}
    mt = (te.get('_MainTex') or {}).get('m_Texture') or {}
    tex = mt.get('guid')
    cols = props.get('m_Colors') or {}
    if isinstance(cols, list): cols = {k: v for d2 in cols for k, v in d2.items()}
    tint = [1, 1, 1, 1]
    if fid in TINTED_BUILTIN and '_TintColor' in cols: tint = [2 * x for x in col(cols['_TintColor'])]
    elif '_Color' in cols and fid not in ADDITIVE_BUILTIN: tint = col(cols['_Color'])
    add = fid in ADDITIVE_BUILTIN or (fid not in (203, 10753) and any(s in name.lower() for s in ('additive', '_add', ' add', 'glow')))
    return tex, add, tint, name

# Particle materials' textures aren't in the asset export; data/particle_textures.json maps each material (by name) to
# its texture's name, read from the game's .assets files (tools/unityfile.py), and the images are found by name
PARTICLE_TEXTURES = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    'data',
    'particle_textures.json',
)
MAT2TEX = (
    json.load(open(PARTICLE_TEXTURES))
    if os.path.isfile(PARTICLE_TEXTURES)
    else {}
)
TEXDIRS = [os.environ.get('HKDATA', 'hkdata') + '/ex/Texture2D', os.environ.get('HKDATA', 'hkdata') + '/hs/Texture2D']

@functools.lru_cache(None)
def texture_image(guid, mname=''):
    p = texture_path(guid) if guid else None
    if not p and mname in MAT2TEX:
        for d in TEXDIRS:
            if os.path.exists(f'{d}/{MAT2TEX[mname]}.png'): p = f'{d}/{MAT2TEX[mname]}.png'; break
    if not p: return None
    for ext in ('', '.png', '.PNG'):
        if os.path.exists(p + ext) and not os.path.isdir(p + ext):
            try: return Image.open(p + ext).convert('RGBA')
            except Exception: pass
    return None

def tile_images(img, tx, ty):
    w, h = img.size; tw, th = w // max(1, tx), h // max(1, ty)
    out = []
    for j in range(ty):
        for i in range(tx):
            t = img.crop((i * tw, j * th, (i + 1) * tw, (j + 1) * th))
            s = min(1.0, TEX_MAX / max(tw, th))
            if s < 1: t = t.resize((max(1, round(tw * s)), max(1, round(th * s))), Image.LANCZOS)
            out.append(t)
    return out

# ---------------- the system ----------------
def mod(ps, name):
    m = ps.get(name)
    return m if isinstance(m, dict) and int(f(m.get('enabled', 0))) else None

def compile_ps(ps, rend, sheet, texcache):
    """ParticleSystem + ParticleSystemRenderer YAML -> emitter dict (transform-free), or None when it can't draw."""
    if rend is not None and not int(f(rend.get('m_Enabled', 1))): return None
    mats = (rend or {}).get('m_Materials') or []
    mi = material(mats[0].get('guid')) if mats and isinstance(mats[0], dict) else None
    if not mi: return None
    texg, add, tint, mname = mi
    rmode = int(f((rend or {}).get('m_RenderMode', 0)))
    if rmode == 4: return None          # mesh particles
    init = ps.get('InitialModule') or {}
    uv = mod(ps, 'UVModule')
    tx, ty = (int(f(uv.get('tilesX', 1))) or 1, int(f(uv.get('tilesY', 1))) or 1) if uv and int(f(uv.get('mode', 0))) == 0 else (1, 1)
    key = (texg, mname, tx, ty)
    if key not in texcache:
        img = texture_image(texg, mname)
        if img is None: texcache[key] = None
        else:
            tiles = tile_images(img, tx, ty)
            base = None
            for t in tiles:
                fi = sheet.add_image(t, -(t.size[0] // 2), -(t.size[1] // 2))
                if base is None: base = fi
            texcache[key] = (base, len(tiles))
    tc = texcache[key]
    if tc is None: return None
    e = dict(dur=round(f(ps.get('lengthInSec', 5)), 3), loop=int(f(ps.get('looping', 1))), pre=int(f(ps.get('prewarm', 0))),
             poa=int(f(ps.get('playOnAwake', 1))), delay=round(mmc_const(ps.get('startDelay')), 3), sim=1 if int(f(ps.get('moveWithTransform', 0))) == 1 else 0,
             scm=int(f(ps.get('scalingMode', 0))),
             life=mmc(init.get('startLifetime'), 1, 5), spd=mmc(init.get('startSpeed'), 1, 5), size=mmc(init.get('startSize'), 1, 1),
             rot=mmc(init.get('startRotation'), 180 / math.pi), grav=mmc(init.get('gravityModifier')), col=mmg(init.get('startColor')),
             max=int(f(init.get('maxNumParticles', 1000))))
    em = mod(ps, 'EmissionModule')
    e['rate'] = mmc(em.get('rateOverTime'), 1) if em else [1, 0, 0]
    if em:
        bs = []
        for b in em.get('m_Bursts') or []:
            cc = mmc(b.get('countCurve'), 1, f(b.get('minCount', 0))) if 'countCurve' in b else [1, f(b.get('minCount', 0)), f(b.get('maxCount', 0))]
            bs.append([round(f(b.get('time', 0)), 3), round(cc[1], 1), round(cc[-1], 1), int(f(b.get('cycleCount', 1))), round(f(b.get('repeatInterval', 0.01)), 3)])
        if bs: e['bursts'] = bs
        if e['rate'][0] == 1 and e['rate'][1] == 0 and e['rate'][2] == 0 and not bs: return None
    else: return None
    sh = mod(ps, 'ShapeModule')
    if sh:
        rad = sh.get('radius'); r = f(rad.get('value', 1)) if isinstance(rad, dict) else f(rad if rad is not None else 1)
        arc = sh.get('arc'); a = f(arc.get('value', 360)) if isinstance(arc, dict) else f(arc if arc is not None else 360)
        e['shape'] = dict(t=int(f(sh.get('type', 0))), r=round(r, 3), rt=round(f(sh.get('radiusThickness', 1)), 3), ang=round(f(sh.get('angle', 25)), 2),
                          arc=round(a, 1), rd=round(f(sh.get('randomDirectionAmount', 0)), 3), sd=round(f(sh.get('sphericalDirectionAmount', 0)), 3),
                          len=round(f(sh.get('length', 5)), 3))
        e['_shape_trs'] = (sh.get('m_Position') or {}, sh.get('m_Rotation') or {}, sh.get('m_Scale') or {})
        if e['shape']['t'] in (6, 13, 14):      # mesh: the built-in Plane (10x10 in XZ, emits along +Y) or Quad (1x1 in XY)
            mid = int(f((sh.get('m_Mesh') or {}).get('fileID', 0)))
            e['shape'].update(t=5, box=[10, 0, 10] if mid == 10209 else [1, 1, 0] if mid == 10210 else [1, 1, 1], ax=1 if mid == 10209 else 2)
        elif e['shape']['t'] in (5, 15, 16, 18): e['shape'].update(t=5, box=[1, 1, 0 if e['shape']['t'] == 18 else 1], ax=2)
    else: e['shape'] = dict(t=-1)
    v = mod(ps, 'VelocityModule')
    if v: e['vel'] = dict(x=mmc(v.get('x')), y=mmc(v.get('y')), z=mmc(v.get('z')), w=int(f(v.get('inWorldSpace', 0))))
    fo = mod(ps, 'ForceModule')
    if fo: e['frc'] = dict(x=mmc(fo.get('x')), y=mmc(fo.get('y')), z=mmc(fo.get('z')), w=int(f(fo.get('inWorldSpace', 0))))
    cl = mod(ps, 'ClampVelocityModule')
    if cl: e['lim'] = dict(m=mmc(cl.get('magnitude'), 1, 1), d=round(f(cl.get('dampen', 1)), 3))
    c = mod(ps, 'ColorModule')
    if c: e['colt'] = mmg(c.get('gradient'))
    s = mod(ps, 'SizeModule')
    if s: e['szt'] = mmc(s.get('curve'), 1, 1)
    ro = mod(ps, 'RotationModule')
    if ro: e['rott'] = mmc(ro.get('curve'), 180 / math.pi)
    no = mod(ps, 'NoiseModule')
    if no: e['noise'] = dict(s=round(mmc_const(no.get('strength'), 1), 3), fq=round(f(no.get('frequency', 0.5)), 3), sc=round(mmc_const(no.get('scrollSpeed')), 3))
    if uv and tc[1] > 1:
        e['uv'] = dict(n=tc[1], fot=mmc(uv.get('frameOverTime'), 1), st=mmc(uv.get('startFrame')), cyc=round(f(uv.get('cycles', 1)), 3))
    e['ren'] = dict(m=1 if rmode == 1 else 0, ls=round(f((rend or {}).get('m_LengthScale', 2)), 3), vs=round(f((rend or {}).get('m_VelocityScale', 0)), 3),
                    add=1 if add else 0, fr=tc[0], tint=[round(x, 3) for x in tint], ord=int(f((rend or {}).get('m_SortingOrder', 0))),
                    sl=int(f((rend or {}).get('m_SortingLayer', 0))),
                    maxsz=round(f((rend or {}).get('m_MaxParticleSize', 0.5)), 3))
    return e

def place(e, M, Mlocal_scale=None):
    """Bake the emitter transform (world matrix M of its GameObject) into e: A maps shape space (x,y,z,1) to world xy,
    D maps shape directions to world xy, R maps the emitter's local space (velocity/force modules) to world xy."""
    p, r, s = e.pop('_shape_trs', ({}, {}, {}))
    S = np.eye(4); S[:3, :3] = euler_mat(r) @ np.diag([f(s.get('x', 1)) or 1, f(s.get('y', 1)) or 1, f(s.get('z', 1)) or 1]); S[:3, 3] = [f(p.get('x', 0)), f(p.get('y', 0)), f(p.get('z', 0))]
    Mw = M.copy()
    sc = np.linalg.norm(M[:3, :3], axis=0)
    if e['scm'] == 2: pass                      # Shape: scale applies to the shape only (already in M)
    W = Mw @ S
    e['A'] = [round(x, 4) for x in list(W[0, :]) + list(W[1, :])]
    Rn = M[:3, :3] / np.where(sc > 1e-6, sc, 1)
    Dm = (Rn @ euler_mat(r))
    e['D'] = [round(x, 4) for x in list(Dm[0, :]) + list(Dm[1, :])]
    e['R'] = [round(x, 4) for x in list(Rn[0, :]) + list(Rn[1, :])]
    e['ssz'] = round(float(np.mean(np.abs(sc[:2]))), 4) if e['scm'] == 0 else 1.0     # hierarchy scale on particle size
    e['pos'] = [round(float(M[0, 3]), 3), round(float(M[1, 3]), 3), round(float(M[2, 3]), 3)]
    return e

# ---------------- rooms ----------------
def scenery_emitters(room):
    """The room's particle systems that aren't part of a runtime object, playing on their own (playOnAwake)."""
    import build_room as BR
    sp = BR.scene_path(room)
    objs = load_yaml_docs(sp)
    sc = B.SceneT(objs)
    upath = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'uoroots', f'{room}.txt')
    ur = [l.strip() for l in open(upath) if l.strip()] if os.path.exists(upath) else []
    tr = {k: v for k, (t, v) in objs.items() if t in ('Transform', 'RectTransform')}
    mats = Mats(tr)
    rend = {v['m_GameObject']['fileID']: v for k, (t, v) in objs.items() if t == 'ParticleSystemRenderer'}
    out = []
    for k, (t, v) in objs.items():
        if t != 'ParticleSystem': continue
        gid = v['m_GameObject']['fileID']
        if gid not in sc.go2tr: continue
        W = sc.world(sc.go2tr[gid])
        if not W['active']: continue
        path = '/'.join(W['names'])
        if any(path == s or path.startswith(s + '/') for s in ur): continue      # the object plays it (or doesn't)
        if any(B.base(n).startswith(B.SKIP_PREFIXES) for n in W['names']): continue
        if not int(f(v.get('playOnAwake', 1))): continue
        out.append((path, v, rend.get(gid), mats.world(sc.go2tr[gid])))
    return out

def export(room, out_dir):
    sheet = SpriteSet(1.0); texcache = {}; ems = []
    for path, ps, rend, M in scenery_emitters(room):
        e = compile_ps(ps, rend, sheet, texcache)
        if not e: continue
        place(e, M)
        e['name'] = path.split('/')[-1][:40]
        ems.append(e)
    for ext in ('.ptc', '_ptc.spr'):
        if os.path.exists(f'{out_dir}/{room}{ext}'): os.remove(f'{out_dir}/{room}{ext}')
    if not ems: return 0
    sheet.write(f'{out_dir}/{room}_ptc.spr', FMT_RGBA8, 512)
    json.dump({'emitters': ems}, open(f'{out_dir}/{room}.ptc', 'w'), separators=(',', ':'))
    return len(ems)

# ---------------- the area particles that follow the camera ----------------
FOLLOW = os.environ.get('HKDATA', 'hkdata') + '/ex/GameObject/Knight Particles_follow.prefab'
def export_zones(out_dir):
    """SceneParticlesController (a child of the camera): one group of emitters per map zone, the default group when the
    zone has none. Emitter matrices are relative to the camera (x, y, z=0). Writes _zones.ptc + _zones_ptc.spr."""
    objs = load_yaml_docs(FOLLOW)
    go = {k: v for k, (t, v) in objs.items() if t == 'GameObject'}
    tr = {k: v for k, (t, v) in objs.items() if t == 'Transform'}
    g2t = {v['m_GameObject']['fileID']: k for k, v in tr.items()}
    rend = {v['m_GameObject']['fileID']: v for k, (t, v) in objs.items() if t == 'ParticleSystemRenderer'}
    ctrl = next(v for k, (t, v) in objs.items() if t == 'MonoBehaviour' and 'sceneParticles' in v)
    mats = Mats(tr)
    root = next(k for k, v in tr.items() if not v['m_Father']['fileID'])
    inv = np.linalg.inv(mats.world(root))
    def subtree(tid):
        out = [tid]
        for c in tr[tid].get('m_Children') or []: out += subtree(c['fileID'])
        return out
    sheet = SpriteSet(1.0); texcache = {}
    zones = {}
    groups = [(int(f(z['mapZone'])), z['particleObject']['fileID']) for z in ctrl['sceneParticles']] + [(-1, ctrl['defaultParticles']['particleObject']['fileID'])]
    for zone, gid in groups:
        ems = []
        for tid in subtree(g2t[gid]):
            g = tr[tid]['m_GameObject']['fileID']
            if tid != g2t[gid] and not int(f(go[g].get('m_IsActive', 1))): continue
            for k, (t, v) in objs.items():
                if t == 'ParticleSystem' and v['m_GameObject']['fileID'] == g:
                    e = compile_ps(v, rend.get(g), sheet, texcache)
                    if not e or not e['poa']: continue
                    place(e, inv @ mats.world(tid))
                    e['name'] = str(go[g]['m_Name'])[:40]
                    ems.append(e)
        zones[str(zone)] = ems
    sheet.write(f'{out_dir}/_zones_ptc.spr', FMT_RGBA8, 512)
    json.dump({'zones': zones}, open(f'{out_dir}/_zones.ptc', 'w'), separators=(',', ':'))
    return {k: len(v) for k, v in zones.items()}

def zone_table(rooms):
    """room -> the map zone whose area particles it shows (-2: none: noParticles)."""
    import re, build_room as BR
    out = {}
    for r in rooms:
        p = BR.scene_path(r)
        if not p: continue
        txt = open(p, errors='ignore').read()
        g = lambda k: (re.search(r'^  ' + k + r': (\d+)', txt, re.M) or [None, None])[1]
        if g('noParticles') == '1': out[r] = -2; continue
        ov = int(g('overrideParticlesWith') or 0)
        out[r] = ov if ov else int(g('mapZone') or 0)
    return out

if __name__ == '__main__':
    if sys.argv[1] == '--zones':        # particles.py --zones <romfs> : the area particles and the room -> zone table
        out = sys.argv[2]
        print(export_zones(out + '/rooms'))
        zt = zone_table(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'all_rooms.txt')).read().split())
        os.makedirs(out + '/text', exist_ok=True)      # a fresh romfs has no text/ yet (the game data comes later)
        open(out + '/text/ptczones.txt', 'w').write(''.join(f'{r} {z}\n' for r, z in sorted(zt.items())))
        print(len(zt), 'rooms in the zone table')
        sys.exit(0)
    out_dir = sys.argv[1]
    for room in sys.argv[2:]:
        print(room, export(room, out_dir), 'emitters', flush=True)
