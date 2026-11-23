# Plot reachable standing positions (from HK_REACH_DUMP) over a room's collision map.
import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw
from debug_overlay import load_room

room, dump, out = sys.argv[1:4]
W, H, res, grid, ents = load_room(f"romfs/rooms/{room}.room")
S = 6
im = Image.new("RGB", (W * S, H * S), (20, 20, 30))
d = ImageDraw.Draw(im)
ys, xs = np.nonzero(grid)
for cy, cx in zip(ys, xs):
    d.rectangle(
        [
            cx * S / res,
            (H * res - cy - 1) * S / res,
            (cx + 1) * S / res - 1,
            (H * res - cy) * S / res - 1,
        ],
        fill=(110, 40, 40),
    )
for e in ents:
    if e["kind"] in ("gate", "breakable_wall", "collapse_floor", "spikes"):
        c = dict(
            gate=(0, 200, 255),
            breakable_wall=(255, 0, 255),
            collapse_floor=(255, 255, 0),
            spikes=(255, 60, 60),
        )[e["kind"]]
        d.rectangle(
            [
                (e["x"] - e["w"] / 2) * S,
                (H - e["y"] - e["h"] / 2) * S,
                (e["x"] + e["w"] / 2) * S,
                (H - e["y"] + e["h"] / 2) * S,
            ],
            outline=c,
            width=2,
        )
        if e["kind"] == "gate":
            d.text(
                ((e["x"] - e["w"] / 2) * S + 2, (H - e["y"] - e["h"] / 2) * S + 2),
                e["name"],
                fill=c,
            )
for line in open(dump):
    x, y = map(float, line.split())
    d.ellipse(
        [x * S - 2, (H - y) * S - 2, x * S + 2, (H - y) * S + 2], fill=(80, 255, 80)
    )
im.save(out)
print(out, im.size)
