#!/bin/bash
# Rebuild every room from the game data: live objects first (their roots are left out of the baked art), then the
# room art / collision, then install into romfs and pack the objects.
#   scripts/03_full_rebuild.sh [--build DIR] [--phases "objects rooms install pack"]
H=$(cd "$(dirname "$0")/.." && pwd); T=$H/tools
BUILD=$H/build
PH="objects rooms install pack"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: full_rebuild.sh [--build DIR] [--phases \"...\"]}"); shift 2 ;;
    --build=*) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    --phases) PH=${2:?usage: full_rebuild.sh [--build DIR] [--phases \"...\"]}; shift 2 ;;
    --phases=*) PH=${1#*=}; shift ;;
    *) echo "usage: full_rebuild.sh [--build DIR] [--phases \"...\"]" >&2; exit 2 ;;
  esac
done
OBJ=$BUILD/objs
FB=$BUILD
ROMFS=$BUILD/romfs
LIST=$H/tools/data/all_rooms.txt
cd "$T"
rm -rf "$T/data/uoroots"
mkdir -p "$T/data/uoroots" "$OBJ" "$FB"
case "$PH" in *objects*) echo "== objects"
for r in $(cat "$LIST"); do nice python3 build_objs.py "$OBJ" "$r" 2>&1 | tail -1; done ;; esac
case "$PH" in *rooms*) echo "== rooms"
for r in $(cat "$LIST"); do nice python3 build_room.py "$FB" "$r" 2>&1 | grep -v "^  \|top:\|matched" | tail -1; done ;; esac
case "$PH" in *install*) echo "== install"
"$H/scripts/04_install_rooms.sh" --build "$BUILD" 2>&1 | grep -v "lock zones\|mask groups" | tail -3
# rooms built one at a time only link exits to rooms in the same batch: link every exit to the installed rooms
python3 fill_gate_targets.py "$ROMFS/rooms" "$FB" | tail -1 ;; esac
case "$PH" in *pack*) echo "== pack"
for r in $(cat "$LIST"); do
  nice python3 pack_objs.py "$OBJ" "$ROMFS" "$r" 2>&1 | grep -v "^  " | tail -1
  [ -f "$ROMFS/rooms/${r}_objs.spr" ] && python3 zwrap.py "$ROMFS/rooms/${r}_objs.spr" >/dev/null
  # pack_objs writes a fresh <Room>.obj: put its particle systems and sprite materials back (obj_particles.py)
  nice python3 obj_particles.py "$ROMFS" "$r" 2>&1 | grep -v "^wrote" | tail -1
done ;; esac
echo ALLDONE
