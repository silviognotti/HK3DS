# Export a room's live objects (enemies first) as a small Unity-like object graph for the 3DS runtime:
# GameObjects with transforms, colliders, rigidbodies, sprite animators, the game's MonoBehaviours (identified from
# the decompiled sources) and decoded PlayMaker FSMs. Prefabs those objects spawn (shockwaves, spit, corpses...) are
# exported as templates.
#   python build_objs.py <out_dir> Room1 [Room2 ...]   -> <out_dir>/<Room>.obj (JSON), <out_dir>/<Room>.objlibs (json)
import sys, os, re, json, math, glob, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import cs_sigs
from hkcommon import metamap, EX, load_yaml_docs, export_ids
import build_room as BR

SIGS = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'cs_sigs.json')))
# MonoBehaviours with no serialized fields can't be identified by signature; known script guids:
KNOWN_GUIDS = {'e667676c': 'AlertRange'}
# AssetRipper names a script's guid md5(assembly + namespace + class) with each 32-bit word byte-swapped
SCRIPT_GUIDS = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'script_guids.json')))
# the scenes' m_Script guids order the md5 words differently from the table: word 1 has its 16-bit halves swapped,
# words 2 and 3 are byte-reversed (checked against every MonoBehaviour in the scenes: 402 of 405 classes match)
def _scene_guid(k):
    w = [k[i:i + 8] for i in range(0, 32, 8)]
    br = lambda x: ''.join(reversed([x[i:i + 2] for i in range(0, 8, 2)]))
    return w[0] + w[1][4:] + w[1][:4] + br(w[2]) + br(w[3])
SCRIPT_GUIDS.update({_scene_guid(k): v for k, v in list(SCRIPT_GUIDS.items())})
TK2D_SPRITE_KEYS = {'collection', '_spriteId'}
TK2D_ANIM_KEYS = {'library', 'defaultClipId'}

def num(x):
    try:
        f = float(x)
        return int(f) if f == int(f) and 'e' not in str(x).lower() and '.' not in str(x) else f
    except Exception:
        return x

def quat_z(q):
    x, y, z, w = (float(q.get(k, 0)) for k in 'xyzw')
    return math.degrees(math.atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)))

class Doc:
    """One scene or prefab file."""
    def __init__(self, path):
        self.path = path
        self.objs = F.load_objs(path)
        self.go = {k: v for k, (t, v) in self.objs.items() if t == 'GameObject'}
        self.tr = {k: v for k, (t, v) in self.objs.items() if t in ('Transform', 'RectTransform')}
        self.go2tr = {int(v['m_GameObject']['fileID']): k for k, v in self.tr.items()}
        self.children = collections.defaultdict(list)
        for k, v in self.tr.items():
            self.children[int(v['m_Father']['fileID'])].append(k)
        for k in self.children:   # keep the transform's own child order
            pass
        self.comps = collections.defaultdict(list)
        for k, (t, v) in self.objs.items():
            if isinstance(v, dict) and isinstance(v.get('m_GameObject'), dict) and t not in ('Transform', 'RectTransform'):
                self.comps[int(v['m_GameObject']['fileID'])].append((k, t, v))
        self.P = F.Paths(self.objs)
    def kids(self, gid):
        t = self.go2tr.get(gid)
        order = [int(c['fileID']) for c in (self.tr[t].get('m_Children') or [])] if t in self.tr else []
        if not order: order = self.children.get(t, [])
        return [int(self.tr[c]['m_GameObject']['fileID']) for c in order if c in self.tr]
    def parent(self, gid):
        t = self.go2tr.get(gid)
        if t is None: return None
        p = int(self.tr[t]['m_Father']['fileID'])
        return int(self.tr[p]['m_GameObject']['fileID']) if p in self.tr else None
    def active_in_hierarchy(self, gid):
        while gid is not None:
            if not int(self.go[gid].get('m_IsActive', 1) or 0): return False
            gid = self.parent(gid)
        return True

def is_pm(v):
    """a PlayMakerFSM: its 'fsm' is the Fsm itself. A few other scripts have an 'fsm' field that refers to one
    ({fileID: ...}). EnemyWakeRegion (wakes an enemy when the Knight comes near: Fungoons, Shrumal Ogres, husks...) was
    taken for an empty FSM, so its enemies never woke: it is a component. TinkEffect and SendFSMEventOnEntry, the other
    two, stay as they were exported (the runtime relies on those objects as they are)."""
    f = v.get('fsm')
    if not isinstance(f, dict): return False
    if 'fileID' in f and SCRIPT_GUIDS.get((v.get('m_Script') or {}).get('guid') or '') == 'EnemyWakeRegion': return False
    return True

def classify_mb(v):
    keys = [k for k in v if not k.startswith('m_')]
    ks = set(keys)
    if TK2D_ANIM_KEYS <= ks: return 'tk2dSpriteAnimator'
    if TK2D_SPRITE_KEYS <= ks: return 'tk2dSprite'
    if is_pm(v): return 'PlayMakerFSM'
    full = (v.get('m_Script') or {}).get('guid') or ''
    if full in SCRIPT_GUIDS: return SCRIPT_GUIDS[full]
    g = full[:8]
    if g in KNOWN_GUIDS: return KNOWN_GUIDS[g]
    return cs_sigs.identify(keys, SIGS)

# prefabs the 3DS runtime implements natively (ShowPromptMarker's marker, the audio player actor)
NATIVE_PREFABS = ('Arrow Prompt New', 'Arrow Prompt', 'Audio Player Actor', 'Audio Player Actor 2D')

TRAJAN_FONTS = ('0075d91c', '58cae6a7')     # TMP font assets drawn with the Trajan bitmap font (else Perpetua)
TRAJAN_GUIDS = set(export_ids().get('trajan_fonts') or ())     # this export's (make_lookups.py)

class Exporter:
    def __init__(self):
        self.libs = {}          # tk2d library guid -> set(clip names)
        self.prefabs = {}       # prefab guid -> template dict (filled lazily)
        self.prefab_queue = []
        self.audio = set()      # audio clip guids
        self.sprites = set()    # Sprite asset guids (SpriteRenderers, FSM sprite params)
        self.spritemap = metamap('Sprite')
        self.gomap = metamap('GameObject')
        self.clipmap = metamap('AudioClip')
        # audio mixers (AssetRipper's AudioMixerController folder): TransitionToAudioSnapshot names its snapshot by the
        # mixer's guid and the snapshot's file id, which mean nothing outside this export, so the snapshot goes in by name
        self.mixermap = {**metamap('AudioMixer'), **metamap('AudioMixerController')}
        self.mixers = {}        # mixer guid -> (mixer name, {file id: snapshot name})

    def ref(self, v, ctx):
        """Convert a {fileID,guid} reference."""
        if not isinstance(v, dict): return None
        fid = int(v.get('fileID', 0) or 0)
        g = v.get('guid')
        if not fid: return None
        if g:
            if g in self.gomap: self.want_prefab(g); return {'prefab': g}
            if g in self.clipmap: self.audio.add(g); return {'audio': os.path.splitext(os.path.basename(self.clipmap[g]))[0]}
            if g in self.spritemap: self.sprites.add(g); return {'sprite': g}
            if g in self.mixermap:
                mixer, snaps = self.mixer(g)
                if fid in snaps: return {'snapshot': snaps[fid], 'mixer': mixer}
            return {'asset': g}
        if fid in ctx['goidx']: return {'o': ctx['goidx'][fid]}
        if fid in ctx['compidx']: return {'o': ctx['compidx'][fid][0], 'c': ctx['compidx'][fid][1]}
        return {'ext': fid}

    def mixer(self, g):
        if g not in self.mixers:
            name, snaps = os.path.splitext(os.path.basename(self.mixermap[g]))[0], {}
            try:
                for fid, (t, v) in load_yaml_docs(self.mixermap[g]).items():
                    if not isinstance(v, dict): continue
                    if t == 'AudioMixerController' and v.get('m_Name'): name = v['m_Name']
                    if t == 'AudioMixerSnapshotController' and v.get('m_Name'): snaps[fid] = v['m_Name']
            except (OSError, UnicodeDecodeError) as ex:
                print(f'mixer {self.mixermap[g]}: {ex}', file=sys.stderr)
            self.mixers[g] = (name, snaps)
        return self.mixers[g]

    def want_prefab(self, g):
        if g not in self.prefabs:
            self.prefabs[g] = None; self.prefab_queue.append(g)

    def conv(self, x, ctx, depth=0):
        if isinstance(x, dict):
            if 'fileID' in x: return self.ref(x, ctx)
            return {k: self.conv(v, ctx, depth + 1) for k, v in x.items() if not k.startswith('m_')}
        if isinstance(x, list): return [self.conv(v, ctx, depth + 1) for v in x]
        if isinstance(x, str): return num(x)
        return x

    def fix_fsm_refs(self, x, ctx):
        """FSM params hold {'go': fid, 'guid'?}; map them to objects / prefabs / audio."""
        if isinstance(x, dict):
            if 'go' in x and isinstance(x['go'], int):
                r = self.ref({'fileID': x['go'], 'guid': x.get('guid')}, ctx)
                return r if r else None
            return {k: self.fix_fsm_refs(v, ctx) for k, v in x.items()}
        if isinstance(x, list): return [self.fix_fsm_refs(v, ctx) for v in x]
        return x

    def export_tree(self, doc, roots):
        """Export the subtrees under roots (gids). Returns {'objects': [...], 'fsms': [...]}"""
        order = []
        def walk(g, p):
            order.append((g, p)); me = len(order) - 1
            for k in doc.kids(g): walk(k, me)
        for r in roots: walk(r, -1)
        ctx = {'goidx': {g: i for i, (g, p) in enumerate(order)}, 'compidx': {}}
        # component indices first (so references between components resolve)
        for i, (g, p) in enumerate(order):
            ci = 0
            for fid, t, v in doc.comps.get(g, []):
                ctx['compidx'][fid] = (i, ci); ci += 1
        objects, fsms = [], []
        def rect_local_pos(doc, tr, lp):
            # RectTransforms (world-space TextMeshPro prompts...) are placed by anchoredPosition, not m_LocalPosition
            ap = tr.get('m_AnchoredPosition')
            if not isinstance(ap, dict): return float(lp['x']), float(lp['y'])
            amin, amax = tr.get('m_AnchorMin') or {'x': .5, 'y': .5}, tr.get('m_AnchorMax') or {'x': .5, 'y': .5}
            ax, ay = (float(amin['x']) + float(amax['x'])) / 2, (float(amin['y']) + float(amax['y'])) / 2
            x, y = float(ap['x']), float(ap['y'])
            par = doc.tr.get((tr.get('m_Father') or {}).get('fileID'))
            if par is not None and isinstance(par.get('m_SizeDelta'), dict):
                sd, pv = par['m_SizeDelta'], par.get('m_Pivot') or {'x': .5, 'y': .5}
                w, h = float(sd['x']), float(sd['y'])
                x += (ax - float(pv['x'])) * w; y += (ay - float(pv['y'])) * h
            return x, y
        for i, (g, p) in enumerate(order):
            go = doc.go[g]; tr = doc.tr[doc.go2tr[g]]
            lp, ls = tr['m_LocalPosition'], tr['m_LocalScale']
            px_, py_ = rect_local_pos(doc, tr, lp)
            o = {'n': str(go['m_Name']), 'p': p, 'x': px_, 'y': py_, 'z': float(lp.get('z', 0)),
                 'sx': float(ls['x']), 'sy': float(ls['y']), 'rz': round(quat_z(tr['m_LocalRotation']), 3),
                 'act': int(go.get('m_IsActive', 1) or 0), 'layer': int(go.get('m_Layer', 0) or 0), 'tag': num(go.get('m_TagString') or ''),
                 'c': []}
            for fid, t, v in doc.comps.get(g, []):
                en = int(v.get('m_Enabled', 1) or 0)
                c = None
                if t == 'BoxCollider2D':
                    c = {'t': 'box', 'ox': float(v['m_Offset']['x']), 'oy': float(v['m_Offset']['y']), 'w': float(v['m_Size']['x']), 'h': float(v['m_Size']['y'])}
                elif t == 'CircleCollider2D':
                    c = {'t': 'circle', 'ox': float(v['m_Offset']['x']), 'oy': float(v['m_Offset']['y']), 'r': float(v['m_Radius'])}
                elif t == 'PolygonCollider2D':
                    paths = [[[round(float(q['x']) + float(v['m_Offset']['x']), 4), round(float(q['y']) + float(v['m_Offset']['y']), 4)] for q in pth] for pth in (v.get('m_Points') or {}).get('m_Paths') or []]
                    c = {'t': 'poly', 'paths': paths}
                elif t == 'EdgeCollider2D':
                    c = {'t': 'edge', 'pts': [[float(q['x']), float(q['y'])] for q in v.get('m_Points') or []]}
                if c is not None:
                    c['trig'] = int(v.get('m_IsTrigger', 0) or 0); c['en'] = en; c['k'] = 'col'
                elif t == 'Rigidbody2D':
                    c = {'k': 'rb', 'g': float(v.get('m_GravityScale', 1)), 'type': int(v.get('m_BodyType', 0) or 0),
                         'kin': int(v.get('m_IsKinematic', 0) or 0), 'fixrot': int(v.get('m_Constraints', 0) or 0)}
                elif t == 'MeshRenderer':
                    c = {'k': 'mr', 'en': en}
                elif t == 'AudioSource':
                    # newer Unity keeps the clip in m_Resource (m_audioClip is empty in all of this game's data)
                    ref = v.get('m_audioClip')
                    if not (isinstance(ref, dict) and ref.get('guid')): ref = v.get('m_Resource')
                    clip = self.conv(ref, ctx)
                    if isinstance(clip, dict) and clip.get('audio'):
                        c = {'k': 'as', 'clip': clip['audio'], 'vol': float(v.get('m_Volume', 1) or 0), 'loop': int(v.get('Loop', 0) or 0),
                             'awake': int(v.get('m_PlayOnAwake', 0) or 0), 'pitch': float(v.get('m_Pitch', 1) or 1), 'en': en,
                             'min': float(v.get('MinDistance', 1) or 1), 'max': float(v.get('MaxDistance', 500) or 500),
                             'roll': int(v.get('rolloffMode', 0) or 0)}
                        # spatial blend (panLevelCustomCurve): 0 = plays at full volume anywhere (2D)
                        try: c['sb'] = float(v['panLevelCustomCurve']['m_Curve'][0]['value'])
                        except Exception: c['sb'] = 1.0
                elif t == 'SpriteRenderer':
                    sg = (v.get('m_Sprite') or {}).get('guid')
                    col = v.get('m_Color') or {}
                    c = {'k': 'sr', 'sp': sg, 'en': en, 'col': [float(col.get(q, 1)) for q in 'rgba'],
                         'fx': int(v.get('m_FlipX', 0) or 0), 'fy': int(v.get('m_FlipY', 0) or 0),
                         'ord': int(v.get('m_SortingOrder', 0) or 0)}
                    if sg: self.sprites.add(sg)
                elif t == 'MonoBehaviour':
                    cls = classify_mb(v)
                    if cls == 'tk2dSpriteAnimator':
                        lib = (v.get('library') or {}).get('guid')
                        clip = self.default_clip(lib, int(v.get('defaultClipId', 0) or 0)) if lib else None
                        c = {'k': 'anim', 'lib': lib, 'clip': clip, 'auto': int(v.get('playAutomatically', 0) or 0), 'en': en}
                        if lib:
                            self.libs.setdefault(lib, set())
                            if clip: self.libs[lib].add(clip)
                    elif cls == 'tk2dSprite':
                        c = {'k': 'sprite', 'coll': (v.get('collection') or {}).get('guid'), 'id': int(v.get('_spriteId', 0) or 0),
                             'color': self.conv(v.get('_color'), ctx), 'en': en}
                    elif cls == 'PlayMakerFSM':
                        try:
                            f = F.decode_fsm(v, doc.P, '')
                        except Exception as ex:
                            print('  fsm decode failed', o['n'], ex); continue
                        f = self.fix_fsm_refs(f, ctx)
                        f.pop('owner', None); f.pop('gid', None)
                        self.collect_fsm_assets(f, ctx)
                        fsms.append(f)
                        c = {'k': 'fsm', 'i': len(fsms) - 1, 'en': en}
                    elif cls in ('TMP_Text', 'TextMeshPro', 'TextMeshProUGUI') or 'm_fontAsset' in v:
                        col = v.get('m_fontColor') or {}
                        fg = (v.get('m_fontAsset') or {}).get('guid') or ''
                        c = {'k': 'tmp', 'text': str(v.get('m_text') or ''), 'size': float(v.get('m_fontSize') or 8),
                             'align': int(v.get('m_textAlignment', 0) or 0), 'col': [float(col.get(q, 1)) for q in 'rgba'],
                             'font': 1 if (fg in TRAJAN_GUIDS if TRAJAN_GUIDS else fg[:8] in TRAJAN_FONTS) else 0, 'en': en}
                        sd = tr.get('m_SizeDelta')
                        if isinstance(sd, dict) and isinstance(tr.get('m_AnchoredPosition'), dict):   # RectTransform: text box width + pivot
                            pv = tr.get('m_Pivot') or {}
                            c['rw'] = float(sd['x']); c['px'] = float(pv.get('x', 0.5))
                            c['rh'] = float(sd['y']); c['py'] = float(pv.get('y', 0.5))
                    elif cls:
                        fields = {k: self.conv(x, ctx) for k, x in v.items() if not k.startswith('m_')}
                        c = {'k': 'mb', 'cls': cls, 'en': en, 'f': fields}
                if c is not None: o['c'].append(c)
            # a Mecanim Animator on a sprite (death-burst splats and other effects): its clip, found by what it animates
            an = [v for fid, t, v in doc.comps.get(g, []) if t == 'Animator' and int(v.get('m_Enabled', 1) or 0)]
            if an:
                import uanim
                sg = next((c['sp'] for c in o['c'] if c.get('k') == 'sr'), None)
                clip = uanim.find_clip(o['n'], sg)
                if clip and (clip['cv'] or clip['spr']):
                    o['c'].append(uanim.to_comp(clip))      # appended last: earlier component indices stay put
                    for t_, sgk in clip['spr']: self.sprites.add(sgk)
            objects.append(o)
        return {'objects': objects, 'fsms': fsms}

    def collect_fsm_assets(self, f, ctx):
        """Animation clip names the FSM plays on its sprite animators."""
        for st in f['states']:
            for a in st['actions']:
                p = a['params']
                for key in ('clipName', 'animName', 'newAnimationClip'):
                    v = p.get(key)
                    if isinstance(v, str) and v: self.pending_clips.add(v)

    _libcache = {}
    def lib_clips(self, guid):
        if guid in self._libcache: return self._libcache[guid]
        path = self.gomap.get(guid); clips = []
        if path:
            for k, (t, v) in F.load_objs(path).items():
                if t == 'MonoBehaviour' and 'clips' in v:
                    clips = [str(c.get('name')) for c in v['clips'] or []]
                    break
        self._libcache[guid] = clips
        return clips
    def default_clip(self, lib, cid):
        cl = self.lib_clips(lib)
        return cl[cid] if 0 <= cid < len(cl) else None

    def export_prefabs(self):
        out = {}
        while self.prefab_queue:
            g = self.prefab_queue.pop(0)
            path = self.gomap.get(g)
            if not path or not path.endswith('.prefab'): out[g] = None; continue
            if os.path.splitext(os.path.basename(path))[0] in NATIVE_PREFABS: out[g] = None; continue
            doc = Doc(path)
            roots = [gid for gid in doc.go if doc.parent(gid) is None]
            if len(doc.go) > 400: out[g] = None; continue
            self.pending_clips = set()
            t = self.export_tree(doc, roots[:1])
            t['name'] = os.path.splitext(os.path.basename(path))[0]
            self.assign_clips(t)
            out[g] = t
        return out

    def assign_clips(self, tree):
        """Clips named in this tree's FSMs belong to the libraries of the tree's animators."""
        libs = [c['lib'] for o in tree['objects'] for c in o['c'] if c.get('k') == 'anim' and c.get('lib')]
        for lib in libs:
            have = set(self.lib_clips(lib))
            for clip in self.pending_clips:
                if clip in have: self.libs.setdefault(lib, set()).add(clip)

def doc_is_ancestor(doc, a, b):
    while b is not None:
        b = doc.parent(b)
        if b == a: return True
    return False

def enemy_roots(doc):
    """Objects with a HealthManager that are active, plus the top object they belong to."""
    roots = []
    for gid, cl in doc.comps.items():
        hm = any(t == 'MonoBehaviour' and 'hp' in v and 'smallGeoDrops' in v for fid, t, v in cl)
        if not hm or gid not in doc.go2tr: continue
        name = str(doc.go[gid]['m_Name'])
        if not doc.active_in_hierarchy(gid) or BR.X.base(name).startswith(BR.SKIP_ENEMIES): continue
        # climb to the enemy's own top object (stop at scene folders like _Enemies / Battle Scene)
        top = gid
        while True:
            p = doc.parent(top)
            if p is None: break
            pn = str(doc.go[p]['m_Name'])
            if pn.startswith('_') or 'Battle' in pn or pn.startswith('Summon') or 'Summon' in pn: break
            # parents that carry an FSM or a sprite belong to the enemy (e.g. "Infected Parent")
            pcomps = doc.comps.get(p, [])
            if not any(t == 'MonoBehaviour' for fid, t, v in pcomps): break
            top = p
        anc = []; q = doc.parent(gid)
        while q is not None: anc.append(str(doc.go[q]['m_Name'])); q = doc.parent(q)
        if any('Summon' in n for n in anc): continue
        if top not in roots: roots.append(top)
    return roots

NPC_FSMS = ('npc_control', 'Conversation Control',
            # Stagway stations, Goams, ...
            'Stag Bell', 'Bell Control', 'Stag Control', 'Grate Control', 'Worm Control', 'Worm Control Inert',
            # pickups and collectibles: items / charms / relics, grubs, soul totems, lore tablets, mask shards, vessel fragments
            'Shiny Control', 'Grub Control', 'soul_totem', 'inspect', 'Tablet Control', 'Inspection', 'Heart Container Control',
            'Vessel Fragment Control', 'Vengeful Spirit',
            # toll machines and gates, shops
            'Toll Machine', 'Toll Gate', 'toll switch', 'shop_control', 'Shop Region')
def npc_roots(doc):
    """Talkable NPCs: objects with npc_control / Conversation Control FSMs, exported with their whole object
    (prompt marker, ranges, dream dialogue...). Root = the object right under a scene folder ('_NPCs', '_Scenery'...)."""
    roots = []
    for gid, cl in doc.comps.items():
        if gid not in doc.go2tr: continue
        if not any(t == 'MonoBehaviour' and is_pm(v) and v['fsm'].get('name') in NPC_FSMS for fid, t, v in cl): continue
        top = gid
        while True:
            p = doc.parent(top)
            if p is None or str(doc.go[p]['m_Name']).startswith('_'): break
            top = p
        if top not in roots: roots.append(top)
    return roots

# FSMs that only animate / play sounds don't make an object worth running (its art stays baked into the room)
COSMETIC_ACTIONS = set('''Wait WaitRandom NextFrameEvent PlayParticleEmitter SetParticleEmission StopParticleEmitter SetParticleEmissionRate
SetParticleEmissionSpeed AudioPlay AudioPlayV2 AudioStop AudioPlaySimple AudioPlayRandom AudioPlayerOneShot AudioPlayerOneShotSingle
SetAudioPitch SetAudioVolume SetAudioClip FadeAudio AudioPlayInState Tk2dPlayAnimation Tk2dPlayAnimationWithEvents
Tk2dWatchAnimationEvents Tk2dPlayFrame Tk2dSpriteSetColor RandomFloat RandomInt SetFloatValue SetIntValue SetBoolValue iTweenRotateTo
iTweenRotateBy iTweenScaleTo iTweenScaleBy iTweenMoveBy iTweenMoveTo iTweenShakePosition iTweenPunchScale iTweenFadeTo Rotate SetRotation
SetScale SetRandomRotation EaseColor EaseFloat SetMaterialColor GetMaterialColor SetSpriteRendererColor FadeColorFader SetMeshRenderer
GetOwner FindChild GetChild SendRandomEvent SendRandomEventV2 FloatCompare FloatAdd FloatMultiply GetPosition SetPosition Translate
ObjectJitter SetColorValue ColorInterpolate SetLightIntensity SetLightColor SetLightRange VibrationPlayerPlay PlayVibration'''.split())
# handled natively (secret masks, area titles, benches, the hollow shade markers) or editor-only
NATIVE_FSMS = ('unmasker', 'remasker', 'remasker_inverse', 'Area Title Controller', 'PlayMaker Unity 2D', 'Spawn Offset',
               'Bench Control', 'Detect Hero', 'Toll Machine Bench')
NATIVE_NAMES = ('RestBench', 'Hollow_Shade Marker', '_Transition Gates', 'Camera Lock', 'CameraLockArea', 'Hazard Respawn',
                'Cave Spikes', 'Spikes', 'spike', 'Spike', 'Area Title Controller', '_Managers')

# C# behaviours that make an object part of the game (not just scenery): those objects run at runtime too
GAMEPLAY_CLASSES = {'Breakable', 'BreakableInfectedVine', 'BreakablePoleSimple', 'BreakablePoleTopLand', 'BounceShroom', 'GeoRock',
    'DropPlatform', 'DreamPlatform', 'BridgeSection', 'BridgeLever', 'LiftPlatform', 'RuinsLift', 'LiftChain', 'FlipPlatform',
    'ConveyorBelt', 'ConveyorSpeedZone', 'ConveyorZone', 'ConveyorMovement', 'HeroPlatformStick', 'BigBouncer', 'HealthCocoon',
    'StalactiteControl', 'ShadowGateColliderControl', 'DropCrystal', 'SpellGetOrb', 'DreamPlant', 'DreamPlantOrb', 'JellyEgg',
    'BigCentipede', 'BigCentipedeSection', 'ScuttlerControl', 'MusicRegion', 'SetPosIfPlayerdataBool', 'DeactivateIfPlayerdataTrue',
    'DeactivateIfPlayerdataFalse', 'DeactivateIfPlayerdataFalseDelayed', 'DeactivateInDarknessWithoutLantern', 'PersistentBoolItem',
    'PersistentIntItem', 'ActivateChildrenOnContact', 'MegaJellyZap', 'GlobControl', 'SlopePlat', 'PushableRubble', 'TriggerEnterEvent',
    'CollisionEnterEvent', 'SendEnemyMessageTrigger', 'EnemyWakeRegion', 'FakeBat', 'HiveKnightStinger', 'BossSceneController',
    'SteepSlope', 'VinePlatformCut', 'SoulOrb', 'EventRegister', 'EnemyKillEventListener', 'GrubBGControl', 'BasicSpriteAnimator'}

def interactive_roots(doc, skip=()):
    """Every object whose PlayMaker FSMs do more than animate, grouped with the parents that belong to it (a parent with
    components of its own; scene folders and empty grouping nodes stop the climb)."""
    roots = []
    for gid, cl in doc.comps.items():
        if gid not in doc.go2tr: continue
        fsms = [v for fid, t, v in cl if t == 'MonoBehaviour' and is_pm(v)]
        classes = {classify_mb(v) for fid, t, v in cl if t == 'MonoBehaviour' and not is_pm(v)}
        live = bool(classes & GAMEPLAY_CLASSES)
        if not fsms and not live: continue
        for v in ([] if live else fsms):
            nm = v['fsm'].get('name')
            try: f = F.decode_fsm(v, doc.P, '')
            except Exception: continue
            acts = {a['type'].split('.')[-1] for s in f['states'] for a in s['actions']}
            if nm in ('unmasker', 'remasker', 'remasker_inverse'):
                # masks uncovered by an event (a breakable wall's FSM sends UNCOVER) run here; trigger masks are masks.py's
                if not acts & {'Trigger2dEvent', 'Trigger2dEventLayer'}: live = True; break
                continue
            if nm in NATIVE_FSMS: continue
            if acts - COSMETIC_ACTIONS: live = True; break
        if not live: continue
        path = doc.P.path(gid)
        if any(path == s or path.startswith(s + '/') for s in skip): continue
        if any(seg.startswith(NATIVE_NAMES) for seg in path.split('/')): continue
        top = gid
        while True:
            p = doc.parent(top)
            if p is None: break
            pn = str(doc.go[p]['m_Name'])
            if pn.startswith('_') or 'Battle Scene' in pn: break
            if not any(t != 'Transform' and t != 'RectTransform' for fid, t, v in doc.comps.get(p, [])): break
            top = p
        if top not in roots: roots.append(top)
    return roots

# component fields whose target the script turns on at runtime: when it isn't otherwise exported, it is exported too
REFERENCED_FIELDS = {'BridgeLever': ('bridgeCollider',)}

def found_names(tree):
    """Object names the room's and its prefabs' FSMs look up with FindGameObject."""
    names = set()
    def walk(x):
        if isinstance(x, dict):
            if x.get('type') == 'FindGameObject' and isinstance(x.get('params'), dict):
                n = x['params'].get('objectName')
                if isinstance(n, str) and n: names.add(n)
            for v in x.values(): walk(v)
        elif isinstance(x, list):
            for v in x: walk(v)
    walk(tree.get('fsms')); walk(tree.get('prefabs'))
    return names

# tags scripts look objects up by that every scene has anyway (the Knight, cameras, managers, HUD...)
COMMON_TAGS = {'Player', 'Untagged', 'CameraParent', 'MainCamera', 'Vignette', 'SceneManager', 'Battle Scene', 'Hud Camera',
               'Inventory Top', 'Cinematic', 'Shop Window', 'Colosseum Manager', 'Grimmchild', 'GameManager', 'HeroBox'}
def found_tags(tree):
    """Tags the room's FSMs look objects up by (FindGameObject/GetRandomObject withTag...): the Soul Twisters pick a
    random "Teleplane" to teleport into. Such marker objects are only a collider and a tag, so they weren't exported."""
    tags = set()
    def walk(x):
        if isinstance(x, dict):
            if isinstance(x.get('params'), dict):
                t = x['params'].get('withTag')
                if isinstance(t, str) and t and t not in COMMON_TAGS: tags.add(t)
            for v in x.values(): walk(v)
        elif isinstance(x, list):
            for v in x: walk(v)
    walk(tree.get('fsms')); walk(tree.get('prefabs'))
    return tags

def export_room(room, out, force=()):
    path = BR.scene_path(room)
    doc = Doc(path)
    ex = Exporter()
    ex.pending_clips = set()
    additive = room.endswith(('_boss', '_boss_defeated', '_preload'))
    roots = enemy_roots(doc)
    roots += [r for r in npc_roots(doc) if r not in roots and not any(doc_is_ancestor(doc, q, r) for q in roots)]
    if additive:      # an additive scene (boss fight...) is all live objects: every top-level object
        roots = [g for g in doc.go if doc.parent(g) is None and g in doc.go2tr and not str(doc.go[g]['m_Name']).startswith(NATIVE_NAMES)]
    elif os.environ.get('OBJ_ALL', '1') == '1':
        import masks as MK
        mroots = [p for g, p, pers in MK.mask_roots(path)]
        extra = interactive_roots(doc, skip=mroots)
        extra = [r for r in extra if r not in roots and not any(doc_is_ancestor(doc, q, r) for q in roots)]
        extra = [r for r in extra if not any(q != r and doc_is_ancestor(doc, q, r) for q in extra)]
        # a new root may contain earlier roots: keep the outermost
        roots = [q for q in roots if not any(doc_is_ancestor(doc, r, q) for r in extra)] + extra
    if force:         # grouping objects scripts look up by name (see found_names below): they become roots over their children
        roots = [q for q in roots if not any(doc_is_ancestor(doc, f, q) for f in force)] + [f for f in force if f not in roots]
    tree = ex.export_tree(doc, roots)
    ur = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'uoroots'); os.makedirs(ur, exist_ok=True)
    open(f'{ur}/{room}.txt', 'w').write(''.join(doc.P.path(r) + '\n' for r in roots))
    # roots' world transforms (their parents are scene folders)
    sc = BR.X.Scene(BR.X.load_scene(path))
    k = 0
    for o in tree['objects']:
        if o['p'] == -1:
            W = sc.world(sc.go2tr[roots[k]]); k += 1
            # the world z too: a root's local z is relative to its scene folder (Saved Grubs at z 14.6 puts its grubs in
            # their holes at ~6, not at -8.7 in front of the camera), and z decides parallax and draw order
            o['x'], o['y'], o['z'], o['sx'], o['sy'], o['rz'] = W['x'], W['y'], W['z'], W['sx'], W['sy'], round(math.degrees(W['a']), 3)
    ex.assign_clips(tree)
    tree['prefabs'] = ex.export_prefabs()
    tree['libs'] = {g: sorted(c) for g, c in ex.libs.items()}
    tree['sprites'] = sorted(ex.sprites)
    tree['room'] = room
    # FindGameObject looks objects up by name: a plain grouping object (only a Transform) above exported objects was left
    # out, so the lookup failed (Gruz Mother's corpse finds "Fly Spawn" to release her babies; without it the battle
    # never ended). Export such a parent too, then redo the room.
    want = found_names(tree)
    have = {str(o['n']) for o in tree['objects']}
    add = set()
    for g in doc.go:
        n = str(doc.go[g].get('m_Name'))
        if n in want and n not in have and g in doc.go2tr and not n.startswith('_') and g not in force \
                and any(doc_is_ancestor(doc, g, r) for r in roots):
            add.add(g)
    # objects a gameplay script switches on later, referenced only from its fields: the City bridge levers (Fungus2_21)
    # enable "Bridge Collider 1/2", a disabled BoxCollider2D on its own object. Neither exported (no script of its own)
    # nor baked (disabled), the extended bridge had nothing to stand on.
    for o in tree['objects']:
        for c in o['c']:
            fields = REFERENCED_FIELDS.get(c.get('cls')) if c.get('k') == 'mb' else None
            for k in fields or ():
                v = (c.get('f') or {}).get(k)
                if not (isinstance(v, dict) and 'ext' in v): continue
                t, cv = doc.objs.get(v['ext'], (None, None))
                g = int(cv['m_GameObject']['fileID']) if isinstance(cv, dict) and isinstance(cv.get('m_GameObject'), dict) else v['ext']
                if g in doc.go and g in doc.go2tr and g not in force: add.add(g)
    tags = found_tags(tree)
    if tags:
        for g in doc.go:
            if g in force or g in roots or g not in doc.go2tr or str(doc.go[g].get('m_TagString') or '') not in tags: continue
            if any(doc_is_ancestor(doc, r, g) for r in roots) or any(doc_is_ancestor(doc, r, g) for r in add): continue
            add.add(g)
    if add and not additive:
        print(room, 'adding looked-up objects:', sorted(str(doc.go[g]['m_Name']) for g in add))
        return export_room(room, out, tuple(force) + tuple(sorted(add)))
    json.dump(tree, open(f'{out}/{room}.obj.json', 'w'), separators=(',', ':'))
    nobj = len(tree['objects']); nf = len(tree['fsms']); npf = sum(1 for v in tree['prefabs'].values() if v)
    print(room, 'roots', len(roots), 'objects', nobj, 'fsms', nf, 'prefabs', npf, 'libs', len(tree['libs']), 'audio', len(ex.audio))
    return tree

if __name__ == '__main__':
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    for r in sys.argv[2:]: export_room(r, out)
