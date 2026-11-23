#!/usr/bin/env python3
"""Build HKSP sprite sets (packed 3DS textures + frames + animations).

HKSP format (little endian):
  'HKSP' u16 npages u16 nframes u16 nanims u16 nnames
  pages:  u16 w, u16 h, u8 fmt, u8 0, u16 0, u32 size, data[size]
  frames: u16 page, u16 x, u16 y, u16 w, u16 h, s16 ox, s16 oy, u16 0   (ox,oy: top-left relative to pivot, px, y down)
  anims:  char name[32], f32 fps, u8 wrap, u8 loopStart, u16 first, u16 count, u16 0
  animframes: u16 frame, u16 flags (bit0 = trigger event)
  names:  char name[32], u16 frame, u16 0      (named single images, e.g. menu text)
"""

import sys, os, struct, math
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
from tk2d import *
from texpack import pack, write_page, scale_img, FMT_RGBA8, FMT_RGBA4

TK2D_TEXEL = 1 / 64.0  # tk2d collections in HK are authored at 64 px/unit
# HD sheets (HK_XD=2): twice the texels across, for the 800x240 top screen. Frame widths and offsets are kept even so
# the game can size them in screen pixels (texels / 2) exactly; the pages carry the density in their header byte
XD = int(os.environ.get("HK_XD", "1") or 1)


def hd_fit(im, ox):
    """an HD frame: an even width (a transparent column on the right) and an even left offset (one column of
    transparency on the left when it's odd)"""
    if XD == 1:
        return im, ox
    if ox % 2:
        pad = Image.new("RGBA", (im.width + 1, im.height), (0, 0, 0, 0))
        pad.paste(im, (1, 0))
        im = pad
        ox -= 1
    if im.width % 2:
        pad = Image.new("RGBA", (im.width + 1, im.height), (0, 0, 0, 0))
        pad.paste(im, (0, 0))
        im = pad
    return im, ox


class SpriteSet:
    def __init__(self, scale):
        self.scale = scale  # output px per source px
        self.frames = []  # (PIL, ox, oy)
        self.key2frame = {}
        self.anims = []  # (name, fps, wrap, loopStart, [(frame, flags)])
        self.names = []  # (name, frame)
        self.frame_dens = {}  # frame -> density, where it differs from the sheet's
        self._tex = {}

    def tex(self, path):
        if path not in self._tex:
            self._tex[path] = Image.open(path).convert("RGBA")
        return self._tex[path]

    def add_image(self, im, ox, oy, key=None, dens=0):
        """dens: this frame's texels across per screen pixel when it isn't the sheet's (1: a standard frame in an HD sheet)"""
        if key is not None and key in self.key2frame:
            return self.key2frame[key]
        self.frames.append((im, ox, oy))
        if dens:
            self.frame_dens[len(self.frames) - 1] = dens
        idx = len(self.frames) - 1
        if key is not None:
            self.key2frame[key] = idx
        return idx

    def add_tk2d_frame(self, coll_guid, sid, scale=None):
        key = (coll_guid, sid, scale)
        if key in self.key2frame:
            return self.key2frame[key]
        coll = collection_by_guid(coll_guid)
        if not coll or sid >= len(
            coll["sprites"]
        ):  # a few clips point past their collection's sprites
            return self.add_image(Image.new("RGBA", (1, 1)), 0, 0, key)
        spr = coll["sprites"][sid]
        tp = collection_texture(coll, spr["mat"])
        if not tp or not spr["uvs"]:
            im = Image.new("RGBA", (1, 1))
            return self.add_image(im, 0, 0, key)
        im, x0, y1, w, h = render_sprite(self.tex(tp), spr)
        s = scale or self.scale
        # positions are in units; texel = 1/64 unit -> output px per unit = 64*s
        ppu_out = s / TK2D_TEXEL
        tw, th = max(1, round(w * ppu_out * XD)), max(1, round(h * ppu_out))
        from texpack import premul_resize

        im2 = premul_resize(im, tw, th)
        ox = round(x0 * ppu_out * XD)
        oy = round(-y1 * ppu_out)
        im2, ox = hd_fit(im2, ox)
        return self.add_image(im2, ox, oy, key)

    def add_library(self, lib_prefab, clips=None, prefix="", scale=None):
        lib = load_library(lib_prefab)
        for name, c in lib.items():
            if name is None or (clips is not None and name not in clips):
                continue
            fr = [
                (self.add_tk2d_frame(g, sid, scale), trig)
                for g, sid, trig in c["frames"]
            ]
            self.anims.append((prefix + name, c["fps"], c["wrap"], c["loopStart"], fr))

    def add_named(self, name, im, ox=0, oy=0, dens=0):
        self.names.append((name, self.add_image(im, ox, oy, dens=dens)))

    def write(self, path, fmt=FMT_RGBA8, maxsize=1024):
        imgs = [f[0] for f in self.frames]
        pages, place = pack(imgs, maxsize)
        with open(path, "wb") as fp:
            fp.write(b"HKSP")
            fp.write(
                struct.pack(
                    "<HHHH",
                    len(pages),
                    len(self.frames),
                    len(self.anims),
                    len(self.names),
                )
            )
            for pg in pages:
                write_page(fp, pg, fmt)
            for fi, ((im, ox, oy), (pi, x, y)) in enumerate(zip(self.frames, place)):
                fp.write(
                    struct.pack(
                        "<HHHHHhhH",
                        pi,
                        x,
                        y,
                        im.width,
                        im.height,
                        ox,
                        oy,
                        self.frame_dens.get(fi, 0),
                    )
                )
            allf = []
            for name, fps, wrap, ls, fr in self.anims:
                fp.write(
                    struct.pack(
                        "<32sfBBHHH",
                        name.encode()[:31],
                        fps,
                        wrap,
                        ls,
                        len(allf),
                        len(fr),
                        0,
                    )
                )
                allf += fr
            for fi, trig in allf:
                fp.write(struct.pack("<HH", fi, trig))
            for name, fi in self.names:
                fp.write(struct.pack("<32sHH", name.encode()[:31], fi, 0))
        print(
            f"wrote {path}: {len(pages)} pages {[p.size for p in pages]}, {len(self.frames)} frames, {len(self.anims)} anims, {len(self.names)} names, {os.path.getsize(path)//1024} KB"
        )
        return pages


GO = f"{EX}/GameObject"

KNIGHT_CLIPS = [
    "Idle",
    "Run",
    "Run To Idle",
    "Turn",
    "Airborne",
    "Land",
    "HardLand",
    "Slash",
    "SlashAlt",
    "UpSlash",
    "DownSlash",
    "SlashEffect",
    "SlashEffectAlt",
    "UpSlashEffect",
    "DownSlashEffect",
    "Dash",
    "Dash To Idle",
    "Dash Effect",
    "Recoil",
    "Death",
    "Focus",
    "Focus Get",
    "Focus Get Once",
    "Focus End",
    "LookUp",
    "LookDown",
    "LookUpEnd",
    "LookDownEnd",
    "Idle Hurt",
    "Hazard Respawn",
    "Wake Up Ground",
    "Prostrate",
    "Prostrate Rise",
    "Sit",
    "Sit Idle",
    "Get Off",
    "Walk",
    "Enter",
    "Fall",
    "Spike Death",
    "Stun",
    "Collect Normal 1",
    "Collect Normal 2",
    "Collect Normal 3",
    "Map Idle",
    "Wall Slide",
    "Walljump",
    "Double Jump",
    "Sprint",
    "Respawn Wake",
    "Exit",
]


def build_knight(out):
    s = SpriteSet(0.25)
    s.add_library(
        f"{GO}/Knight_1.prefab", None
    )  # every clip: the Knight's FSMs (spells, superdash, dream nail...) play them
    s.write(out)


def build_enemies(out):
    s = SpriteSet(0.25)
    s.add_library(f"{GO}/Crawler.prefab", None, "Crawler ")
    s.add_library(f"{GO}/Buzzer_0.prefab", None, "Buzzer ")
    s.add_library(
        f"{GO}/Geo_0.prefab",
        ["Small Idle", "Small Air", "Med Idle", "Med Air", "Get"],
        "Geo ",
    )
    s.add_library(f"{GO}/Geo Rocks Green.prefab", None, "GeoRock ")
    s.add_library(f"{GO}/Chest Anim.prefab", None, "Chest ")
    s.add_library(
        f"{GO}/Hollow Shade_0.prefab",
        ["Idle", "Fly", "Slash Antic", "Slash", "Death", "Startle", "Appear"],
        "Shade ",
    )
    s.write(out)


def build_hud(out):
    s = SpriteSet(0.25)
    s.add_library(
        f"{GO}/HUD Anim.prefab",
        [
            "Health Idle",
            "Health Empty",
            "Health Break",
            "Health Refill",
            "Health Appear",
            "Coin Idle",
            "Coin Get",
            "Soul Orb Glow",
            "HUD Frame",
            "V_Empty",
            "V_Quarter",
            "V_Half",
            "V_3Quarter",
            "V_Full",
            "Soul Burst",
        ],
        "",
        0.25,
    )
    s.write(out)


if __name__ == "__main__":
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    which = sys.argv[2:] or ["knight", "enemies", "hud"]
    if "knight" in which:
        build_knight(f"{out}/knight.spr")
    if "enemies" in which:
        build_enemies(f"{out}/enemies.spr")
    if "hud" in which:
        build_hud(f"{out}/hud.spr")
