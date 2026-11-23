"""Render a perspective camera view of a scene's SpriteRenderers to a flat image (used for the title screen)."""

import sys, os, math
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import bake_scene as B
from hkcommon import *


def render_view(
    scene_path,
    cx,
    cy,
    W=400,
    H=240,
    include=None,
    exclude_names=(),
    cam_d=B.CAM_D,
    fov=24.0,
):
    sc, items = B.collect(scene_path)
    items.sort(key=lambda it: (it["layer"], it["order"], -it["W"]["z"]))
    view_h = 2 * cam_d * math.tan(math.radians(fov / 2))
    S = H / view_h
    XD = B.XD
    WX = W * XD  # HD (HK_XD 2): twice the columns, same view
    canv = {0: np.zeros((H, WX, 4), np.float32)}
    for it in items:
        if include and not any(it["path"].startswith(p) for p in include):
            continue
        if any(x in it["path"] for x in exclude_names):
            continue
        z = it["W"]["z"]
        fpar = cam_d / max(0.5, cam_d + z)
        # render in "layer space" with F = fpar so position is exact for this camera
        r = B.render_item(it, S, fpar)
        if r is None:
            continue
        img, left, top = r
        # layer space origin: world (cx,cy) maps to screen centre
        ox = cx * fpar * S * XD - WX / 2
        oy = -cy * fpar * S - H / 2
        B.composite(canv, 0, img, left, top, it["add"], (ox, oy))
    cv = canv[0]
    al = np.clip(cv[..., 3:4], 0, 1)
    rgb = np.where(al > 1e-4, cv[..., :3] / np.maximum(al, 1e-4), 0)
    out = np.concatenate([rgb * al, np.ones_like(al)], -1)  # composite over black
    return Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA"), S


if __name__ == "__main__":
    im, S = render_view(
        EX + "/Scenes/Menu_Title.unity", float(sys.argv[1]), float(sys.argv[2])
    )
    im.save(sys.argv[3])
    print("S", S)
