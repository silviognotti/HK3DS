# Decode PlayMaker FSMs (PlayMakerFSM components) from an AssetRipper scene/prefab into plain JSON.
#   python fsm_extract.py <scene.unity|prefab> [object-name-substring] > out.json
# Each FSM: name, owner path, start state, events, global transitions, variables, states with transitions and
# actions (type, enabled, params by field name). Param values: numbers, strings, {"var": name} for variable refs,
# {"go": path} / {"owner": true} for GameObjects, {"event": name}, lists for arrays.
import sys, re, yaml, struct, json, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

class _L(getattr(yaml, "CSafeLoader", yaml.SafeLoader)): pass
# AssetRipper writes byte arrays as hex strings; YAML would turn "0101..." into (octal) ints. Keep ints as strings.
_L.yaml_implicit_resolvers = {k: [(t, r) for t, r in v if t not in ('tag:yaml.org,2002:int', 'tag:yaml.org,2002:bool')] for k, v in yaml.SafeLoader.yaml_implicit_resolvers.items()}

PT = ['Integer', 'Boolean', 'Float', 'String', 'Color', 'ObjectReference', 'LayerMask', 'Enum', 'Vector2', 'Vector3', 'Vector4',
      'Rect', 'Array', 'Character', 'AnimationCurve', 'FsmFloat', 'FsmInt', 'FsmBool', 'FsmString', 'FsmGameObject',
      'FsmOwnerDefault', 'FunctionCall', 'FsmAnimationCurve', 'FsmEvent', 'FsmObject', 'FsmColor', 'Unsupported', 'GameObject',
      'FsmVector3', 'LayoutOption', 'FsmRect', 'FsmEventTarget', 'FsmMaterial', 'FsmTexture', 'Quaternion', 'FsmQuaternion',
      'FsmProperty', 'FsmVector2', 'FsmTemplateControl', 'FsmVar', 'CustomClass', 'FsmArray', 'FsmEnum']

def H(s): return bytes.fromhex(s) if isinstance(s, str) and s else b''
def ints(s): b = H(s); return list(struct.unpack('<%di' % (len(b) // 4), b))
def num(x):
    try: return float(x) if ('.' in str(x) or 'e' in str(x).lower()) else int(x)
    except Exception: return x

def load_objs(path):
    src = open(path, encoding='utf-8').read()
    parts = re.split(r"^--- !u!(\d+) &(-?\d+).*$", src, flags=re.M)
    objs = {}
    for i in range(1, len(parts), 3):
        body = parts[i + 2]
        need = 'fsm:' in body or 'm_Name' in body or 'm_Father' in body or 'm_GameObject' in body
        if not need: continue
        try: d = yaml.load(body, Loader=_L)
        except Exception: continue
        if d:
            (t, v), = d.items(); objs[int(parts[i + 1])] = (t, v)
    return objs

class Paths:
    def __init__(self, objs):
        self.go = {k: v for k, (t, v) in objs.items() if t == 'GameObject'}
        self.tr = {k: v for k, (t, v) in objs.items() if t in ('Transform', 'RectTransform')}
        self.go2tr = {int(v['m_GameObject']['fileID']): k for k, v in self.tr.items()}
    def path(self, gid):
        n, t = [], self.go2tr.get(gid)
        while t is not None and t in self.tr:
            g = int(self.tr[t]['m_GameObject']['fileID']); n.append(str(self.go.get(g, {}).get('m_Name')))
            p = int(self.tr[t]['m_Father']['fileID']); t = p if p in self.tr else None
        return '/'.join(reversed(n))

def objref(v):
    if not isinstance(v, dict) or not int(v.get('fileID', 0) or 0): return None
    r = {'go': int(v['fileID'])}
    if v.get('guid'): r['guid'] = v['guid']
    return r

def fsmvar(d, kind):
    """FsmFloat-like dict from a typed param list."""
    if d is None: return None
    if int(d.get('useVariable', 0) or 0): return {'var': d['name']} if d.get('name') else {'none': True}
    v = d.get('value')
    if kind in ('FsmGameObject', 'FsmObject', 'FsmMaterial', 'FsmTexture'):
        return objref(v)
    if isinstance(v, dict) and set(v) <= {'x', 'y', 'z', 'w', 'r', 'g', 'b', 'a'}: return {k: num(x) for k, x in v.items()}
    return num(v) if kind != 'FsmString' else v

def decode_actions(ad, P):
    names = ad.get('actionNames') or []
    if not names: return []
    starts = ints(ad['actionStartIndex'])
    types = ints(ad['paramDataType']); pos = ints(ad['paramDataPos']); size = ints(ad['paramByteDataSize'])
    pnames = ad.get('paramName') or []
    bd = H(ad.get('byteData'))
    en = H(ad.get('actionEnabled')) if isinstance(ad.get('actionEnabled'), str) else b'\x01' * len(names)
    arr_sizes = ints(ad.get('arrayParamSizes')); arr_types = ad.get('arrayParamTypes') or []
    L = lambda key: ad.get(key) or []
    def byte_str(p, n): return bd[p:p + n].decode('utf-8', 'replace')
    def one(k):
        t = PT[types[k]] if 0 <= types[k] < len(PT) else str(types[k])
        p, n = pos[k], size[k]
        if t == 'Integer' or t == 'LayerMask' or t == 'Enum': return struct.unpack_from('<i', bd, p)[0] if n >= 4 else 0
        if t == 'Boolean': return bool(bd[p]) if n else False
        if t == 'Float': return struct.unpack_from('<f', bd, p)[0]
        if t == 'String':
            if n: return byte_str(p, n)
            sp = L('stringParams')
            return sp[p] if 0 <= p < len(sp) and sp[p] is not None else ''
        if t in ('Vector2', 'Vector3', 'Vector4', 'Color', 'Rect', 'Quaternion'):
            return list(struct.unpack_from('<%df' % (n // 4), bd, p))
        if t == 'FsmFloat':
            if n:
                v = struct.unpack_from('<f', bd, p)[0]; use = bd[p + 4] if n > 4 else 0; nm = byte_str(p + 5, n - 5)
                return ({'var': nm} if nm else {'none': True}) if use else v
            return fsmvar(L('fsmFloatParams')[p], t)
        if t == 'FsmInt':
            if n:
                v = struct.unpack_from('<i', bd, p)[0]; use = bd[p + 4] if n > 4 else 0; nm = byte_str(p + 5, n - 5)
                return ({'var': nm} if nm else {'none': True}) if use else v
            return fsmvar(L('fsmIntParams')[p], t)
        if t == 'FsmBool':
            if n:
                v = bool(bd[p]); use = bd[p + 1] if n > 1 else 0; nm = byte_str(p + 2, n - 2)
                return ({'var': nm} if nm else {'none': True}) if use else v
            return fsmvar(L('fsmBoolParams')[p], t)
        if t == 'FsmEvent':
            if n: return {'event': byte_str(p, n)}
            sp = L('stringParams')      # newer PlayMaker serialization: the event name sits in stringParams[pos]
            return {'event': sp[p]} if 0 <= p < len(sp) and sp[p] else None
        if t == 'FsmVector2':
            if n:
                v = list(struct.unpack_from('<2f', bd, p)); use = bd[p + 8] if n > 8 else 0; nm = byte_str(p + 9, n - 9)
                return ({'var': nm} if nm else {'none': True}) if use else v
            return fsmvar(L('fsmVector2Params')[p], t)
        if t in ('FsmVector3', 'FsmColor', 'FsmQuaternion', 'FsmRect') and n:
            k_ = 3 if t == 'FsmVector3' else 4
            v = list(struct.unpack_from('<%df' % k_, bd, p)); use = bd[p + 4 * k_] if n > 4 * k_ else 0; nm = byte_str(p + 4 * k_ + 1, n - 4 * k_ - 1)
            return ({'var': nm} if nm else {'none': True}) if use else v
        lists = {'FsmString': 'fsmStringParams', 'FsmGameObject': 'fsmGameObjectParams', 'FsmVector3': 'fsmVector3Params',
                 'FsmColor': 'fsmColorParams', 'FsmRect': 'fsmRectParams', 'FsmQuaternion': 'fsmQuaternionParams',
                 'FsmObject': 'fsmObjectParams', 'FsmMaterial': 'fsmObjectParams', 'FsmTexture': 'fsmObjectParams',
                 'FsmEnum': 'fsmEnumParams', 'FsmArray': 'fsmArrayParams', 'FsmVar': 'fsmVarParams'}
        if t in lists:
            lst = L(lists[t]); d = lst[p] if p < len(lst) else None
            if t == 'FsmVar' and d:
                vn = d.get('variableName'); tn = d.get('type')
                return {'var': vn} if vn else {'type': tn, 'f': num(d.get('floatValue')), 'i': num(d.get('intValue')), 'b': num(d.get('boolValue')), 's': d.get('stringValue')}
            if t == 'FsmEnum' and d:
                return {'var': d['name']} if int(d.get('useVariable', 0) or 0) and d.get('name') else num(d.get('intValue'))
            if t == 'FsmArray' and d:
                return {'var': d['name']} if int(d.get('useVariable', 0) or 0) and d.get('name') else {'array': d.get('type'), 'floats': d.get('floatValues'), 'ints': d.get('intValues'), 'strings': d.get('stringValues'), 'bools': d.get('boolValues')}
            return fsmvar(d, t)
        if t == 'FsmOwnerDefault':
            d = L('fsmOwnerDefaultParams')[p]
            if int(d.get('ownerOption', 0) or 0) == 0: return {'owner': True}
            g = d.get('gameObject') or {}
            if int(g.get('useVariable', 0) or 0) and g.get('name'): return {'var': g['name']}
            return objref(g.get('value'))
        if t == 'FsmEventTarget':
            d = L('fsmEventTargetParams')[p]
            g = d.get('gameObject') or {}
            if int(g.get('ownerOption', 0) or 0) == 0: go = {'owner': True}
            else:
                gg = g.get('gameObject') or {}
                go = {'var': gg['name']} if int(gg.get('useVariable', 0) or 0) and gg.get('name') else objref(gg.get('value'))
            fn = d.get('fsmName') or {}
            return {'target': num(d.get('target')), 'fsmName': ({'var': fn['name']} if int(fn.get('useVariable', 0) or 0) and fn.get('name') else fn.get('value')),
                    'go': go, 'children': bool(num((d.get('sendToChildren') or {}).get('value') or 0)),
                    'exclude': bool(num((d.get('excludeSelf') or {}).get('value') or 0))}
        if t == 'FsmProperty':
            d = L('fsmPropertyParams')[p]; return {'property': d.get('PropertyName'), 'target': d.get('TargetObject')}
        if t == 'FunctionCall':
            d = L('functionCallParams')[p]
            pt = d.get('parameterType') or d.get('ParameterType')
            key = {'int': 'IntParameter', 'float': 'FloatParameter', 'bool': 'BoolParameter', 'string': 'StringParameter',
                   'GameObject': 'GameObjectParameter', 'Vector2': 'Vector2Parameter', 'Vector3': 'Vector3Parameter'}.get(str(pt))
            v = None
            if key:
                q = d.get(key) or {}
                if int(q.get('useVariable', 0) or 0) and q.get('name'): v = {'var': q['name']}
                elif key == 'GameObjectParameter': v = objref(q.get('value'))
                else: v = q.get('value')
                if isinstance(v, str) and key in ('IntParameter', 'FloatParameter', 'BoolParameter'): v = num(v)
            return {'function': d.get('FunctionName'), 'type': pt, 'v': v, 'f': num((d.get('FloatParameter') or {}).get('value')), 's': (d.get('StringParameter') or {}).get('value')}
        if t == 'ObjectReference':
            lst = L('unityObjectParams'); d = lst[p] if p < len(lst) else None
            return objref(d)
        if t == 'AnimationCurve' or t == 'FsmAnimationCurve':
            lst = L('animationCurveParams'); d = lst[p] if p < len(lst) else None
            return {'curve': d}
        return {'unsupported': t}
    out = []
    for ai, nm in enumerate(names):
        e = starts[ai + 1] if ai + 1 < len(names) else len(types)
        params = {}
        k = starts[ai]
        while k < e:
            t = PT[types[k]] if 0 <= types[k] < len(PT) else str(types[k])
            pname = pnames[k] if k < len(pnames) else str(k)
            if t == 'Array':
                n = arr_sizes[pos[k]] if pos[k] < len(arr_sizes) else 0
                params[pname] = [one(k + 1 + j) for j in range(n)]
                k += 1 + n; continue
            try: params[pname] = one(k)
            except Exception as ex: params[pname] = {'error': str(ex)}
            k += 1
        out.append({'type': nm.split('.')[-1], 'enabled': bool(en[ai]) if ai < len(en) else True, 'params': params})
    return out

_TEMPLATES = None
_TEMPLATE_FSM = {}
def template_fsm(guid):
    """The FSM of a PlayMaker template asset (fsm_templates.json maps the guid: fsm_templates.py, make_lookups.py)."""
    global _TEMPLATES
    if _TEMPLATES is None:
        from hkcommon import lookup_path
        tp = lookup_path('fsm_templates.json')
        _TEMPLATES = json.load(open(tp)) if os.path.exists(tp) else {}
    path = _TEMPLATES.get(guid)
    if not path: return None, None
    from hkcommon import EX
    if not os.path.isabs(path): path = os.path.join(EX, path)                # paths are relative to the export's ex/
    elif not os.path.exists(path) and '/ex/' in path: path = os.path.join(EX, path.split('/ex/', 1)[1])
    if not os.path.exists(path): return None, None
    if path not in _TEMPLATE_FSM:
        objs = load_objs(path)
        _TEMPLATE_FSM[path] = next(((v['fsm'], Paths(objs)) for k, (t, v) in objs.items() if isinstance(v.get('fsm'), dict)), (None, None))
    return _TEMPLATE_FSM[path]

def decode_fsm(v, P, path):
    f = v['fsm']
    tg = (v.get('fsmTemplate') or {}).get('guid')
    if tg:      # a templated FSM: the template's states, events and start state with this component's variable values
        tf, tP = template_fsm(tg)
        if tf:
            merged = dict(tf); merged['name'] = f.get('name'); merged['variables'] = f.get('variables') or tf.get('variables')
            if 'RestartOnEnable' in f: merged['RestartOnEnable'] = f['RestartOnEnable']
            f = merged; P = tP or P
    vars_ = {}
    V = f.get('variables') or {}
    for key, kind in (('floatVariables', 'float'), ('intVariables', 'int'), ('boolVariables', 'bool'), ('stringVariables', 'string'),
                      ('vector2Variables', 'vector2'), ('vector3Variables', 'vector3'), ('gameObjectVariables', 'gameobject'),
                      ('objectVariables', 'object'), ('colorVariables', 'color'), ('enumVariables', 'enum'), ('arrayVariables', 'array')):
        for d in V.get(key) or []:
            val = d.get('value')
            if kind in ('gameobject', 'object'): val = objref(val)
            elif kind in ('float', 'int'): val = num(val)
            elif kind == 'bool': val = bool(int(val or 0))
            elif isinstance(val, dict): val = {k: num(x) for k, x in val.items() if not isinstance(x, dict)}
            elif kind == 'enum': val = num(d.get('intValue'))
            vars_[d['name']] = {'type': kind, 'value': val}
    states = []
    for st in f.get('states') or []:
        states.append({'name': st['name'],
                       'transitions': [{'event': (t.get('fsmEvent') or {}).get('name'), 'to': t.get('toState')} for t in st.get('transitions') or []],
                       'actions': decode_actions(st['actionData'], P)})
    return {'fsm': f.get('name'), 'owner': path, 'start': f.get('startState'), 'restart': int(f.get('RestartOnEnable', 1) or 0) if 'RestartOnEnable' in f else 1,
            'events': [e.get('name') for e in f.get('events') or []],
            'global': [{'event': (t.get('fsmEvent') or {}).get('name'), 'to': t.get('toState')} for t in f.get('globalTransitions') or []],
            'variables': vars_, 'states': states}

def extract(path, filt=None):
    objs = load_objs(path); P = Paths(objs)
    out = []
    for fid, (t, v) in objs.items():
        if t != 'MonoBehaviour' or not isinstance(v, dict) or not isinstance(v.get('fsm'), dict) or 'states' not in v['fsm']: continue
        gid = int(v['m_GameObject']['fileID']); p = P.path(gid)
        if filt and filt not in p: continue
        d = decode_fsm(v, P, p); d['gid'] = gid
        out.append(d)
    return out, P

if __name__ == '__main__':
    res, P = extract(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else None)
    json.dump(res, sys.stdout, indent=1, default=str)
