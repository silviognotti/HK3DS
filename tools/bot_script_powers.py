# Random play with every power (test the Knight's state machines against each other): runs, jumps, double jumps,
# slashes, spells (tap and quick cast, in the air too), dashes, super dash charges, focus. usage: <seed> <frames>
import random, sys

rnd = random.Random(int(sys.argv[1]))
total = int(sys.argv[2])
segs = ["150:", "1:A", "40:", "1:A", "30:"]
f = 0
d = rnd.choice(["RIGHT", "LEFT"])
while f < total:
    if rnd.random() < 0.15:
        d = "LEFT" if d == "RIGHT" else "RIGHT"
    a = rnd.random()
    if a < 0.25:
        seq = [(rnd.randint(5, 40), d)]
    elif a < 0.40:
        seq = [
            (rnd.randint(3, 14), d + "+B"),
            (rnd.randint(0, 12), d),
            (rnd.randint(2, 10), d + "+B"),
        ]  # jump, double jump
    elif a < 0.50:
        seq = [(1, d + "+Y"), (8, d)]
    elif a < 0.60:
        seq = [
            (rnd.randint(2, 14), "B"),
            (rnd.randint(0, 10), ""),
            (1, "A"),
            (rnd.randint(0, 20), ""),
        ]  # jump + cast
    elif a < 0.66:
        seq = [
            (1, "A"),
            (rnd.randint(0, 6), ""),
            (rnd.randint(3, 14), "B"),
        ]  # cast + jump
    elif a < 0.72:
        seq = [
            (rnd.randint(2, 14), "B"),
            (1, "ZR"),
            (rnd.randint(1, 15), "B"),
        ]  # quick cast in the air
    elif a < 0.80:
        seq = [
            (rnd.randint(0, 12), "B"),
            (1, d + "+R"),
            (rnd.randint(0, 10), ""),
            (rnd.randint(2, 10), "B"),
        ]  # dash (air)
    elif a < 0.84:
        seq = [
            (rnd.randint(30, 80), "ZL"),
            (rnd.randint(10, 60), ""),
            (rnd.randint(1, 3), "B"),
        ]  # super dash
    elif a < 0.88:
        seq = [(rnd.randint(1, 3), "DOWN+A"), (rnd.randint(10, 40), "")]  # dive
    elif a < 0.91:
        seq = [(1, "UP+A"), (rnd.randint(10, 30), "")]  # shriek
    elif a < 0.94:
        seq = [(rnd.randint(20, 90), "A")]  # focus
    else:
        seq = [(rnd.randint(5, 20), "")]
    for n, k in seq:
        if n > 0:
            segs.append(f"{n}:{k}")
            f += n
print(";".join(segs))
