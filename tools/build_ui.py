#!/usr/bin/env python3
"""Title screen background, menu pointer animation, Team Cherry logo and bitmap fonts."""

import sys, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

sys.path.insert(0, os.path.dirname(__file__))
from hkcommon import *
import bake_scene as B
import render_view as RV
from build_sprites import SpriteSet
from texpack import premul_resize, FMT_RGBA8, FMT_RGBA4

FONTDIR = f"{EX}/Font"


def sprite_by_name(name):
    sp = B.metamap("Sprite")
    for g, p in sp.items():
        if os.path.basename(p) == name + ".asset":
            return B.sprite_image(B.sprite_asset(g))
    return None


def build_ui(out):
    s = SpriteSet(1.0)
    bg, S = RV.render_view(
        f"{EX}/Scenes/Menu_Title.unity",
        14.6,
        8.4,
        exclude_names=("black_solid", "spatter"),
    )
    bg.save(
        os.path.join(os.path.dirname(out), "..", "..", "docs_title_preview.png")
        if False
        else "/tmp/title_bg.png"
    )
    s.add_named("title_bg", bg, 0, 0)
    frames = []
    from build_sprites import XD, hd_fit

    for i in range(11):
        r = sprite_by_name(f"main_menu_pointer_anim{i:04d}")
        if not r:
            continue
        im = r[0]
        sc = 0.22
        im2 = premul_resize(
            im, max(1, round(im.width * sc)) * XD, max(1, round(im.height * sc))
        )
        im2, ox = hd_fit(im2, -im2.width)
        frames.append(s.add_image(im2, ox, -im2.height // 2))
    s.anims.append(("pointer", 24.0, 2, 0, [(f, 0) for f in frames]))
    r = sprite_by_name("team_cherry_logo_main_menu")
    if r:
        im = r[0]
        sc = 110 / im.width
        s.add_named(
            "team_cherry", premul_resize(im, 110, round(im.height * sc)), 0, 0, dens=1
        )  # (bottom screen)
    # soft light around the Knight (HK's hero light)
    import numpy as np

    R = 96
    yy, xx = np.mgrid[-R:R, -R:R] + 0.5
    d = np.sqrt(xx**2 + yy**2) / R
    a = np.clip(1 - d, 0, 1) ** 2
    img = np.zeros((2 * R, 2 * R, 4), np.uint8)
    img[..., 0] = 176
    img[..., 1] = 195
    img[..., 2] = 214
    img[..., 3] = (a * 255).astype(np.uint8)
    s.add_named(
        "hero_light", Image.fromarray(img, "RGBA"), -R, -R, dens=1
    )  # (a soft gradient: standard in HD too)
    s.write(out)


def build_font(out, ttf, size, glow=False, tracking=0):
    from build_sprites import XD

    font = ImageFont.truetype(ttf, size)
    fontx = ImageFont.truetype(ttf, size * XD) if XD > 1 else font
    asc, desc = font.getmetrics()
    h = asc + desc
    s = SpriteSet(1.0)
    from text_sheets import (
        LATIN1,
    )  # the accented letters and marks the Spanish text uses

    for c in list(range(32, 127)) + [ord(ch) for ch in LATIN1]:
        ch = chr(c)
        adv = max(1, int(round(font.getlength(ch))) + tracking)
        pad = 2 if glow else 0
        im = Image.new("RGBA", ((adv + pad * 2) * XD, h + pad * 2), (255, 255, 255, 0))
        # HD (XD 2): the glyph drawn at twice the size, then squashed back to the standard height: twice the texels
        # across in exactly twice the standard glyph's width, so text measures and wraps as in the standard build
        mask = Image.new("L", ((adv + pad * 2) * XD, (h + pad * 2) * XD), 0)
        ImageDraw.Draw(mask).text((pad * XD, pad * XD), ch, font=fontx, fill=255)
        if glow:
            g = mask.filter(ImageFilter.GaussianBlur(1.5 * XD)).point(
                lambda v: int(v * 0.5)
            )
            mask = Image.fromarray(
                __import__("numpy").maximum(
                    __import__("numpy").asarray(mask), __import__("numpy").asarray(g)
                )
            )
        if XD > 1:
            mask = mask.resize(im.size, Image.BOX)
        im.putalpha(mask)
        s.add_named(f"c{c}", im, -pad * XD, -pad)
    s.write(out, FMT_RGBA4, 512 * XD)


def build_dialog(out):
    """Dialogue box, prompt markers (Listen / Enter / Rest...), yes-no fleurs and area-title fleurs (HUD camera art)."""
    from build_sprites import GO

    s = SpriteSet(0.25)
    s.add_library(f"{GO}/Dialogue Anim.prefab", None, "Dlg ")
    s.add_library(f"{GO}/Area Titles Anim.prefab", None, "Ttl ")
    s.write(out, FMT_RGBA4)


if __name__ == "__main__":
    gfx = sys.argv[1]
    if sys.argv[2:] == ["dialog"]:
        build_dialog(f"{gfx}/dialog.spr")
        sys.exit()
    build_ui(f"{gfx}/ui.spr")
    build_font(
        f"{gfx}/font_menu.spr", f"{FONTDIR}/TrajanPro-Regular.otf", 14, glow=True
    )
    build_font(
        f"{gfx}/font_big.spr",
        f"{FONTDIR}/TrajanPro-Regular.otf",
        26,
        glow=True,
        tracking=1,
    )
    build_font(f"{gfx}/font_text.spr", f"{FONTDIR}/Perpetua.ttf", 14)
