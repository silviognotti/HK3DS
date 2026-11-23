"""A scene's colour correction: the SceneManager's saturation and red/green/blue curves, which the game's camera
applies to the whole picture (ColorCorrectionCurves). Applied to the baked art, so each area keeps its colour cast
(Greenpath's teal, Crystal Peak's violet...)."""

import re, sys, numpy as np


def scene_grade(scene_path):
    t = open(scene_path, errors="ignore").read()
    i = t.find("\n  saturation:")
    if i < 0:
        return None
    blk = t[i : i + 12000]
    sat = float(re.search(r"saturation: ([-\d.e]+)", blk).group(1))
    if not re.search(r"ignorePlatformSaturationModifiers: 1", blk):
        sat += 0.17  # SceneManager.AdjustSaturation
    curves = []
    for ch in ("redChannel", "greenChannel", "blueChannel"):
        j = blk.find(ch + ":")
        k = blk.find("m_PreInfinity", j)
        keys = [
            (float(a), float(b), float(c), float(d))
            for a, b, c, d in re.findall(
                r"time: ([-\d.e]+)\n\s+value: ([-\d.e]+)\n\s+inSlope: ([-\d.e]+|Infinity|-Infinity)\n\s+outSlope: ([-\d.e]+|Infinity|-Infinity)",
                blk[j:k],
            )
        ]
        curves.append(keys)
    return sat, curves


def eval_curve(keys, x):
    """Unity AnimationCurve (Hermite between keys, clamped outside)."""
    x = np.asarray(x, np.float64)
    out = np.empty_like(x)
    if not keys:
        return x
    if len(keys) == 1:
        out[:] = keys[0][1]
        return out
    out[x <= keys[0][0]] = keys[0][1]
    out[x >= keys[-1][0]] = keys[-1][1]
    for (t0, v0, _, o0), (t1, v1, i1, _) in zip(keys, keys[1:]):
        m = (x > t0) & (x < t1)
        if not m.any():
            continue
        dt = t1 - t0
        s = (x[m] - t0) / dt
        h00 = 2 * s**3 - 3 * s**2 + 1
        h10 = s**3 - 2 * s**2 + s
        h01 = -2 * s**3 + 3 * s**2
        h11 = s**3 - s**2
        out[m] = h00 * v0 + h10 * dt * o0 + h01 * v1 + h11 * dt * i1
    return out


def apply(rgb, g):
    """rgb float 0..1 (..., 3). ColorCorrectionCurves (simple mode): curves per channel, then saturation
    (lerp from luminance)."""
    sat, curves = g
    lut = [eval_curve(c, np.linspace(0, 1, 256)) for c in curves]
    idx = np.clip(np.round(rgb * 255), 0, 255).astype(np.int32)
    o = np.stack([lut[k][idx[..., k]] for k in range(3)], -1)
    lum = (o * [0.22, 0.707, 0.071]).sum(-1, keepdims=True)
    o = lum + (o - lum) * sat
    return np.clip(o, 0, 1)


def identity(g):
    if g is None:
        return True
    sat, curves = g
    x = np.linspace(0, 1, 32)
    return abs(sat - 1) < 1e-3 and all(
        np.abs(eval_curve(c, x) - x).max() < 2e-3 for c in curves
    )


if __name__ == "__main__":
    from PIL import Image

    sys.path.insert(
        0, __import__("os").path.dirname(__import__("os").path.abspath(__file__))
    )
    import build_room as BR

    room, src, out = sys.argv[1:4]
    g = scene_grade(BR.scene_path(room))
    print(
        room,
        "sat",
        g[0],
        [[(round(a, 3), round(b, 3)) for a, b, _, _ in c] for c in g[1]],
    )
    im = np.asarray(Image.open(src).convert("RGB")).astype(np.float64) / 255
    Image.fromarray((apply(im, g) * 255 + 0.5).astype(np.uint8)).save(out)
