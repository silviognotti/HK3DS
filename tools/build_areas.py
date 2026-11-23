#!/usr/bin/env python3
"""Save-profile backgrounds: the game's area art (Area_*.asset) for each map zone, as romfs/gfx/areas.spr, named by the
zone's display name (as the save file's "Z" line stores it).   python build_areas.py <romfs/gfx>
"""

import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_ui import sprite_by_name
from build_sprites import SpriteSet
from texpack import premul_resize, FMT_RGBA8

ZONES = {
    "King's Pass": "Area_Kings_Pass",
    "Dirtmouth": "Area_Dirtmouth",
    "Forgotten Crossroads": "Area_Forgotten Crossroads",
    "Greenpath": "Area_Green_Path",
    "Fungal Wastes": "Area_Fungal_Wastes",
    "Fog Canyon": "Area_Queen_Station",
    "City of Tears": "Area_Art_City_of_Tears",
    "The Royal Quarter": "Area_Royal_Quarter",
    "Royal Waterways": "Area_Waterways",
    "Crystal Peak": "Area_Crystal_Mines",
    "Resting Grounds": "Area_Resting_Grounds",
    "Deepnest": "Area_Deepnest",
    "Ancient Basin": "Area_Abyss",
    "The Abyss": "Area_Abyss",
    "Howling Cliffs": "Area_Cliffs",
    "The Hive": "Area_Hive",
    "White Palace": "Area_White_Palace",
    "Godhome": "Area_Art_Godshome",
    "Kingdom's Edge": "Area_Kingdoms_Edge",
    "Queen's Gardens": "Area_Royal_Gardens",
    "Hallownest": "Area_Forgotten Crossroads",
}
s = SpriteSet(1.0)
for zone, spr in ZONES.items():
    r = sprite_by_name(spr)
    if not r:
        print("missing", spr)
        continue
    im = r[0]
    w = 318
    h = max(1, round(im.height * w / im.width))
    s.add_named(zone, premul_resize(im, w, h), 0, 0)
    print(zone, spr, im.size, "->", (w, h))
s.write(os.path.join(sys.argv[1], "areas.spr"), FMT_RGBA8, 1024)
