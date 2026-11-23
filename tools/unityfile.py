#!/usr/bin/env python3
"""A small Unity SerializedFile reader (format 17..22): header, types (with type trees when present), the object
table and the externals; reads objects through their type tree. Enough to look up Materials and Texture2D names in
the game's .assets files without third-party packages."""

import struct, sys, os


class R:
    def __init__(self, b, off=0, le=True):
        self.b = b
        self.o = off
        self.e = "<" if le else ">"

    def u8(self):
        v = self.b[self.o]
        self.o += 1
        return v

    def i16(self):
        v = struct.unpack_from(self.e + "h", self.b, self.o)[0]
        self.o += 2
        return v

    def u16(self):
        v = struct.unpack_from(self.e + "H", self.b, self.o)[0]
        self.o += 2
        return v

    def i32(self):
        v = struct.unpack_from(self.e + "i", self.b, self.o)[0]
        self.o += 4
        return v

    def u32(self):
        v = struct.unpack_from(self.e + "I", self.b, self.o)[0]
        self.o += 4
        return v

    def i64(self):
        v = struct.unpack_from(self.e + "q", self.b, self.o)[0]
        self.o += 8
        return v

    def u64(self):
        v = struct.unpack_from(self.e + "Q", self.b, self.o)[0]
        self.o += 8
        return v

    def f32(self):
        v = struct.unpack_from(self.e + "f", self.b, self.o)[0]
        self.o += 4
        return v

    def f64(self):
        v = struct.unpack_from(self.e + "d", self.b, self.o)[0]
        self.o += 8
        return v

    def cstr(self):
        j = self.b.index(b"\0", self.o)
        s = self.b[self.o : j].decode("utf-8", "replace")
        self.o = j + 1
        return s

    def align(self, n=4):
        self.o = (self.o + n - 1) // n * n

    def bytes_(self, n):
        v = self.b[self.o : self.o + n]
        self.o += n
        return v


COMMON = None


def common_strings():
    # Unity's built-in type-tree string table (offsets with bit 31 set). Only the names we need are looked up through
    # it in practice; unknown offsets fall back to '?'.
    return {}


class Node:
    __slots__ = ("type", "name", "size", "flags", "level", "children")


class SFile:
    def __init__(self, path):
        self.path = path
        b = open(path, "rb").read()
        self.b = b
        r = R(b, 0, False)
        msize, fsize, ver, doff = r.u32(), r.u32(), r.u32(), r.u32()
        self.ver = ver
        le = True
        if ver >= 9:
            le = r.u8() == 0
            r.o += 3
        if ver >= 22:
            msize = r.u32()
            fsize = r.i64()
            doff = r.i64()
            r.i64()
        self.doff = doff
        r.e = "<" if le else ">"
        self.unity = r.cstr()
        self.platform = r.i32()
        self.tt = bool(r.u8()) if ver >= 13 else True
        self.types = []
        for _ in range(r.i32()):
            self.types.append(self._type(r, False))
        objs = []
        for _ in range(r.i32()):
            if ver >= 14:
                r.align()
            pid = r.i64()
            start = r.i64() if ver >= 22 else r.u32()
            size = r.u32()
            ti = r.i32()
            objs.append((pid, start + doff, size, ti))
        self.objects = {pid: (start, size, ti) for pid, start, size, ti in objs}
        for _ in range(r.i32()):
            r.i32()
            if ver >= 14:
                r.align()
            r.i64()
        self.externals = []
        for _ in range(r.i32()):
            if ver >= 6:
                r.cstr()
            r.bytes_(16)
            r.i32()
            self.externals.append(r.cstr())
        self.le = le

    def _type(self, r, ref):
        t = {"cid": r.i32()}
        if self.ver >= 16:
            r.u8()
        if self.ver >= 17:
            t["sidx"] = r.i16()
        if self.ver >= 13:
            if (self.ver < 16 and t["cid"] < 0) or (self.ver >= 16 and t["cid"] == 114):
                r.bytes_(16)
            r.bytes_(16)
        if self.tt:
            nn, sbs = r.i32(), r.i32()
            raw = []
            for _ in range(nn):
                ver_, lvl, flags = r.u16(), r.u8(), r.u8()
                tso, nso, bs, idx, meta = r.u32(), r.u32(), r.i32(), r.i32(), r.i32()
                if self.ver >= 19:
                    r.u64()
                raw.append((lvl, flags, tso, nso, bs, meta))
            sb = r.bytes_(sbs)

            def s(o):
                if o & 0x80000000:
                    return COMMON_STR.get(o & 0x7FFFFFFF, "?")
                j = sb.index(b"\0", o)
                return sb[o:j].decode()

            nodes = []
            for lvl, flags, tso, nso, bs, meta in raw:
                n = Node()
                n.type, n.name, n.size, n.flags, n.level, n.children = (
                    s(tso),
                    s(nso),
                    bs,
                    meta,
                    lvl,
                    [],
                )
                nodes.append(n)
            stack = []
            for n in nodes:
                while stack and stack[-1].level >= n.level:
                    stack.pop()
                if stack:
                    stack[-1].children.append(n)
                stack.append(n)
            t["tree"] = nodes[0] if nodes else None
            if self.ver >= 21:
                [r.i32() for _ in range(r.i32())]
        return t

    def read(self, pid):
        start, size, ti = self.objects[pid]
        t = self.types[ti]
        if "tree" not in t or t["tree"] is None:
            return None
        r = R(self.b, start, self.le)
        return read_node(r, t["tree"])

    def cid(self, pid):
        return self.types[self.objects[pid][2]]["cid"]


def read_node(r, n):
    t = n.type
    if t == "string":
        ln = r.i32()
        v = r.bytes_(ln).decode("utf-8", "replace")
        r.align()
        return v
    if not n.children:
        if t in ("SInt32", "int", "Type*"):
            v = r.i32()
        elif t in ("UInt32", "unsigned int"):
            v = r.u32()
        elif t in ("SInt64", "long long"):
            v = r.i64()
        elif t in ("UInt64", "unsigned long long", "FileSize"):
            v = r.u64()
        elif t in ("SInt16", "short"):
            v = r.i16()
        elif t in ("UInt16", "unsigned short"):
            v = r.u16()
        elif t in ("SInt8", "UInt8", "char", "bool"):
            v = r.u8()
        elif t == "float":
            v = r.f32()
        elif t == "double":
            v = r.f64()
        else:
            v = r.bytes_(max(0, n.size))
        if n.flags & 0x4000:
            r.align()
        return v
    if t == "vector" or (n.children and n.children[0].type == "Array"):
        arr = n.children[0] if n.children[0].type == "Array" else n
        cnt = r.i32()
        el = arr.children[1]
        if el.type in ("UInt8", "SInt8", "char") and not el.children:
            v = r.bytes_(cnt)
        else:
            v = [read_node(r, el) for _ in range(cnt)]
        if n.flags & 0x4000 or arr.flags & 0x4000:
            r.align()
        return v
    if t == "Array":
        cnt = r.i32()
        el = n.children[1]
        v = [read_node(r, el) for _ in range(cnt)]
        if n.flags & 0x4000:
            r.align()
        return v
    if t == "map":
        arr = n.children[0]
        cnt = r.i32()
        pair = arr.children[1]
        v = [
            (read_node(r, pair.children[0]), read_node(r, pair.children[1]))
            for _ in range(cnt)
        ]
        if n.flags & 0x4000 or arr.flags & 0x4000:
            r.align()
        return v
    d = {}
    for c in n.children:
        d[c.name] = read_node(r, c)
    if n.flags & 0x4000:
        r.align()
    return d


# Unity's common string table (offset -> name), the part type trees use
COMMON_STR = {}
_cs = (
    "AABB\0AnimationClip\0AnimationCurve\0AnimationState\0Array\0Base\0BitField\0bitset\0bool\0char\0ColorRGBA\0Component\0data\0deque\0"
    "double\0dynamic_array\0FastPropertyName\0first\0float\0Font\0GameObject\0Generic Mono\0GradientNEW\0GUID\0GUIStyle\0int\0list\0long long\0"
    "map\0Matrix4x4f\0MdFour\0MonoBehaviour\0MonoScript\0m_ByteSize\0m_Curve\0m_EditorClassIdentifier\0m_EditorHideFlags\0m_Enabled\0"
    "m_ExtensionPtr\0m_GameObject\0m_Index\0m_IsArray\0m_IsStatic\0m_MetaFlag\0m_Name\0m_ObjectHideFlags\0m_PrefabInternal\0m_PrefabParentObject\0"
    "m_Script\0m_StaticEditorFlags\0m_Type\0m_Version\0Object\0pair\0PPtr<Component>\0PPtr<GameObject>\0PPtr<Material>\0PPtr<MonoBehaviour>\0"
    "PPtr<MonoScript>\0PPtr<Object>\0PPtr<Prefab>\0PPtr<Sprite>\0PPtr<TextAsset>\0PPtr<Texture>\0PPtr<Texture2D>\0PPtr<Transform>\0Prefab\0"
    "Quaternionf\0Rectf\0RectInt\0RectOffset\0second\0set\0short\0size\0SInt16\0SInt32\0SInt64\0SInt8\0staticvector\0string\0TextAsset\0"
    "TextMesh\0Texture\0Texture2D\0Transform\0TypelessData\0UInt16\0UInt32\0UInt64\0UInt8\0unsigned int\0unsigned long long\0unsigned short\0"
    "vector\0Vector2f\0Vector3f\0Vector4f\0m_ScriptingClassIdentifier\0Gradient\0Type*\0int2_storage\0int3_storage\0BoundsInt\0m_CorrespondingSourceObject\0"
    "m_PrefabInstance\0m_PrefabAsset\0FileSize\0Hash128\0"
)
_o = 0
for _s in _cs.split("\0"):
    if _s:
        COMMON_STR[_o] = _s
    _o += len(_s.encode()) + 1

if __name__ == "__main__":
    sf = SFile(sys.argv[1])
    print(
        sf.unity,
        "format",
        sf.ver,
        "typetree",
        sf.tt,
        "types",
        len(sf.types),
        "objects",
        len(sf.objects),
        "externals",
        sf.externals[:5],
    )
