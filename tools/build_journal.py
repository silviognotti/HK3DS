#!/usr/bin/env python3
"""The Hunter's Journal (the Inventory's Journal pane: _GameCameras/HudCamera/Inventory/Journal, JournalList).
   python build_journal.py <romfs>
Writes:
  text/journal_list.txt       one entry per line, in the game's order (JournalList.list):
                                playerDataName <tab> convoName <tab> flags (w = warrior ghost, g = Grimm entry)
                              The entry shows once PlayerData "killed<playerDataName>" is set; "kills<...>" counts down
                              to the Hunter's notes; "newData<...>" marks it new. Texts: NAME_ / DESC_ / NOTE_<convo>.
  gfx/journal_icons.spr       the list icons (the entry prefab's Portrait sprite), named after convoName
  gfx/journal/<convo>.spr     each entry's large portrait (JournalEntryStats.sprite), loaded when it's selected
(named by convoName: two entries share a playerDataName - the game's Hollow Shade entry uses "Crawler")
Portraits come from the game's Bestiary atlases (hs_texmatch) through tex2sprites (sprite guid -> atlas rect).
"""

import sys, os, re, glob, pickle
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_sprites import SpriteSet
from texpack import FMT_RGBA4

HK = os.environ.get("HKDATA", "hkdata")
EX = f"{HK}/ex"
ICON_H = 30  # list icon height on the bottom screen (px)
FULL_H = 112  # large portrait height on the top screen (px)


def prefab_by_guid():
    out = {}
    for m in glob.glob(f"{EX}/GameObject/*.prefab.meta"):
        g = re.search(r"^guid: ([0-9a-f]+)", open(m).read(), re.M)
        if g:
            out[g.group(1)] = m[:-5]
    return out


def journal_order():
    s = open(f"{EX}/GameObject/_GameCameras.prefab", errors="replace").read()
    i = s.index("guid: 98075af954e4922da035fc9784b55620")  # JournalList
    blk = s[i : s.index("listInv", i)]
    return re.findall(r"guid: ([0-9a-f]+), type: 2", blk)


def entry_fields(path):
    s = open(path, errors="replace").read()
    f = {}
    for k in ("playerDataName", "convoName", "warriorGhost", "grimmEntry"):
        m = re.search(rf"^\s*{k}: (.*)$", s, re.M)
        f[k] = m.group(1).strip() if m else ""
    m = re.search(r"^\s*sprite: \{fileID: \d+, guid: ([0-9a-f]+)", s, re.M)
    f["full"] = m.group(1) if m else None
    sprites = re.findall(r"m_Sprite: \{fileID: \d+, guid: ([0-9a-f]+)", s)
    f["icon"] = sprites[0] if sprites else None
    return f


def sprite_lookup():
    by = pickle.load(open(f"{HK}/tex2sprites.pkl", "rb"))
    match = pickle.load(open(f"{HK}/hs_texmatch.pkl", "rb"))
    out = {}
    for tg, L in by.items():
        for name, rect, sg in L:
            if tg in match and rect:
                out[sg] = (match[tg], rect, name)
    return out


_atlas = {}


def crop(info):
    path, (x, y, w, h), name = info
    if path not in _atlas:
        _atlas[path] = Image.open(path).convert("RGBA")
    im = _atlas[path]
    H = im.height
    return im.crop(
        (int(round(x)), int(round(H - y - h)), int(round(x + w)), int(round(H - y)))
    )


def fit(im, h):
    k = h / im.height
    return im.resize((max(1, int(round(im.width * k))), h), Image.LANCZOS)


def main(romfs):
    pf = prefab_by_guid()
    look = sprite_lookup()
    icons = SpriteSet(1.0)
    os.makedirs(f"{romfs}/gfx/journal", exist_ok=True)
    lines = []
    missing = []
    for g in journal_order():
        p = pf.get(g)
        if not p:
            missing.append(("prefab", g))
            continue
        f = entry_fields(p)
        pd, convo = f["playerDataName"], f["convoName"]
        if not pd:
            continue
        flags = ("w" if f["warriorGhost"] == "1" else "") + (
            "g" if f["grimmEntry"] == "1" else ""
        )
        lines.append(f'{pd}\t{convo}\t{flags or "-"}')
        if f["icon"] in look:
            im = fit(crop(look[f["icon"]]), ICON_H)
            icons.add_named(convo, im, -im.width // 2, -im.height // 2)
        else:
            missing.append(("icon", pd))
        if f["full"] in look:
            im = crop(look[f["full"]])
            if im.height > FULL_H or im.width > FULL_H * 1.6:
                k = min(FULL_H / im.height, FULL_H * 1.6 / im.width)
                im = im.resize(
                    (max(1, int(im.width * k)), max(1, int(im.height * k))),
                    Image.LANCZOS,
                )
            ss = SpriteSet(1.0)
            ss.add_named("portrait", im, -im.width // 2, -im.height // 2)
            ss.write(f"{romfs}/gfx/journal/{convo}.spr", FMT_RGBA4, maxsize=256)
        else:
            missing.append(("full", pd))
    icons.write(f"{romfs}/gfx/journal_icons.spr", FMT_RGBA4)
    open(f"{romfs}/text/journal_list.txt", "w").write("\n".join(lines) + "\n")
    print(len(lines), "entries;", "missing:", missing)


if __name__ == "__main__":
    main(sys.argv[1])
