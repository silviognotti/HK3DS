# Random-play input script for the host harness (HK_SCRIPT format): runs, jumps of varying height, slashes in
# all directions, direction changes. usage: bot_script.py <seed> <frames>
import random, sys

rnd = random.Random(int(sys.argv[1]))
total = int(sys.argv[2])
segs = [
    "150:",
    "1:A",
    "40:",
    "1:A",
    "30:",
]  # menu: Start Game -> New Game, then wait for the fade
f = 0
d = rnd.choice(["RIGHT", "LEFT"])
while f < total:
    r = rnd.random()
    if r < 0.15:
        d = "LEFT" if d == "RIGHT" else "RIGHT"
    n = rnd.randint(5, 40)
    act = rnd.random()
    if act < 0.35:
        k = d
    elif act < 0.6:
        k = d + "+B"
        n = rnd.randint(3, 14)  # jump (held 3..14 frames)
    elif act < 0.72:
        k = d + "+Y"
        n = 1
    elif act < 0.78:
        k = "UP+Y"
        n = 1
    elif act < 0.86:
        k = d + "+B"
        n = 12
        segs.append(f"{n}:{k}")
        f += n
        k = "DOWN+Y"
        n = 1  # jump + pogo
    elif act < 0.92:
        k = "B"
        n = rnd.randint(4, 14)  # straight jump
    else:
        k = ""
        n = rnd.randint(5, 20)
    segs.append(f"{n}:{k}")
    f += n
    if k.endswith("Y"):
        segs.append("8:" + d)
        f += 8
print(";".join(segs))
