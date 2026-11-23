#!/usr/bin/env python3
"""camcheck.py <log> <min knight x>: 1 if, on the last trace line, the Knight is past min x and inside the 3DS view."""

import sys, re

last = [l for l in open(sys.argv[1], errors="ignore") if l.startswith("t=")][-1]
p = re.search(r"P=\(([-0-9.]+),([-0-9.]+)\)", last)
c = re.search(r"cam=\(([-0-9.]+),([-0-9.]+)\)", last)
px, py, cx, cy = float(p[1]), float(p[2]), float(c[1]), float(c[2])
print(
    1 if px > float(sys.argv[2]) and abs(px - cx) < 14.2 and abs(py - cy) < 8.1 else 0
)
