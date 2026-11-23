#!/usr/bin/env python3
"""The Knight's HeroLight: the soft additive glow he carries (Knight_0.prefab 'HeroLight': haze2 sprite, additive
material, scale 3, at his feet), tinted by each scene's SceneManager.heroLightColor.
Writes romfs/gfx/herolight.a8 (128x128 8-bit alpha, smooth: at 4 bits it showed rings around the Knight) and
romfs/text/herolight.txt ("<room> r g b a ar ag ab" per scene: the hero light colour and the scene's ambient light,
which tints objects on the game's lit sprite material; the sprite's world size on the first line).
    python3 build_herolight.py <romfs> [rooms.txt]"""
import sys, os, re
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bake_scene as B
import build_room as BR
from texpack import MORTON

HAZE2 = B.export_ids().get('herolight') or 'b04b22f2cc0b98f4992e9b4a31714b97'     # make_lookups.py: this export's guid
SCALE = 3.0                  # the HeroLight transform's scale
romfs = sys.argv[1]
sp = B.sprite_asset(HAZE2)
if sp is None: sys.exit('the HeroLight sprite (haze2): not in the export (run make_lookups.py: tools/build.sh --from lookups)')
im, offx, offy, rw, rh, pivx, pivy, ppu = B.sprite_image(sp)
al = np.asarray(im.convert('RGBA'))[..., 3]
lum = np.asarray(im.convert('RGBA'))[..., :3].astype(np.float32).max(-1) / 255.0
a = (al.astype(np.float32) * lum).clip(0, 255)        # premultiplied by the texture's own brightness (additive light)
# The texture's own gradient wobbles by a level or two (its compression): magnified ~10x and added over black those
# wobbles showed as rings around the Knight. Rebuilt as a smooth elliptical falloff with the same profile and centre.
H0, W0 = a.shape; yy, xx = np.mgrid[0:H0, 0:W0]; tot = a.sum()
cx, cy = (a * xx).sum() / tot, (a * yy).sum() / tot; rx, ry = W0 / 2, H0 / 2
rr = np.sqrt(((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2)
NB = 64; bins = np.minimum((rr * NB / 1.3).astype(int), NB - 1)
prof = np.array([a[bins == b].mean() if (bins == b).any() else 0.0 for b in range(NB)])
prof = np.minimum.accumulate(np.nan_to_num(prof))                     # never brighter further out
k = np.exp(-np.arange(-3, 4) ** 2 / 4.0); k /= k.sum()
prof = np.convolve(np.pad(prof, 3, mode='edge'), k, 'valid'); prof[-2:] = 0
ys, xs = np.mgrid[0:128, 0:128]
r2 = np.sqrt((((xs + 0.5) * W0 / 128 - 0.5 - cx) / rx) ** 2 + (((ys + 0.5) * H0 / 128 - 0.5 - cy) / ry) ** 2)
sm = np.interp(r2 * NB / 1.3, np.arange(NB) + 0.5, prof, left=prof[0], right=0.0)
img = Image.fromarray(np.clip(sm + 0.5, 0, 255).astype(np.uint8))
t = np.asarray(img).reshape(16, 8, 16, 8).transpose(0, 2, 1, 3).reshape(16, 16, 64)
out = np.empty_like(t); out[:, :, MORTON] = t
open(os.path.join(romfs, 'gfx', 'herolight.a8'), 'wb').write(out.tobytes())
ww, wh = im.size[0] / ppu * SCALE, im.size[1] / ppu * SCALE
rooms = open(sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'all_rooms.txt')).read().split()
lines = [f'size {ww:.3f} {wh:.3f} 0.0 -0.6']
for r in rooms:
    p = BR.scene_path(r)
    if not p: continue
    t = open(p, errors='ignore').read()
    i = t.find('\n  heroLightColor:')
    if i < 0: continue
    m = re.search(r'heroLightColor: \{r: ([-\d.e]+), g: ([-\d.e]+), b: ([-\d.e]+), a: ([-\d.e]+)\}', t[i:i + 200])
    amb = B.scene_ambient(p) or (1.0, 1.0, 1.0)
    if m: lines.append(f'{r} ' + ' '.join(f'{float(m.group(k)):.3f}' for k in (1, 2, 3, 4)) + ' ' + ' '.join(f'{x:.3f}' for x in amb))
open(os.path.join(romfs, 'text', 'herolight.txt'), 'w').write('\n'.join(lines) + '\n')
print('hero light', im.size, 'ppu', ppu, 'world', round(ww, 2), round(wh, 2), len(lines) - 1, 'scenes')
