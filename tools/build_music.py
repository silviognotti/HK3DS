#!/usr/bin/env python3
"""Music and ambience for every scene.
MusicCue assets (MonoBehaviour/*.asset with channelInfos) are mixed down to one stereo ogg each: the Main channel plus the
layers the game plays by default (main / bass / sub); the Action / Tension / drone layers the mixer brings in only during
fights or quiet moments are left out. AtmosCue loops are the AudioManager's atmos sources.
   python build_music.py <romfs/music> [cue names...]      (default: every cue)"""

import sys, os, re, glob, subprocess, pickle

EX = os.environ.get("HKDATA", "hkdata") + "/ex"
AM = pickle.load(open(os.environ.get("HKDATA", "hkdata") + "/meta_AudioClip.pkl", "rb"))
CUE_SCRIPT = "400403b587ef0def1b40de34c0f1ebec"
ATMOS_LOOPS = [
    "cave_wind_loop",
    "dirtmouth_wind_loop_a",
    "dirtmouth_wind_loop_b_grass",
    "dirtmouth_wind_loop_c_gate",
    "cave_noises",
    "ruins_rain_indoor_loop",
    "ruins_rain_outdoor_loop",
    "green_path_atmos_loop",
    "fungal_wastes_atmos_loop",
    "fog_canyon_atmos_loop",
    "waterways_atmos_loop",
    "waterway_waterfall_medium",
    "mines_crystal_shimmer_loop",
    "mines_machinery_atmos",
    "deepnest_atmos_loop",
    "cave_atmos_misc_3",
]
SKIP = re.compile(
    r"action|tension|drone|epic|inside|mantis|timp|suspence [234]|final stage [2-6]",
    re.I,
)
KEEP = re.compile(r"main|bass|sub|loop", re.I)


def cue_file(name):
    return name.lower().replace(" ", "_")


def cues():
    out = {}
    for f in glob.glob(f"{EX}/MonoBehaviour/*.asset"):
        t = open(f, errors="ignore").read()
        if CUE_SCRIPT not in t or "channelInfos" not in t:
            continue
        name = os.path.basename(f)[:-6]
        chans = []
        for m in re.finditer(r"- clip: \{fileID: (\d+)(?:, guid: ([0-9a-f]+))?", t):
            g = m.group(2)
            chans.append(AM.get(g) if g else None)
        out[name] = chans
    return out


def pick(chans):
    have = [(i, p) for i, p in enumerate(chans) if p]
    if not have:
        return []
    sel = [
        p
        for i, p in have
        if i == 0
        or (KEEP.search(os.path.basename(p)) and not SKIP.search(os.path.basename(p)))
    ]
    if chans[0] is None or SKIP.search(os.path.basename(have[0][1])):
        sel = [p for i, p in have if not SKIP.search(os.path.basename(p))] or [
            have[0][1]
        ]
    return sel


def encode(srcs, out):
    if os.path.exists(out):
        return False
    cmd = ["ffmpeg", "-v", "error", "-y"]
    for s in srcs:
        cmd += ["-i", s]
    if len(srcs) > 1:
        cmd += [
            "-filter_complex",
            f"amix=inputs={len(srcs)}:normalize=0:duration=longest",
        ]
    cmd += ["-ar", "32000", "-ac", "2", "-c:a", "libvorbis", "-q:a", "2", out]
    subprocess.run(cmd, check=True)
    return True


def main(outdir, only=None):
    os.makedirs(outdir, exist_ok=True)
    for name, chans in sorted(cues().items()):
        if only and name not in only:
            continue
        sel = pick(chans)
        if not sel:
            continue
        if encode(sel, f"{outdir}/{cue_file(name)}.ogg"):
            print("music", name, [os.path.basename(s) for s in sel], flush=True)
    if not only:
        for a in ATMOS_LOOPS:
            src = f"{EX}/AudioClip/{a}.ogg"
            if os.path.exists(src) and encode([src], f"{outdir}/{a}.ogg"):
                print("atmos", a, flush=True)


if __name__ == "__main__":
    main(sys.argv[1], set(sys.argv[2:]) or None)
