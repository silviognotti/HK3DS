#!/bin/bash
# Re-export and pack the live objects (enemies, NPCs, interactables) of every room.
#   scripts/02_export_all.sh [--build DIR]
H=$(cd "$(dirname "$0")/.." && pwd); T=$H/tools
BUILD=$H/build
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: export_all.sh [--build DIR]}"); shift 2 ;;
    --build=*) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    *) echo "usage: export_all.sh [--build DIR]" >&2; exit 2 ;;
  esac
done
OBJ=$BUILD/objs
ROMFS=$BUILD/romfs
LIST=$H/tools/data/all_rooms.txt
cd "$T"
rm -rf "$T/data/uoroots"
mkdir -p "$T/data/uoroots"
mkdir -p "$OBJ"
for r in $(cat "$LIST"); do
  nice python3 build_objs.py "$OBJ" "$r" 2>&1 | tail -1
done
for r in $(cat "$LIST"); do
  nice python3 pack_objs.py "$OBJ" "$ROMFS" "$r" 2>&1 | grep -v "^  " | tail -1
  [ -f "$ROMFS/rooms/${r}_objs.spr" ] && python3 zwrap.py "$ROMFS/rooms/${r}_objs.spr" >/dev/null
  # pack_objs writes a fresh <Room>.obj: put its particle systems and sprite materials back (obj_particles.py)
  nice python3 obj_particles.py "$ROMFS" "$r" 2>&1 | grep -v "^wrote" | tail -1
done
echo ALLDONE
