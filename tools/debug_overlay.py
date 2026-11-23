# Debug view: draw a room's collision grid and entity boxes over its baked preview image.
# usage: debug_overlay.py <romfs_room_file.room> <preview.png> <out.png> [x0 x1 y0 y1  (world-unit crop)] [scale]
import sys, struct, zlib
import numpy as np
from PIL import Image, ImageDraw

KINDS = [
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
COL = dict(
    solid=(255, 60, 60),
    platform=(255, 160, 0),
    breakable_wall=(255, 0, 255),
    collapse_floor=(255, 255, 0),
    gate_door=(0, 255, 255),
    spikes=(255, 0, 0),
    stalactite=(200, 100, 255),
    enemy=(0, 255, 0),
    gate=(0, 160, 255),
    geo_rock=(120, 200, 255),
    chest=(255, 220, 80),
    bench=(255, 255, 255),
)


def load_room(p):
    d = open(p, "rb").read()
    if d[:4] == b"HKZ1":
        d = zlib.decompress(d[8:])
    assert d[:4] == b"HKRM"
    ver, W, H, n, sx, sy = struct.unpack_from("<HHHHff", d, 4)
    o = 4 + 16
    o += 4 * 32
    res, _ = struct.unpack_from("<HH", d, o)
    o += 4
    cw, ch = W * res, H * res
    rowb = (cw + 7) // 8
    bits = np.frombuffer(d, np.uint8, rowb * ch, o)
    o += rowb * ch
    grid = np.unpackbits(bits.reshape(ch, rowb), axis=1, bitorder="little")[
        :, :cw
    ]  # row 0 = bottom
    ents = []
    fmt = "<BBHffffhh32s32s96s"
    for _ in range(n):
        k, ai, _, x, y, w, h, hp, geo, nm, tg, ex = struct.unpack_from(fmt, d, o)
        o += struct.calcsize(fmt)
        ents.append(
            dict(
                kind=KINDS[k],
                ai=ai,
                x=x,
                y=y,
                w=w,
                h=h,
                name=nm.split(b"\0")[0].decode(errors="replace"),
                target=tg.split(b"\0")[0].decode(errors="replace"),
                extra=ex.split(b"\0")[0].decode(errors="replace"),
            )
        )
    return W, H, res, grid, ents


def main():
    room, prev, out = sys.argv[1:4]
    W, H, res, grid, ents = load_room(room)
    crop = list(map(float, sys.argv[4:8])) if len(sys.argv) >= 8 else [0, W, 0, H]
    S = int(sys.argv[8]) if len(sys.argv) >= 9 else 16
    im = Image.open(prev).convert("RGBA")
    ppu = im.width / W
    im = im.resize((int(W * S), int(H * S)), Image.NEAREST)
    ov = Image.new("RGBA", im.size, (0, 0, 0, 0))
    dr = ImageDraw.Draw(ov)
    cs = S / res
    ys, xs = np.nonzero(grid)
    for cy, cx in zip(ys, xs):
        X0 = cx * cs
        Y1 = (H * res - cy) * cs
        dr.rectangle([X0, Y1 - cs, X0 + cs - 1, Y1 - 1], fill=(255, 40, 40, 90))
    for e in ents:
        c = COL.get(e["kind"], (200, 200, 200))
        x0 = (e["x"] - e["w"] / 2) * S
        x1 = (e["x"] + e["w"] / 2) * S
        y0 = (H - (e["y"] + e["h"] / 2)) * S
        y1 = (H - (e["y"] - e["h"] / 2)) * S
        if e["w"] == 0:
            dr.ellipse(
                [
                    e["x"] * S - 3,
                    (H - e["y"]) * S - 3,
                    e["x"] * S + 3,
                    (H - e["y"]) * S + 3,
                ],
                outline=c + (255,),
            )
        else:
            dr.rectangle([x0, y0, x1, y1], outline=c + (255,), width=2)
        if e["kind"] in (
            "enemy",
            "gate",
            "breakable_wall",
            "collapse_floor",
            "spikes",
            "stalactite",
        ):
            dr.text(
                (x0 + 2, y0 + 2), e["kind"][:10] + " " + e["name"][:16], fill=c + (255,)
            )
    im = Image.alpha_composite(im, ov)
    x0, x1, y0, y1 = crop
    im = im.crop((int(x0 * S), int((H - y1) * S), int(x1 * S), int((H - y0) * S)))
    im.convert("RGB").save(out)
    print(out, im.size)


if __name__ == "__main__":
    main()
