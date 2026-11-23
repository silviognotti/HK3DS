#!/bin/bash
# Clear and rebuild the HKDATA indexes and generated lookup data.
#   scripts/01_index_hkdata.sh [--hkdata DIR]
set -euo pipefail

trap 'echo "Interrupted: stopping HKDATA indexing." >&2; exit 130' INT TERM

START_TIME=$(python3 -c 'import time; print(time.monotonic())')

run_timed() {
  local label=$1
  shift
  local step_start step_end elapsed
  echo "== $label"
  step_start=$(python3 -c 'import time; print(time.monotonic())')
  "$@" || return
  step_end=$(python3 -c 'import time; print(time.monotonic())')
  elapsed=$(python3 -c 'import sys; print(f"{float(sys.argv[2]) - float(sys.argv[1]):.2f}s" if float(sys.argv[2]) >= float(sys.argv[1]) else "0.00s")' "$step_start" "$step_end")
  echo "[$label] $elapsed"
}

H=$(cd "$(dirname "$0")/.." && pwd)
T=$H/tools
HKDATA=${HKDATA:-$H/hkdata}
WORKERS=$(python3 -c 'import os; print(max(1, min(16, (os.cpu_count() or 1) * 2)))')
cd "$H"

while [ "$#" -gt 0 ]; do
  case "$1" in
    --hkdata) HKDATA=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: 01_index_hkdata.sh [--hkdata DIR]}"); shift 2 ;;
    --hkdata=*) HKDATA=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    --workers) WORKERS=${2:?usage: 01_index_hkdata.sh [--hkdata DIR] [--workers COUNT]}; shift 2 ;;
    --workers=*) WORKERS=${1#*=}; shift ;;
    *) echo "usage: 01_index_hkdata.sh [--hkdata DIR] [--workers COUNT]" >&2; exit 2 ;;
  esac
done

if [ ! -d "$HKDATA/ex" ]; then
  echo "HKDATA export not found at $HKDATA/ex" >&2
  exit 1
fi

echo "== clear indexes"
find "$HKDATA" -type f -name '*.pkl' -delete

echo "== clear generated data"
rm -f \
  "$T/data/audio_map.json" \
  "$T/data/script_guids.json" \
  "$T/data/cs_sigs.json" \
  "$T/data/fsm_templates.json" \
  "$T/data/particle_textures.json" \
  "$T/data/gg_statues.txt" \
  "$T/data/music_cues.txt" \
  "$T/data/all_rooms.txt"
rm -rf "$T/data/uoroots"
mkdir -p "$T/data"

echo "== make room list"
HKDATA="$HKDATA" python3 - <<'PY'
import os
from pathlib import Path

hkdata = Path(os.environ["HKDATA"])
scene_dir = hkdata / "ex" / "Scenes"
excluded = {
    "BetaEnd",
    "Cinematic_Ending_A",
    "Cinematic_Ending_B",
    "Cinematic_Ending_C",
    "Cinematic_Ending_D",
    "Cinematic_Ending_E",
    "Cinematic_MrMushroom",
    "Cinematic_Stag_travel",
    "Cutscene_Boss_Door",
    "End_Credits",
    "End_Game_Completion",
    "Knight_Pickup",
    "Menu_Credits",
    "Menu_Title",
    "Opening_Sequence",
    "PermaDeath",
    "PermaDeath_Unlock",
    "Pre_Menu_Intro",
    "Quit_To_Menu",
    "_test_cocoon_1",
    "_test_cocoon_2",
}
rooms = sorted({
    path.stem
    for path in scene_dir.rglob("*.unity")
    if path.stem not in excluded
})
output = Path("tools/data/all_rooms.txt")
output.write_text("".join(room + "\n" for room in rooms), encoding="utf-8")
print(f"Wrote {output}: {len(rooms)} rooms")
PY

run_timed "index export" env HKDATA="$HKDATA" python3 "$T/index_export.py" "$HKDATA" --workers "$WORKERS"

run_timed "make audio map" env HKDATA="$HKDATA" python3 "$T/make_audio_map.py" "$HKDATA" --workers "$WORKERS"

run_timed "make script GUIDs" env HKDATA="$HKDATA" python3 "$T/make_script_guids.py" "$HKDATA" --workers "$WORKERS"

run_timed "make C# signatures" env HKDATA="$HKDATA" python3 "$T/cs_sigs.py" --workers "$WORKERS"

run_timed "make FSM templates" env HKDATA="$HKDATA" python3 "$T/fsm_templates.py" --workers "$WORKERS"

run_timed "make particle textures" env HKDATA="$HKDATA" python3 "$T/make_particle_textures.py" "$HKDATA" --workers "$WORKERS"

run_timed "make gamedata lookups" env HKDATA="$HKDATA" python3 "$T/make_gamedata_lookup.py" "$HKDATA"

run_timed "make alpha cache" env HKDATA="$HKDATA" python3 "$T/alpha_cache.py" --workers "$WORKERS"
run_timed "match textures" env HKDATA="$HKDATA" python3 "$T/match_textures.py" --all --workers "$WORKERS"

run_timed "make animation cache" env HKDATA="$HKDATA" python3 - <<'PY'
import sys
sys.path.insert(0, "tools")
import uanim
uanim.index()
PY

run_timed "make .meta caches" env HKDATA="$HKDATA" python3 - <<'PY'
import sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, "tools")
from hkcommon import metamap
folders = ("AudioClip", "GameObject", "Material", "MonoBehaviour", "Sprite", "Texture2D")
with ThreadPoolExecutor(max_workers=min(6, len(folders))) as pool:
    list(pool.map(metamap, folders))
PY

END_TIME=$(python3 -c 'import time; print(time.monotonic())')
TOTAL=$(python3 -c 'import sys; print(f"{float(sys.argv[2]) - float(sys.argv[1]):.2f}s")' "$START_TIME" "$END_TIME")
echo "DONE: HKDATA indexed in $TOTAL"
