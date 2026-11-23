# Combine HK_REACH outputs into a map-wide graph: rooms reachable from a new game, dead exits, and return paths.
import sys, re, collections

ent = collections.defaultdict(
    lambda: collections.defaultdict(set)
)  # room -> entry -> set(exits)
targets = {}  # (room, gate) -> (troom, tgate)
for fn in sys.argv[1:]:
    room = None
    for line in open(fn):
        m = re.match(r"ROOM (\S+)", line)
        if m:
            room = m.group(1)
            continue
        if line.strip().startswith("GATES:"):
            body = line.strip()[len("GATES:") :]
            for m2 in re.finditer(r"\s*(.+?)->(\S+)", body):
                g, t = m2.group(1).strip(), m2.group(2)
                targets[(room, g)] = tuple(t.split(":")) if t != "(none)" else None
            continue
        m = re.match(r"\s+ENTRY (.+?)\s+at \(.* reaches:(.*)", line)
        if m:
            ent[room][m.group(1)] |= set(m.group(2).split())
            continue
        m = re.match(r"\s+ENTRY (.+?): never lands", line)
        if m:
            ent[room].setdefault(m.group(1), set())
            print("NEVER LANDS", room, m.group(1))


def succ(node):
    room, g = node
    exits = set(ent[room].get(g, ()))
    if g in ent[room] and g != "spawn":
        exits.add(g)  # you can always walk back out the way you came in
    for ex in exits:
        t = targets.get((room, ex))
        if t:
            yield (t[0], t[1] if len(t) > 1 else "")


start = ("Tutorial_01", "spawn")
seen = {start}
q = [start]
while q:
    n = q.pop()
    for s in succ(n):
        if s not in seen:
            seen.add(s)
            q.append(s)
rooms = sorted(ent)
reached_rooms = sorted({n[0] for n in seen})
print("rooms reachable from a new game:", len(reached_rooms), "/", len(rooms))
print("NOT reachable:", [r for r in rooms if r not in reached_rooms])


# can we get back to Town from every reached node?
def can_reach(src, goal_room):
    sn = {src}
    q = [src]
    while q:
        n = q.pop()
        if n[0] == goal_room:
            return True
        for s in succ(n):
            if s not in sn:
                sn.add(s)
                q.append(s)
    return False


stuck = [n for n in sorted(seen) if not can_reach(n, "Town")]
print("arrival points with no way back to Dirtmouth:", len(stuck))
for n in stuck:
    print("   ", n)
# exits with a destination that no reached entry can get to
print("unreachable exits (with destinations) in reached rooms:")
for r in reached_rooms:
    got = set()
    for rr, g in seen:
        if rr == r:
            got |= ent[r].get(g, set())
            got.add(g)
    for (rr, g), t in targets.items():
        if rr == r and t and g not in got:
            print("   ", r, g, "->", ":".join(t))
