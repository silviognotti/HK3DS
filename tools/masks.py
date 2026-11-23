#!/usr/bin/env python3
"""Secret masks: the black covers Hollow Knight puts over hidden areas and fades out (an 'unmasker'-style FSM:
iTweenFadeTo on itself + a Player trigger) when the Knight walks in.
   python masks.py <romfs/rooms> Room [Room ...]
Writes <Room>.msk + <Room>_msk.spr:
   G gid x0 y0 x1 y1 persistent name          trigger bounds of one mask group
   S gid frame x y F dx dy alpha              one sprite: world pivot (x,y), parallax F, image offset (baked px)
Also usable from bake_scene (mask_paths) so the masks aren't baked into the static layers.
"""

import sys, os, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fsm_extract as F
import build_room as BR
import bake_scene as B
from build_sprites import SpriteSet

MS = 4.0  # mask image resolution: px per world unit (they're soft black shapes)


def mask_roots(scene_path):
    """[(gid, path, persistent)] for objects whose FSM fades them out when the Player touches them."""
    objs = F.load_objs(scene_path)
    P = F.Paths(objs)
    comps = {}
    for k, (t, v) in objs.items():
        if isinstance(v, dict) and isinstance(v.get("m_GameObject"), dict):
            comps.setdefault(int(v["m_GameObject"]["fileID"]), []).append((t, v))
    out = []
    for k, (t, v) in objs.items():
        if t != "MonoBehaviour" or not isinstance(v.get("fsm"), dict):
            continue
        try:
            f = F.decode_fsm(v, P, "")
        except Exception:
            continue
        types = [a["type"].split(".")[-1] for s in f["states"] for a in s["actions"]]
        if "iTweenFadeTo" not in types or not any(
            x in types for x in ("Trigger2dEvent", "Trigger2dEventLayer")
        ):
            continue
        fades_self = any(
            a["type"].endswith("iTweenFadeTo")
            and (a["params"].get("gameObject") or {}).get("owner")
            for s in f["states"]
            for a in s["actions"]
        )
        if not fades_self:
            continue
        gid = int(v["m_GameObject"]["fileID"])
        pers = any(
            tt == "MonoBehaviour" and "persistentBoolData" in vv
            for tt, vv in comps.get(gid, [])
        )
        MODES[gid] = fsm_mode(f)
        out.append((gid, P.path(gid), pers))
    return out


MODES = (
    {}
)  # gid -> (mode, fade time, listens for HIT): mode 0 unmasker (fades out for good), 1 remasker (covers again


# when he leaves), 2 inverse (shown only while he's inside). HIT comes from a Breakable's hitEventReciever
def fsm_mode(f):
    st = {s["name"]: s for s in f["states"]}
    hit = any(
        t.get("event") == "HIT" for s in f["states"] for t in s.get("transitions") or []
    )
    u = None
    for s in f["states"]:
        for t in s.get("transitions") or []:
            if t.get("event") in ("UNCOVER", "HIT") and t.get("to") in st and u is None:
                u = st[t["to"]]
    mode, ft = 0, 0.5
    if u:
        own = [
            a
            for a in u["actions"]
            if a["type"].endswith("iTweenFadeTo")
            and (a["params"].get("gameObject") or {}).get("owner")
        ]
        if own:
            if float(own[0]["params"].get("alpha") or 0) > 0.5:
                mode = 2
            tm = own[0]["params"].get("time")
            if isinstance(tm, (int, float)) and tm > 0.05:
                ft = float(tm)
    if mode == 0 and any(
        a["type"].endswith(("Trigger2dEvent", "Trigger2dEventLayer"))
        and a["params"].get("trigger") == 2
        and ((a["params"].get("sendEvent") or {}).get("event") == "COVER")
        for s in f["states"]
        for a in s["actions"]
    ):
        mode = 1
    return mode, ft, hit


def add_pieces(ss, img, left, top, maxw=1000):
    """[(frame, left, top)]: a mask wider or taller than a sheet page goes in as several pieces."""
    out = []
    for y in range(0, img.height, maxw):
        for x in range(0, img.width, maxw):
            pc = img.crop((x, y, min(img.width, x + maxw), min(img.height, y + maxw)))
            if pc.getbbox():
                out.append((ss.add_image(pc, 0, 0), left + x, top + y))
    return out


def hit_linked(out_dir, room, _cache={}):
    """Scene objects a Breakable of the room's runtime objects sends HIT to (hitEventReciever {"ext": fileID})."""
    if room in _cache:
        return _cache[room]
    out = set()
    try:
        import zlib

        d = open(f"{out_dir}/{room}.obj", "rb").read()
        if d[:4] == b"HKZ1":
            d = zlib.decompress(d[8:])

        def walk(x):
            if isinstance(x, dict):
                if x.get("cls") == "Breakable":
                    r = (x.get("f") or {}).get("hitEventReciever")
                    if isinstance(r, dict) and "ext" in r:
                        out.add(int(r["ext"]))
                for v in x.values():
                    walk(v)
            elif isinstance(x, list):
                for v in x:
                    walk(v)

        walk(json.loads(d))
    except Exception:
        pass
    _cache[room] = out
    return out


def export(room, out_dir):
    sp = BR.scene_path(room)
    if not sp:
        for ext in (".msk", "_msk.spr"):
            path = f"{out_dir}/{room}{ext}"
            if os.path.exists(path):
                os.remove(path)
        return 0
    if sp:
        try:
            scene_text = open(sp, "rb").read(2_000_000).decode("utf-8", "replace")
            if "iTweenFadeTo" not in scene_text and "msk" not in scene_text:
                for ext in (".msk", "_msk.spr"):
                    path = f"{out_dir}/{room}{ext}"
                    if os.path.exists(path):
                        os.remove(path)
                return 0
        except OSError:
            pass
    roots = mask_roots(sp)
    _, items = B.collect(sp)
    # other masks (e.g. 'break_wall_masks' faded by a breakable wall's FSM): msk_* sprites outside unmasker groups.
    # They uncover when the Knight gets into the area they cover.
    extra = {}
    for it in items:
        nm = str(it["sprite"].get("m_Name") or "")
        if not nm.startswith("msk"):
            continue
        if any(
            it["path"] == p or it["path"].startswith(p + "/") for g, p, pers in roots
        ):
            continue  # handled with its group below
        parts = it["path"].split("/")
        k = next(
            (i for i, n in enumerate(parts) if "mask" in n.lower()), len(parts) - 1
        )
        extra.setdefault("/".join(parts[: k + 1]), []).append(it)
    if not roots and not extra:
        for ext in (".msk", "_msk.spr"):
            if os.path.exists(f"{out_dir}/{room}{ext}"):
                os.remove(f"{out_dir}/{room}{ext}")
        return 0
    sc = BR.X.Scene(BR.X.load_scene(sp))
    cols = {}
    for c in BR.X.colliders(sc):
        if c["active"]:
            cols.setdefault(c["gid"], []).append(c)
    ss = SpriteSet(1.0)
    lines = []
    used = set()
    # groups with a trigger collider first, so a mask nested in another mask's group is drawn once
    order = sorted(
        range(len(roots)),
        key=lambda i: (
            0 if cols.get(roots[i][0]) or MODES.get(roots[i][0], (0, 0, 0))[2] else 1
        ),
    )
    roots = [roots[i] for i in order]
    nextg = [
        len(roots)
    ]  # ids after the roots: inverse-mask halves, then the loose masks
    for gi, (gid, path, pers) in enumerate(roots):
        cl = cols.get(gid) or []
        mode, ft, hit = MODES.get(gid, (0, 0.5, False))
        if (
            not cl and not hit
        ):  # trigger on a child or elsewhere: uncover when the Knight reaches the covered area
            extra.setdefault(path, []).extend(
                it
                for it in items
                if (it["path"] == path or it["path"].startswith(path + "/"))
                and id(it) not in used
            )
            continue
        # trigger shapes: boxes, and polygons (an AABB would be far too big for the L-shaped ones)
        polys = [c for c in cl if c["type"] == "PolygonCollider2D" and c.get("paths")]
        bxs = [c for c in cl if c not in polys]
        b = (
            bxs[0]["box"] if bxs else (0, 0, -1, -1)
        )  # no trigger box: polygons, or only a breakable's HIT uncovers it
        nm = path.split("/")[-1].replace(" ", "_")
        gline = len(
            lines
        )  # (a HIT-only mask gets its covered area as a fallback trigger below)
        lines.append(
            "G %d %.3f %.3f %.3f %.3f %d %s %d %.2f"
            % (gi, b[0], b[1], b[2], b[3], 1 if pers else 0, nm, mode, ft)
        )

        def shapes(g):
            for c in bxs[1:4]:
                lines.append("B %d %.3f %.3f %.3f %.3f" % ((g,) + tuple(c["box"])))
            for c in polys[:2]:
                for pth in c["paths"][:1]:
                    pts = pth[:24]
                    lines.append(
                        "P %d %d " % (g, len(pts))
                        + " ".join("%.3f %.3f" % (x, y) for x, y in pts)
                    )

        shapes(gi)
        if hit:
            lines.append("E %d %d" % (gi, gid))
        inv = path + "/Inverse Mask"
        if mode == 2 and any(
            it["path"] == inv or it["path"].startswith(inv + "/") for it in items
        ):
            ig = nextg[0]
            nextg[0] += 1
            lines.append(
                "G %d %.3f %.3f %.3f %.3f 0 %s_inv 1 %.2f"
                % (ig, b[0], b[1], b[2], b[3], nm, ft)
            )
            shapes(ig)
        else:
            ig = gi
        core = []
        for it in items:
            if (
                not (it["path"] == path or it["path"].startswith(path + "/"))
                or id(it) in used
            ):
                continue
            used.add(id(it))
            g_of = ig if (it["path"] == inv or it["path"].startswith(inv + "/")) else gi
            r = B.render_item(it, MS, 1.0)
            if not r:
                continue
            img, left, top = r
            bb = img.getbbox()
            if not bb:
                continue
            if not cl:
                a = __import__("numpy").asarray(img)[..., 3]
                ys, xs = (a > 200).nonzero()
                if len(xs):
                    core.append(
                        (
                            (left + xs.min()) / MS,
                            -(top + ys.max()) / MS,
                            (left + xs.max()) / MS,
                            -(top + ys.min()) / MS,
                        )
                    )
            img = img.crop(bb)
            left += bb[0]
            top += bb[1]
            W = it["W"]
            Fp = B.CAM_D / max(1.0, B.CAM_D + W["z"])
            for fi, pl, pt in add_pieces(ss, img, left, top):
                lines.append(
                    "S %d %d %.3f %.3f %.4f %.1f %.1f %.3f"
                    % (
                        g_of,
                        fi,
                        W["x"],
                        W["y"],
                        Fp,
                        pl - W["x"] * MS,
                        pt + W["y"] * MS,
                        1.0,
                    )
                )
        if (
            not cl and core and gid not in hit_linked(out_dir, room)
        ):  # HIT from a script the port doesn't link: reaching the area uncovers it
            x0 = min(c[0] for c in core)
            y0 = min(c[1] for c in core)
            x1 = max(c[2] for c in core)
            y1 = max(c[3] for c in core)
            lines[gline] = "G %d %.3f %.3f %.3f %.3f %d %s %d %.2f" % (
                gi,
                x0,
                y0,
                x1,
                y1,
                1 if pers else 0,
                nm,
                mode,
                ft,
            )
    gi = nextg[0]
    rootinfo = {p: (g, pers) for g, p, pers in roots}
    for path, its in extra.items():
        boxes = []
        rg, rpers = rootinfo.get(path, (None, True))
        mode, ft, hit = (
            MODES.get(rg, (0, 0.5, False)) if rg is not None else (0, 0.5, False)
        )
        # a plain black shape nothing fades (no 'mask' group, no script above it) is scenery: always shown (mode 3)
        static = (
            rg is None
            and not any("mask" in n.lower() for n in path.split("/"))
            and not any(it.get("fsmd") for it in its)
        )
        its = [it for it in its if id(it) not in used]
        for it in its:
            used.add(id(it))
            r = B.render_item(it, MS, 1.0)
            if not r:
                continue
            img, left, top = r
            bb = img.getbbox()
            if not bb:
                continue
            # opaque core of the mask in world units (at the gameplay plane)
            a = __import__("numpy").asarray(img)[..., 3]
            ys, xs = (a > 200).nonzero()
            if len(xs) and (
                rg is not None
                or str(it["sprite"].get("m_Name") or "").startswith("msk")
            ):
                boxes.append(
                    (
                        (left + xs.min()) / MS,
                        -(top + ys.max()) / MS,
                        (left + xs.max()) / MS,
                        -(top + ys.min()) / MS,
                    )
                )
            img = img.crop(bb)
            left += bb[0]
            top += bb[1]
            W = it["W"]
            Fp = B.CAM_D / max(1.0, B.CAM_D + W["z"])
            for fi, pl, pt in add_pieces(ss, img, left, top):
                lines.append(
                    "S %d %d %.3f %.3f %.4f %.1f %.1f %.3f"
                    % (
                        gi,
                        fi,
                        W["x"],
                        W["y"],
                        Fp,
                        pl - W["x"] * MS,
                        pt + W["y"] * MS,
                        1.0,
                    )
                )
        if static:
            lines.append(
                "G %d 0 0 -1 -1 0 %s 3 0.50"
                % (gi, path.split("/")[-1].replace(" ", "_"))
            )
        elif boxes:
            x0 = min(b[0] for b in boxes)
            y0 = min(b[1] for b in boxes)
            x1 = max(b[2] for b in boxes)
            y1 = max(b[3] for b in boxes)
            lines.append(
                "G %d %.3f %.3f %.3f %.3f %d %s %d %.2f"
                % (
                    gi,
                    x0,
                    y0,
                    x1,
                    y1,
                    1 if rpers else 0,
                    path.split("/")[-1].replace(" ", "_"),
                    mode,
                    ft,
                )
            )
        gi += 1
    if not ss.frames:
        return 0
    ss.write(f"{out_dir}/{room}_msk.spr")
    open(f"{out_dir}/{room}.msk", "w").write("M %.1f\n" % MS + "\n".join(lines) + "\n")
    return gi


if __name__ == "__main__":
    out_dir = sys.argv[1]
    for room in sys.argv[2:]:
        n = export(room, out_dir)
        print(room, n, "mask groups", flush=True)
