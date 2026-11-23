import os

# Packing + 3DS GPU texture encoding (tiled/morton, vertically flipped like tex3ds).
import struct, numpy as np
from PIL import Image

FMT_RGBA8, FMT_RGB565, FMT_RGBA4, FMT_RGBA5551 = 0, 1, 2, 3


def premul_resize(im, w, h):
    # (in bands of rows: a whole float copy of a huge HD sprite took gigabytes; the result is the same)
    a8 = np.asarray(im.convert("RGBA"))
    pm8 = np.empty_like(a8)
    for r0 in range(0, a8.shape[0], 256):
        a = a8[r0 : r0 + 256].astype(np.float32) / 255.0
        pm8[r0 : r0 + 256] = (
            np.concatenate([a[..., :3] * a[..., 3:4], a[..., 3:4]], -1) * 255 + 0.5
        ).astype(np.uint8)
    pim = Image.fromarray(pm8, "RGBA").resize((max(1, w), max(1, h)), Image.LANCZOS)
    del pm8
    b8 = np.asarray(pim)
    out8 = np.empty_like(b8)
    for r0 in range(0, b8.shape[0], 256):
        b = b8[r0 : r0 + 256].astype(np.float32) / 255.0
        al = b[..., 3:4]
        rgb = np.where(al > 1e-4, b[..., :3] / np.maximum(al, 1e-4), 0)
        out8[r0 : r0 + 256] = (
            np.concatenate([np.clip(rgb, 0, 1), al], -1) * 255 + 0.5
        ).astype(np.uint8)
    return Image.fromarray(out8, "RGBA")


def scale_img(im, s):
    if abs(s - 1) < 1e-6:
        return im.convert("RGBA")
    return premul_resize(im, round(im.width * s), round(im.height * s))


def _morton_lut():
    lut = np.zeros(64, np.int32)
    for y in range(8):
        for x in range(8):
            i = (
                (x & 1)
                | ((y & 1) << 1)
                | ((x & 2) << 1)
                | ((y & 2) << 2)
                | ((x & 4) << 2)
                | ((y & 4) << 3)
            )
            lut[y * 8 + x] = i
    return lut


MORTON = _morton_lut()


def encode(im, fmt=FMT_RGBA8):
    """im: PIL RGBA with power-of-two dims (>=8). Returns bytes in 3DS GPU layout."""
    a = np.asarray(
        im.convert("RGBA")
    )  # top row first, like tex3ds (memory row 0 is sampled at v = 1)
    h, w, _ = a.shape
    if fmt == FMT_RGBA8:
        px = np.stack([a[..., 3], a[..., 2], a[..., 1], a[..., 0]], -1).astype(
            np.uint8
        )  # A B G R bytes
        bpp = 4
    else:
        r, g, b, al = [a[..., i].astype(np.uint16) for i in range(4)]
        if fmt == FMT_RGB565:
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        elif fmt == FMT_RGBA4:
            v = ((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (al >> 4)
        else:
            v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (al >> 7)
        px = v.astype("<u2").view(np.uint8).reshape(h, w, 2)
        bpp = 2
    # tiles
    t = (
        px.reshape(h // 8, 8, w // 8, 8, bpp)
        .transpose(0, 2, 1, 3, 4)
        .reshape(h // 8, w // 8, 64, bpp)
    )
    out = np.empty_like(t)
    out[:, :, MORTON, :] = t
    return out.tobytes()


def pot(n, lo=8):
    p = lo
    while p < n:
        p *= 2
    return p


class ShelfPacker:
    def __init__(self, w, h, pad=1):
        self.w, self.h, self.pad = w, h, pad
        self.x = self.y = self.rowh = 0

    def add(self, iw, ih):
        iw += self.pad
        ih += self.pad
        if iw > self.w or ih > self.h:
            return None
        if self.x + iw > self.w:
            self.x = 0
            self.y += self.rowh
            self.rowh = 0
        if self.y + ih > self.h:
            return None
        p = (self.x, self.y)
        self.x += iw
        self.rowh = max(self.rowh, ih)
        return p


def pack(images, maxsize=1024):
    """images: list of PIL RGBA. Returns pages (list of PIL) and placements [(page,x,y)]."""
    order = sorted(range(len(images)), key=lambda i: -images[i].height)
    pages, packers, place = [], [], [None] * len(images)
    for i in order:
        im = images[i]
        for pi, pk in enumerate(packers):
            p = pk.add(im.width, im.height)
            if p:
                place[i] = (pi, *p)
                break
        else:
            pk = ShelfPacker(maxsize, maxsize)
            packers.append(pk)
            p = pk.add(im.width, im.height)
            assert p, ("image too large", im.size)
            place[i] = (len(packers) - 1, *p)
    # build pages, shrink to used extents
    used = {}
    for i, (pi, x, y) in enumerate(place):
        uw, uh = used.get(pi, (0, 0))
        used[pi] = (max(uw, x + images[i].width), max(uh, y + images[i].height))
    for pi in range(len(packers)):
        uw, uh = used[pi]
        pg = Image.new("RGBA", (pot(uw), pot(uh)), (0, 0, 0, 0))
        pages.append(pg)
    for i, (pi, x, y) in enumerate(place):
        pages[pi].paste(images[i], (x, y))
    return pages, place


def rle(data, unit):
    """RLE over 2- or 4-byte units: u16 header (0x8000|n = run of next value, n = n literals)."""
    v = np.frombuffer(data, "<u2" if unit == 2 else "<u4")
    n = len(v)
    out = []
    change = np.nonzero(np.diff(v))[0] + 1
    starts = np.concatenate([[0], change]).astype(int)
    ends = np.concatenate([change, [n]]).astype(int)
    lit_s = None

    def flush(a, b):
        while a < b:
            c = min(b - a, 0x7FFF)
            out.append(struct.pack("<H", c))
            out.append(v[a : a + c].tobytes())
            a += c

    for s_, e_ in zip(starts, ends):
        L = e_ - s_
        if L >= 3:
            if lit_s is not None:
                flush(lit_s, s_)
                lit_s = None
            while L > 0:
                c = min(L, 0x7FFF)
                out.append(struct.pack("<H", 0x8000 | c))
                out.append(v[s_ : s_ + 1].tobytes())
                L -= c
        elif lit_s is None:
            lit_s = s_
    if lit_s is not None:
        flush(lit_s, n)
    return b"".join(out)


def write_page(fp, page, fmt, dens=None):
    """dens: texels across per screen pixel (2: HD sheets, HK_XD), stored in the page header's spare byte"""
    if dens is None:
        dens = int(os.environ.get("HK_XD", "1") or 1)
    dens = 0 if dens <= 1 else dens
    data = encode(page, fmt)
    unit = 4 if fmt == FMT_RGBA8 else 2
    packed = rle(data, unit)
    if len(packed) < len(data) * 0.9:
        fp.write(
            struct.pack(
                "<HHBBxxI", page.width, page.height, fmt | 0x80, dens, len(packed)
            )
        )
        fp.write(packed)
    else:
        fp.write(struct.pack("<HHBBxxI", page.width, page.height, fmt, dens, len(data)))
        fp.write(data)
