#!/usr/bin/env python3
"""The game map (Game_Map.prefab): every area's room sprites, Cornifer's rough version and the full (quill) version.
-> romfs/gfx/map.spr  (frames "R:<scene>" rough / "F:<scene>" full, MAP_PPU px per map unit)
-> romfs/text/map.txt (A <area> <pd bool> / R <area index> <scene> <cx> <cy> <w> <h> <active by default> <rgba hex>)
Coordinates are map units in Game_Map's local space, y up.
python build_map.py <romfs>"""

import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import bake_scene as B
from build_sprites import SpriteSet
from texpack import premul_resize, FMT_RGBA4

MAP_PPU = 36
AREA_BOOL = {
    "areaAncientBasin": "mapAbyss",
    "areaCity": "mapCity",
    "areaCliffs": "mapCliffs",
    "areaCrossroads": "mapCrossroads",
    "areaCrystalPeak": "mapMines",
    "areaDeepnest": "mapDeepnest",
    "areaFogCanyon": "mapFogCanyon",
    "areaFungalWastes": "mapFungalWastes",
    "areaGreenpath": "mapGreenpath",
    "areaKingdomsEdge": "mapOutskirts",
    "areaQueensGardens": "mapRoyalGardens",
    "areaRestingGrounds": "mapRestingGrounds",
    "areaDirtmouth": "mapDirtmouth",
    "areaWaterways": "mapWaterways",
}


def fl(x):
    try:
        return float(x)
    except Exception:
        return 0.0


PIN_PX = 40  # pins are packed at this many pixels across (drawn at their map size, never under a few pixels)
_pin_frames = {}


def pin_frame(ss, guid):
    if guid in _pin_frames:
        return _pin_frames[guid]
    sp = B.sprite_asset(guid)
    r = B.sprite_image(sp) if sp else None
    if not r:
        _pin_frames[guid] = None
        return None
    im, ox, oy, RW, RH, px, py, ppu = r
    from PIL import Image

    full = Image.new(
        "RGBA", (max(1, int(round(RW))), max(1, int(round(RH)))), (0, 0, 0, 0)
    )
    full.paste(im, (int(round(ox)), int(round(RH - oy - im.height))))
    k = PIN_PX / max(RW, RH)
    pw, ph = max(1, round(RW * k)), max(1, round(RH * k))
    nm = f"PIN:{guid[:8]}"
    ss.add_named(nm, premul_resize(full, pw, ph), -pw // 2, -ph // 2)
    _pin_frames[guid] = (
        nm,
        RW / ppu,
        RH / ppu,
        (RW / 2 - px) / ppu,
        (RH / 2 - py) / ppu,
    )
    return _pin_frames[guid]


def pin_line(room, go, cs, tr, rx, ry, rsx, rsy, ss, P):
    """P <scene> <x> <y> <w> <h> <frame> <kind> <bool a> <bool b> <bool c> <frame 2>: kind 0 always shown; 1 the pin FSM
    (a = the pin type bool, b = the specific bool, c = the completed bool); 2 a sprite swap on bool a (toll benches)
    """
    if str(go.get("m_IsActive", 1)) == "0":
        return None  # cocoon / whispering root pins: from lists we don't keep
    sr = next((v for t, v in cs if t == "SpriteRenderer"), None)
    if not sr or not (sr.get("m_Sprite") or {}).get("guid"):
        return None
    fsm = None
    for t, v in cs:
        if t == "MonoBehaviour" and "fsm" in v:
            fsm = F.decode_fsm(v, P, str(go.get("m_Name")))
    vs = {
        k: (v.get("value") or "-") for k, v in (fsm["variables"] if fsm else {}).items()
    }
    alt = "-"
    if "Pin Type Bool" in vs:
        kind, a, b, c = (
            1,
            vs["Pin Type Bool"],
            vs.get("Specific Bool", "-"),
            vs.get("Completed Bool", "-"),
        )
    elif "PlayerData Bool" in vs:
        kind, a, b, c = 2, vs["PlayerData Bool"], "-", "-"
        for st in fsm["states"]:
            for ac in st.get("actions", []):
                g = ((ac.get("params") or {}).get("sprite") or {}).get("guid")
                if st["name"] == "Inactive" and g and pin_frame(ss, g):
                    alt = pin_frame(ss, g)[0]
    elif not fsm and str(sr.get("m_Enabled", 1)) != "0":
        kind, a, b, c = 0, "-", "-", "-"
    else:
        return None
    pf = pin_frame(ss, sr["m_Sprite"]["guid"])
    if not pf:
        return None
    nm, w, h, cxo, cyo = pf
    lp, ls = tr["m_LocalPosition"], tr["m_LocalScale"]
    psx, psy = fl(ls["x"]) * rsx, fl(ls["y"]) * rsy
    x = rx + fl(lp["x"]) * rsx + cxo * psx
    y = ry + fl(lp["y"]) * rsy + cyo * psy
    return f"P {room} {x:.3f} {y:.3f} {w * abs(psx):.3f} {h * abs(psy):.3f} {nm} {kind} {a} {b} {c} {alt}"


def main(romfs):
    os.makedirs(f"{romfs}/text", exist_ok=True)
    os.makedirs(f"{romfs}/gfx", exist_ok=True)
    objs = F.load_objs(f"{B.EX}/GameObject/Game_Map.prefab")
    P = F.Paths(objs)
    gm = next(
        v
        for k, (t, v) in objs.items()
        if t == "MonoBehaviour" and "areaCrossroads" in v
    )
    tr_of = {}
    for k, (t, v) in objs.items():
        if t in ("Transform", "RectTransform"):
            tr_of[int(v["m_GameObject"]["fileID"])] = v

    def comps(gid):
        out = []
        for c in objs[gid][1]["m_Component"]:
            out.append(objs[int(c["component"]["fileID"])])
        return out

    ss = SpriteSet(1.0)
    lines = []
    areas = []
    for field, pdb in AREA_BOOL.items():
        agid = int(gm[field]["fileID"])
        areas.append((agid, pdb))
    for ai, (agid, pdb) in enumerate(areas):
        an = P.path(agid).split("/")[-1]
        at = tr_of[agid]
        ap = at["m_LocalPosition"]
        ax, ay = fl(ap["x"]), fl(ap["y"])
        lines.append(f'A {an.replace(" ", "_")} {pdb}')
        for ch in at.get("m_Children", []):
            ct = objs[int(ch["fileID"])][1]
            cg = int(ct["m_GameObject"]["fileID"])
            name = str(objs[cg][1].get("m_Name"))
            cs = comps(cg)
            sr = next((v for t, v in cs if t == "SpriteRenderer"), None)
            rough = next(
                (v for t, v in cs if t == "MonoBehaviour" and "fullSprite" in v), None
            )
            if not sr or not sr.get("m_Sprite", {}).get("guid"):
                continue
            lp, ls = ct["m_LocalPosition"], ct["m_LocalScale"]
            sx, sy = fl(ls["x"]), fl(ls["y"])
            col = sr.get("m_Color") or {}
            rgba = "%02x%02x%02x%02x" % tuple(
                int(max(0, min(1, fl(col.get(c, 1)))) * 255) for c in "rgba"
            )
            cx = cy = w = h = None
            for tag, g in (
                ("R", sr["m_Sprite"]["guid"]),
                ("F", (rough or {}).get("fullSprite", {}).get("guid")),
            ):
                if not g:
                    continue
                sp = B.sprite_asset(g)
                if not sp:
                    continue
                r = B.sprite_image(sp)
                if not r:
                    continue
                im, ox, oy, RW, RH, px, py, ppu = r
                # whole rect (untrimmed) so rough and full line up: paste the trimmed pixels at their offset
                from PIL import Image

                full = Image.new(
                    "RGBA",
                    (max(1, int(round(RW))), max(1, int(round(RH)))),
                    (0, 0, 0, 0),
                )
                full.paste(im, (int(round(ox)), int(round(RH - oy - im.height))))
                ww, hh = RW / ppu * abs(sx), RH / ppu * abs(sy)
                pw, ph = max(1, round(ww * MAP_PPU)), max(1, round(hh * MAP_PPU))
                ss.add_named(
                    f"{tag}:{name}", premul_resize(full, pw, ph), -pw // 2, -ph // 2
                )
                if cx is None:
                    w, h = ww, hh
                    cx = ax + fl(lp["x"]) + (RW / 2 - px) / ppu * sx
                    cy = ay + fl(lp["y"]) + (RH / 2 - py) / ppu * sy
            if cx is None:
                continue
            act = 0 if str(objs[cg][1].get("m_IsActive", 1)) == "0" else 1
            lines.append(
                f"R {ai} {name} {cx:.3f} {cy:.3f} {w:.3f} {h:.3f} {act} {rgba}"
            )
            # the room's map pins (bench, stag, tram, vendors, hot spring, warrior graves...), shown by their FSMs
            for pch in ct.get("m_Children", []):
                pt = objs[int(pch["fileID"])][1]
                pg = int(pt["m_GameObject"]["fileID"])
                pin = pin_line(
                    name,
                    objs[pg][1],
                    comps(pg),
                    pt,
                    ax + fl(lp["x"]),
                    ay + fl(lp["y"]),
                    sx,
                    sy,
                    ss,
                    P,
                )
                if pin:
                    lines.append(pin)
    os.makedirs(f"{romfs}/gfx", exist_ok=True)
    ss.write(f"{romfs}/gfx/map.spr", FMT_RGBA4, 1024)
    open(f"{romfs}/text/map.txt", "w").write("\n".join(lines) + "\n")
    print(len(lines), "map lines")


if __name__ == "__main__":
    main(sys.argv[1])
