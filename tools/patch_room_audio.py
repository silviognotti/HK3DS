#!/usr/bin/env python3
"""Rewrite the music / ambience / map zone fields of already-built .room files (build_room.area_of), without rebaking.
python patch_room_audio.py <dir with Room.room files> [rooms...]"""

import sys, os, glob, struct

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_room as BR

def patch_room(directory, room):
    path = f"{directory}/{room}.room"
    if not os.path.isfile(path):
        return False
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"HKRM":
        print("skip (packed?)", room)
        return True
    music, ambience, zone = BR.area_of(room)
    for index, text in enumerate((music, ambience, zone)):
        data[20 + 32 * index:52 + 32 * index] = struct.pack("32s", text.encode()[:31])
    open(path, "wb").write(bytes(data))
    return True


def patch_rooms(directory, rooms):
    if rooms:
        for room in rooms:
            patch_room(directory, room)
    else:
        for path in glob.glob(f"{directory}/*.room"):
            patch_room(directory, os.path.basename(path)[:-5])


if __name__ == "__main__":
    patch_rooms(sys.argv[1], sys.argv[2:])
