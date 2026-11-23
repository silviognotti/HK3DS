#!/bin/bash
# Copy freshly built rooms (build_room.py output) into romfs and finish them: HKL2 layers, zlib wrapping, camera locks,
# secret masks.   scripts/04_install_rooms.sh [--build DIR] [--room-build DIR] [--romfs DIR] [--jobs COUNT] [Room ...]
H=$(cd "$(dirname "$0")/.." && pwd); T=$H/tools
BUILD=$H/build
ROOM_BUILD=
ROMFS=
JOBS=4
ROOMS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: install_rooms.sh [--build DIR] [ROOM...]}"); shift 2 ;;
    --build=*) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    --room-build) ROOM_BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: install_rooms.sh [--room-build DIR] [--romfs DIR]}"); shift 2 ;;
    --room-build=*) ROOM_BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    --romfs) ROMFS=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: install_rooms.sh [--room-build DIR] [--romfs DIR]}"); shift 2 ;;
    --romfs=*) ROMFS=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    --jobs) JOBS=${2:?usage: install_rooms.sh [--jobs COUNT]}; shift 2 ;;
    --jobs=*) JOBS=${1#*=}; shift ;;
    -j*) JOBS=${1#-j}; [ -z "$JOBS" ] && { JOBS=$2; shift; } ;;
    *) ROOMS+=("$1"); shift ;;
  esac
done
B=${ROOM_BUILD:-$BUILD}
R=${ROMFS:-$BUILD/romfs}
if [ "${#ROOMS[@]}" -eq 0 ]; then
  mapfile -t ROOMS < "$H/tools/data/all_rooms.txt"
fi
cd "$T"
exec python3 "$T/install_rooms.py" \
  --room-build "$B" \
  --romfs "$R" \
  --jobs "$JOBS" \
  "${ROOMS[@]}"
