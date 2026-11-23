#!/usr/bin/env python3
import sys as _s

_s.set_int_max_str_digits(0) if hasattr(_s, "set_int_max_str_digits") else None
"""
hk_extract_room.py - turn a Hollow Knight scene (AssetRipper .unity export) into
3DS-ready level data for the HK3DS demake.

Usage:
    python hk_extract_room.py <Scene.unity> <out_dir> [ROOM_NAME]

Outputs (in out_dir):
    <room>.json         everything, for tools / debugging
    <room>_level.h/.c   C data for devkitPro (collision grid + entity list)
    <room>_preview.png  map preview (needs Pillow; skipped if missing)

Coordinates: 1 unit = 1 tile (HK uses 64 px per unit). Origin bottom-left, y up,
exactly like Unity. Convert to screen with:  sy = (ROOM_H - y) * TILE_PX
"""
import re, sys, os, json, math, struct, functools
from collections import defaultdict
try:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from hkcommon import load_yaml_docs as _shared_load_yaml_docs
except Exception:
    _shared_load_yaml_docs = None

try:
    import yaml

    Loader = getattr(yaml, "CSafeLoader", yaml.SafeLoader)
except ImportError:
    sys.exit("pip install pyyaml")


# ---------------------------------------------------------------- load scene
@functools.lru_cache(maxsize=128)
def _load_scene_cached(path, size, mtime_ns):
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


def load_scene(path):
    if _shared_load_yaml_docs is not None:
        return _shared_load_yaml_docs(path)
    try:
        stat = os.stat(path)
    except OSError:
        return {}
    return _load_scene_cached(path, stat.st_size, stat.st_mtime_ns)


def f3(d):
    return {k: float(v) for k, v in d.items()}


class Scene:
    def __init__(self, objs):
        self.objs = objs
        self.go = {k: v for k, (t, v) in objs.items() if t == "GameObject"}
        self.tr = {
            k: v for k, (t, v) in objs.items() if t in ("Transform", "RectTransform")
        }
        self.go2tr = {v["m_GameObject"]["fileID"]: k for k, v in self.tr.items()}
        self._w = {}

    def world(self, tid):
        if tid in self._w:
            return self._w[tid]
        t = self.tr[tid]
        p, s, q = (
            f3(t["m_LocalPosition"]),
            f3(t["m_LocalScale"]),
            f3(t["m_LocalRotation"]),
        )
        a = math.atan2(
            2 * (q["w"] * q["z"] + q["x"] * q["y"]), 1 - 2 * (q["y"] ** 2 + q["z"] ** 2)
        )
        par = t["m_Father"]["fileID"]
        if par in self.tr:
            P = self.world(par)
            ca, sa = math.cos(P["a"]), math.sin(P["a"])
            lx, ly = p["x"] * P["sx"], p["y"] * P["sy"]
            r = dict(
                x=P["x"] + ca * lx - sa * ly,
                y=P["y"] + sa * lx + ca * ly,
                z=P["z"] + p["z"] * abs(P["sz"]),
                sz=P["sz"] * s["z"],
                a=P["a"] + a,
                sx=P["sx"] * s["x"],
                sy=P["sy"] * s["y"],
                active=P["active"] and self.active_self(tid),
            )
        else:
            r = dict(
                x=p["x"],
                y=p["y"],
                z=p["z"],
                sz=s["z"],
                a=a,
                sx=s["x"],
                sy=s["y"],
                active=self.active_self(tid),
            )
        self._w[tid] = r
        return r

    def active_self(self, tid):
        return bool(
            self.go[self.tr[tid]["m_GameObject"]["fileID"]].get("m_IsActive", 1)
        )

    def path(self, gid):
        n, t = [], self.go2tr.get(gid)
        while t in self.tr:
            n.append(str(self.go[self.tr[t]["m_GameObject"]["fileID"]]["m_Name"]))
            t = self.tr[t]["m_Father"]["fileID"]
        return "/".join(reversed(n))

    @staticmethod
    def xf(W, ox, oy):
        ca, sa = math.cos(W["a"]), math.sin(W["a"])
        lx, ly = ox * W["sx"], oy * W["sy"]
        return (W["x"] + ca * lx - sa * ly, W["y"] + sa * lx + ca * ly)


# ---------------------------------------------------------------- tilemap
def extract_tilemap(sc):
    for t, v in sc.objs.values():
        if t == "MonoBehaviour" and "spriteCollectionKey" in v and "layers" in v:
            W, H = v["width"], v["height"]
            grid = [[0] * W for _ in range(H)]
            for layer in v["layers"]:
                ch = layer["spriteChannel"]["chunks"]
                cols = math.ceil(W / v["partitionSizeX"])
                px, py = v["partitionSizeX"], v["partitionSizeY"]
                for ci, c in enumerate(ch):
                    s = c.get("spriteIds") or ""
                    if not s:
                        continue
                    if not isinstance(s, str):
                        s = str(s).zfill(
                            px * py * 8
                        )  # YAML read an all-digit hex blob as int
                    vals = struct.unpack("<%di" % (len(s) // 8), bytes.fromhex(s))
                    cx, cy = ci % cols, ci // cols
                    for i, val in enumerate(vals):
                        x, y = cx * px + i % px, cy * py + i // px
                        if x < W and y < H and val != -1:
                            grid[y][x] = 1
            return W, H, grid
    raise SystemExit("no tk2dTileMap found in scene")


# ---------------------------------------------------------------- colliders
def colliders(sc):
    out = []
    for t, v in sc.objs.values():
        if not t.endswith("Collider2D"):
            continue
        gid = v["m_GameObject"]["fileID"]
        if gid not in sc.go2tr:
            continue
        g = sc.go[gid]
        W = sc.world(sc.go2tr[gid])
        off = f3(v.get("m_Offset", {"x": 0, "y": 0}))
        paths = None
        if t == "BoxCollider2D":
            hx, hy = float(v["m_Size"]["x"]) / 2, float(v["m_Size"]["y"]) / 2
            pts = [
                sc.xf(W, off["x"] + dx * hx, off["y"] + dy * hy)
                for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1))
            ]
            paths = [pts]
        elif t == "PolygonCollider2D":
            paths = [
                [
                    sc.xf(W, float(p["x"]) + off["x"], float(p["y"]) + off["y"])
                    for p in pth
                ]
                for pth in v["m_Points"]["m_Paths"]
            ]
            pts = [p for pth in paths for p in pth]
        elif t == "EdgeCollider2D":
            pts = [
                sc.xf(W, float(p["x"]) + off["x"], float(p["y"]) + off["y"])
                for p in v["m_Points"]
            ]
            paths = [pts]
        elif t == "CircleCollider2D":
            cx, cy = sc.xf(W, off["x"], off["y"])
            r = float(v["m_Radius"]) * abs(W["sx"])
            pts = [(cx - r, cy - r), (cx + r, cy + r)]
        else:
            continue
        if not pts:
            continue
        xs, ys = [p[0] for p in pts], [p[1] for p in pts]
        out.append(
            dict(
                path=sc.path(gid),
                name=g["m_Name"],
                gid=gid,
                layer=g["m_Layer"],
                type=t,
                trigger=bool(v.get("m_IsTrigger")),
                active=W["active"] and bool(v.get("m_Enabled", 1)),
                box=[min(xs), min(ys), max(xs), max(ys)],
                paths=paths,
            )
        )
    return out


# ---------------------------------------------------------------- classify
HK_LAYER_TERRAIN, HK_LAYER_ENEMY, HK_LAYER_HAZARD = 8, 11, 17
ENEMY_NAMES = (
    "Buzzer",
    "Crawler",
    "Climber",
    "Zombie",
    "Fly",
    "Spitter",
    "Roller",
    "Hopper",
    "Mosquito",
    "Bouncer Fly",
)

ALL_KINDS = [
    "solid",
    "platform",
    "breakable_wall",
    "collapse_floor",
    "gate_door",
    "spikes",
    "stalactite",
    "enemy",
    "gate",
    "hazard_respawn",
    "hazard_respawn_marker",
    "respawn_marker",
    "geo_rock",
    "chest",
    "lifeblood_cocoon",
    "lore_tablet",
    "bench",
    "vine",
]


def base(n):
    return re.sub(r"\s*\(\d+\)$|\s+\d+$", "", n)


def classify(sc, cols):
    ents = []

    def add(kind, name, box, **kw):
        ents.append(
            dict(
                kind=kind,
                name=name,
                x=(box[0] + box[2]) / 2,
                y=(box[1] + box[3]) / 2,
                w=box[2] - box[0],
                h=box[3] - box[1],
                **kw,
            )
        )

    by_root = defaultdict(list)
    for c in cols:
        if not c["active"]:
            continue
        segs = c["path"].split("/")
        # group by the top-level "thing" (child of a _Folder root, or the root itself)
        key = (
            "/".join(segs[:2]) if segs[0].startswith("_") and len(segs) > 1 else segs[0]
        )
        by_root[key].append(c)

    for key, L in by_root.items():
        thing = key.split("/")[-1]
        b = base(thing)
        solid = [c for c in L if c["layer"] == HK_LAYER_TERRAIN and not c["trigger"]]
        if key.startswith("_Transition Gates"):
            gate = next((c for c in L if c["layer"] == 10), L[0])
            add("gate", thing, gate["box"])
        elif "Hazard Respawn" in key or "Hazard Respawn" in thing:
            for c in L:
                add("hazard_respawn", c["name"], c["box"])
        elif b.startswith("Tute Door") or b.startswith("Break Wall"):
            for c in solid:
                add("breakable_wall", thing, c["box"])
        elif b.startswith("Break Floor") or b.startswith("Collapser"):
            for c in solid:
                add("collapse_floor", thing, c["box"])
        elif b.startswith("Geo Rock"):
            c = next((c for c in L if not c["trigger"]), L[0])
            add("geo_rock", thing, c["box"])
        elif b.startswith("Chest"):
            add("chest", thing, next(c for c in L if c["name"] == thing)["box"])
        elif b.startswith("Health Cocoon"):
            add("lifeblood_cocoon", thing, solid[0]["box"] if solid else L[0]["box"])
        elif b.startswith("Tut_tablet") or "tablet" in b.lower():
            add("lore_tablet", thing, L[0]["box"])
        elif b.startswith("Stalactite"):
            for c in L:
                if c["layer"] == HK_LAYER_HAZARD:
                    add("stalactite", thing, c["box"])
        elif b.startswith("Hallownest_Main_Gate") or b.endswith("Gate"):
            for c in solid:
                add("gate_door", thing, c["box"])
        elif key.startswith("_Enemies") or any(b.startswith(e) for e in ENEMY_NAMES):
            body = next(
                (c for c in L if c["layer"] == HK_LAYER_ENEMY and c["name"] == thing),
                None,
            )
            if body:
                add("enemy", thing, body["box"])
        elif b.startswith("Vine Platform"):
            # Greenpath vine platform: hangs until the vine is cut, then drops onto its "Activated Plat" spot
            body = next((c for c in solid if c["path"] == thing), None) or (
                solid[0] if solid else None
            )
            if body:
                add("vine", thing, body["box"])
        elif b.startswith("plat_") or b.startswith("Platform"):
            for c in solid:
                add("platform", thing, c["box"])
        elif (
            b.startswith("Roof Collider")
            or thing == "roof colliders"
            or "TileMap" in thing
        ):
            pass  # tilemap is handled separately; roof colliders are smoothing shells
        else:
            for c in L:
                # layer 17 also holds "Enemy Kill" (under falling platforms) and "Tink" (nail clink) boxes: not hazards
                if (
                    c["layer"] == HK_LAYER_HAZARD
                    and not c["name"].startswith("Stalactite")
                    and base(str(c["name"])) not in ("Enemy Kill", "Tink")
                ):
                    add("spikes", thing, c["box"])
                elif c["layer"] == HK_LAYER_TERRAIN and not c["trigger"]:
                    add("solid", thing, c["box"])
    # loose markers (no colliders)
    for gid, g in sc.go.items():
        n = str(g["m_Name"])
        if gid in sc.go2tr and (
            n.startswith("Death Respawn Marker")
            or n.startswith("Hazard Respawn Marker")
        ):
            W = sc.world(sc.go2tr[gid])
            ents.append(
                dict(
                    kind=(
                        "respawn_marker"
                        if n.startswith("Death")
                        else "hazard_respawn_marker"
                    ),
                    name=n,
                    x=W["x"],
                    y=W["y"],
                    w=0,
                    h=0,
                )
            )
    ents.sort(key=lambda e: (e["kind"], e["x"]))
    return ents


# ---------------------------------------------------------------- writers
def write_c(out_dir, room, W, H, grid, ents):
    R = room.upper()
    r = room.lower()
    kinds = ALL_KINDS
    h = f"""// Auto-generated by hk_extract_room.py from {room} - do not edit by hand.
#pragma once
#include <stdint.h>

#define {R}_W {W}
#define {R}_H {H}
// 1 bit per tile, rows stored BOTTOM-UP (row 0 = y 0), {math.ceil(W/8)} bytes per row.
extern const uint8_t {r}_solid[{R}_H][({R}_W + 7) / 8];

static inline int {r}_is_solid(int x, int y) {{
    if (y >= {R}_H) return 0;                         // open sky above: top entrances live up here
    if (x < 0 || y < 0 || x >= {R}_W) return 1;      // sides and bottom = wall
    return ({r}_solid[y][x >> 3] >> (x & 7)) & 1;
}}

#ifndef HK_ENT_KIND_DEFINED
#define HK_ENT_KIND_DEFINED
typedef enum {{
{chr(10).join(f"    ENT_{k.upper()}," for k in kinds)}
    ENT_KIND_COUNT
}} EntKind;

// Centre position and size in tiles (y up). Name is the original Unity object.
typedef struct {{ EntKind kind; float x, y, w, h; const char* name; }} RoomEnt;
#endif

#define {R}_ENT_COUNT {len(ents)}
extern const RoomEnt {r}_ents[{R}_ENT_COUNT];
"""
    open(os.path.join(out_dir, f"{r}_level.h"), "w").write(h)
    rows = []
    for y in range(H):
        bs = []
        for bx in range(0, W, 8):
            v = 0
            for i in range(8):
                if bx + i < W and grid[y][bx + i]:
                    v |= 1 << i
            bs.append("0x%02X" % v)
        rows.append("    {" + ",".join(bs) + "}, // y=%d" % y)
    el = [
        f'    {{ENT_{e["kind"].upper()}, {e["x"]:.3f}f, {e["y"]:.3f}f, {e["w"]:.3f}f, {e["h"]:.3f}f, "{e["name"]}"}},'
        for e in ents
    ]
    c = f"""// Auto-generated by hk_extract_room.py from {room} - do not edit by hand.
#include "{r}_level.h"

const uint8_t {r}_solid[{R}_H][({R}_W + 7) / 8] = {{
{chr(10).join(rows)}
}};

const RoomEnt {r}_ents[{R}_ENT_COUNT] = {{
{chr(10).join(el)}
}};
"""
    open(os.path.join(out_dir, f"{r}_level.c"), "w").write(c)


def write_preview(out_dir, room, W, H, grid, ents, S=8):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return
    im = Image.new("RGB", (W * S, H * S), (18, 20, 28))
    d = ImageDraw.Draw(im)
    for y in range(H):
        for x in range(W):
            if grid[y][x]:
                d.rectangle(
                    [x * S, (H - 1 - y) * S, x * S + S - 1, (H - y) * S - 1],
                    fill=(120, 124, 140),
                )
    col = dict(
        enemy=(255, 150, 0),
        spikes=(255, 50, 50),
        stalactite=(255, 90, 90),
        platform=(230, 230, 230),
        breakable_wall=(190, 120, 60),
        collapse_floor=(200, 160, 90),
        gate=(80, 200, 255),
        gate_door=(80, 160, 255),
        hazard_respawn=(80, 255, 120),
        hazard_respawn_marker=(80, 255, 120),
        respawn_marker=(255, 255, 0),
        geo_rock=(120, 200, 255),
        chest=(255, 220, 80),
        lifeblood_cocoon=(90, 150, 255),
        lore_tablet=(220, 220, 255),
    )
    for e in ents:
        c = col.get(e["kind"], (255, 0, 255))
        x0, y0 = (e["x"] - e["w"] / 2) * S, (H - e["y"] - e["h"] / 2) * S
        x1, y1 = (e["x"] + e["w"] / 2) * S, (H - e["y"] + e["h"] / 2) * S
        if e["w"] == 0:
            d.ellipse([x0 - 4, y0 - 4, x0 + 4, y0 + 4], outline=c, width=2)
        else:
            d.rectangle(
                [x0, y0, max(x1, x0 + 2), max(y1, y0 + 2)],
                outline=c,
                width=2,
                fill=(
                    c
                    if e["kind"]
                    in (
                        "platform",
                        "breakable_wall",
                        "spikes",
                        "collapse_floor",
                        "gate_door",
                    )
                    else None
                ),
            )
        if e["kind"] in (
            "enemy",
            "gate",
            "chest",
            "lore_tablet",
            "respawn_marker",
            "lifeblood_cocoon",
        ):
            d.text((x0, y0 - 11), e["name"], fill=c)
    for x in range(0, W, 10):
        d.text((x * S + 2, H * S - 11), str(x), fill=(90, 90, 100))
    im.save(os.path.join(out_dir, f"{room.lower()}_preview.png"))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    scene, out_dir = sys.argv[1], sys.argv[2]
    room = (
        sys.argv[3]
        if len(sys.argv) > 3
        else os.path.splitext(os.path.basename(scene))[0]
    )
    os.makedirs(out_dir, exist_ok=True)
    sc = Scene(load_scene(scene))
    W, H, grid = extract_tilemap(sc)
    cols = colliders(sc)
    ents = classify(sc, cols)
    json.dump(
        dict(
            room=room,
            width=W,
            height=H,
            solid_rows_bottom_up=[
                "".join("#" if c else "." for c in row) for row in grid
            ],
            entities=ents,
        ),
        open(os.path.join(out_dir, f"{room.lower()}.json"), "w"),
        indent=1,
    )
    write_c(out_dir, room, W, H, grid, ents)
    write_preview(out_dir, room, W, H, grid, ents)
    from collections import Counter

    print(
        f"{room}: {W}x{H} tiles, {sum(map(sum, grid))} solid; entities:",
        dict(Counter(e["kind"] for e in ents)),
    )


if __name__ == "__main__":
    main()
