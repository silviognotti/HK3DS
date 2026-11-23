#!/bin/bash
# HK3DS: build the game from your own copy of Hollow Knight, start to finish, with one command:
#
#   scripts/00_build.sh       the standard version: romfs/, then hk3ds_std.romfs (the game data for the SD card),
#                             hk3ds.3dsx and hk3ds.cia (the code)
#   scripts/00_build.sh --hd  the HD version as well: romfs_hd/, then hk3ds_hd.romfs and hk3ds_hd.cia
#
# It reads the game exports in $HKDATA (default: the hkdata folder next to this repository's folder, or ./hkdata).
# Every step writes its own log in work/logs/. When a step fails, the build stops and names the step and its log.
# Steps (and rooms) that finished are remembered in work/done/, so running the same command again carries on from the
# step that failed. Run it from anywhere; paths are relative to the repository.
#
# Options:
#   --hd             also build the HD version
#   --check          only check the requirements and the exports, then stop
#   --check-romfs [DIR]  verify a built ROMFS and stop
#   --rooms FILE     convert only the rooms listed in FILE, one per line (default: tools/data/all_rooms.txt)
#   --from STEP      forget STEP and every step after it, and run them again (see --list)
#   --only STEP      run just STEP (again, if it was done)
#   --index          force regeneration of the HKDATA export and lookup indexes
#   --no-compile     convert the game data, but don't build the .3dsx / .cia
#   --jobs N, -j N   convert N rooms at once (much faster on a PC with several cores; each needs 1-2 GB of memory)
#   --list           list the steps
set -o pipefail
trap 'echo "Interrupted: stopping build." >&2; exit 130' INT TERM
TOP=$(cd "$(dirname "$0")/.." && pwd); T=$TOP/tools; W=$TOP/work
HD=0; ONLY=""; FROM=""; COMPILE=1; CHECKONLY=0; CHECKROMFS=0; CHECKROMFS_DIR=""; FORCE_INDEX=0; ROOMS=$T/data/all_rooms.txt; JOBS=1
STEPS="check index objects rooms install pack particles gamedata compile"
HDSTEPS="hd_copy hd_rooms hd_objects hd_gamedata hd_compile"

while [ $# -gt 0 ]; do
  case "$1" in
    --hd) HD=1 ;;
    --check) CHECKONLY=1 ;;
    --check-romfs)
      CHECKROMFS=1
      if [ "$#" -gt 1 ] && [ "${2#-}" = "$2" ]; then CHECKROMFS_DIR=$2; shift; fi
      ;;
    --rooms) ROOMS=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); shift ;;
    --from) FROM=$2; shift ;;
    --only) ONLY=$2; shift ;;
    --index) FORCE_INDEX=1 ;;
    --no-compile) COMPILE=0 ;;
    --jobs) JOBS=$2; shift ;;
    -j*) JOBS=${1#-j}; [ -z "$JOBS" ] && { JOBS=$2; shift; } ;;
    --list) echo "steps: $STEPS"; echo "HD steps (--hd): $HDSTEPS"; exit 0 ;;
    -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option $1 (scripts/00_build.sh --help)"; exit 2 ;;
  esac
  shift
done

# the exports: $HKDATA, else ../hkdata (next to the repository), else ./hkdata
if [ -z "$HKDATA" ]; then
  if [ -d "$TOP/../hkdata" ]; then HKDATA=$TOP/../hkdata; else HKDATA=$TOP/hkdata; fi
fi
HKDATA=$(cd "$HKDATA" 2>/dev/null && pwd || echo "$HKDATA"); export HKDATA
PY=${PYTHON:-python3}
if [ "$CHECKROMFS" = 1 ]; then
  CHECKROMFS_DIR=${CHECKROMFS_DIR:-$TOP/romfs}
  exec "$PY" "$T/check_romfs.py" "$CHECKROMFS_DIR" --rooms "$ROOMS"
fi
export PYTHONUNBUFFERED=1
export PYTHONIOENCODING=utf-8
[ -n "$DEVKITPRO" ] && export PATH="$DEVKITPRO/tools/bin:${DEVKITARM:-$DEVKITPRO/devkitARM}/bin:$PATH"   # makerom, bannertool, the compiler
mkdir -p "$W/logs" "$W/done" "$W/obj" "$W/rooms"
R=$TOP/romfs; RH=$TOP/romfs_hd
mkdir -p "$R/rooms" "$R/text/es" "$R/gfx" "$R/music" "$R/sfx"     # every tool can count on its output folder

exec 3>&1                         # the terminal: steps write their output to their log, and their progress here
say() { echo "$@" >&3; }
fail() { echo >&3; echo "FAILED: $1" >&3; [ -n "$2" ] && { echo "  log: $2" >&3; echo "  its last lines:" >&3; tail -n 15 "$2" | sed 's/^/    /' >&3; }
         echo >&3; echo "Fix that and run the same command again: it carries on from there." >&3; exit 1; }
done_mark() { touch "$W/done/$1"; }
is_done() { [ -f "$W/done/$1" ]; }

run_logged() {
  local label=$1
  local log=$2
  shift 2
  echo "== $label" | tee -a "$log"
  "$@" 2>&1 | tee -a "$log"
  local status=${PIPESTATUS[0]}
  if [ "$status" -ne 0 ]; then
    echo "[exit $status] $label" | tee -a "$log" >&2
  fi
  return "$status"
}

# run_rooms STEP CMD...: run CMD <room> for every room not done yet in STEP (one marker per room, so a long step
# that stops halfway carries on from the room it stopped at)
run_rooms() {
  local step=$1; shift; local n=0 tot r; tot=$(grep -c . "$ROOMS")
  mkdir -p "$W/done/$step.rooms" "$W/logs/$step"; rm -f "$W/logs/$step.failed"
  local status_dir="$W/logs/$step/status"
  rm -rf "$status_dir"
  mkdir -p "$status_dir"
  while read -r r; do
    [ -z "$r" ] && continue; n=$((n + 1))
    [ -f "$W/done/$step.rooms/$r" ] && continue
    [ -f "$W/logs/$step.failed" ] && break
    if compgen -G "$status_dir/*.status" >/dev/null; then
      for status_file in "$status_dir"/*.status; do
        [ -f "$status_file" ] || continue
        [ "$(cat "$status_file")" -eq 0 ] 2>/dev/null || break 2
      done
    fi
    printf '\r  %4d/%d  %-50s' "$n" "$tot" "$r" >&3
    if [ "$JOBS" -le 1 ]; then
      echo "== $r" | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log"
      "$@" "$r" < /dev/null 2>&1 | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log"
      status=${PIPESTATUS[0]}
      if [ "$status" -ne 0 ]; then
        rm -f "$W/done/$step.rooms/$r"
        echo "[exit $status] room $r" | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log" >&2
        if grep -Eq "Cannot find scene:|Traceback \\(most recent call last\\):|TypeError:|SystemExit:" "$W/logs/$step/$r.log"; then
          echo "[fatal] $r: room export stopped on an actionable error" | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log" >&2
        fi
        echo "room $r failed with exit $status"
        echo "  log: $W/logs/$step/$r.log"
        echo "  last output:"
        tail -n 30 "$W/logs/$step/$r.log" | sed 's/^/    /'
        printf '\n  room %s failed (exit %s)\n' "$r" "$status" >&3
        return 1
      fi
      touch "$W/done/$step.rooms/$r"
    else          # --jobs: up to JOBS rooms at once, each with its own log (work/logs/<step>/<room>.log)
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
      (
        echo "== $r" | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log"
        "$@" "$r" < /dev/null 2>&1 | tee -a "$W/logs/$step.log" "$W/logs/$step/$r.log"
        status=${PIPESTATUS[0]}
        if [ "$status" -eq 0 ]; then
          touch "$W/done/$step.rooms/$r"
        else
          rm -f "$W/done/$step.rooms/$r"
          echo "[exit $status] room $r" >> "$W/logs/$step/$r.log"
          if grep -Eq "Cannot find scene:|Traceback \\(most recent call last\\):|TypeError:|SystemExit:" "$W/logs/$step/$r.log"; then
            echo "[fatal] $r: room export stopped on an actionable error" >> "$W/logs/$step/$r.log"
          fi
        fi
        echo "$status" > "$status_dir/$r.status"
      ) &
    fi
  done < "$ROOMS"
  wait
  failed_status=""
  for status_file in "$status_dir"/*.status; do
    [ -f "$status_file" ] || continue
    status=$(cat "$status_file")
    if [ "$status" -ne 0 ]; then
      failed_status="$status_file"
      break
    fi
  done
  if [ -n "$failed_status" ]; then
    r=$(basename "$failed_status" .status)
    status=$(cat "$failed_status")
    echo "room $r failed with exit $status"
    echo "  log: $W/logs/$step/$r.log"
    echo "  last output:"
    tail -n 30 "$W/logs/$step/$r.log" | sed 's/^/    /'
    printf '\n  room %s failed (exit %s)\n' "$r" "$status" >&3
    return 1
  fi
  printf '\r  %4d/%d  %-50s\n' "$tot" "$tot" "rooms done" >&3
}

# ---------------------------------------------------------------------------------------------------------------
step_check() {
  local bad=0
  ok() { printf '  %-34s ok\n' "$1" >&3; }
  no() { printf '  %-34s MISSING: %s\n' "$1" "$2" >&3; bad=1; }
  say "Checking the requirements"
  command -v "$PY" >/dev/null && "$PY" -c 'import sys; sys.exit(sys.version_info < (3, 8))' && ok "Python 3.8+" \
    || no "Python 3.8+" "install Python 3 (python3)"
  for m in numpy:numpy PIL:Pillow yaml:PyYAML cryptography:cryptography; do
    "$PY" -c "import ${m%%:*}" 2>/dev/null && ok "Python package ${m#*:}" || no "Python package ${m#*:}" "$PY -m pip install ${m#*:}"
  done
  command -v ffmpeg >/dev/null && ok "ffmpeg" || no "ffmpeg" "install ffmpeg (sound and music)"
  say "Checking the exports in $HKDATA"
  for d in ex/Scenes ex/GameObject ex/MonoBehaviour ex/Texture2D ex/Sprite ex/AudioClip ex/Font ex/Resources/languages \
           hs/Texture2D Assembly-CSharp; do
    [ -d "$HKDATA/$d" ] && [ -n "$(ls -A "$HKDATA/$d" 2>/dev/null | head -1)" ] && ok "$d/" || no "$d/" "see README.md, 'Export the game'"
  done
  [ -f "$HKDATA/hs/Texture2D/atlas0_325.png" ] && ok "hs/Texture2D/atlas0_325.png" \
    || no "hs/Texture2D/atlas0_325.png" "the Knight's atlas: the texture dump looks different from the one README.md describes"
  if index_required; then
    printf '  %-34s missing: the build will run scripts/01_index_hkdata.sh\n' "index data" >&3
  else
    ok "index data"
  fi
  if [ $COMPILE = 1 ]; then
    say "Checking devkitPro (to build the .3dsx / .cia; --no-compile skips this)"
    [ -n "$DEVKITPRO" ] && [ -n "$DEVKITARM" ] && ok "DEVKITPRO / DEVKITARM set" \
      || no "DEVKITPRO / DEVKITARM" "install devkitPro and open a new terminal (or: export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM)"
    [ -x "$DEVKITARM/bin/arm-none-eabi-gcc" ] && ok "devkitARM compiler" || no "devkitARM compiler" "dkp-pacman -S 3ds-dev"
    command -v mkromfs3ds >/dev/null && ok "mkromfs3ds (the data file)" || no "mkromfs3ds (the data file)" "dkp-pacman -S 3ds-dev"
    for l in libctru citro2d citro3d; do
      ls "$DEVKITPRO"/libctru/lib/lib${l#lib}*.a >/dev/null 2>&1 && ok "$l" || no "$l" "dkp-pacman -S 3ds-dev"
    done
    for t in makerom bannertool; do
      command -v $t >/dev/null && ok "$t (for the .cia)" \
        || printf '  %-34s not found: you get the .3dsx only (README.md says where to get it)\n' "$t (for the .cia)" >&3
    done
  fi
  [ $bad = 0 ] || return 1
}

step_index() {
  # indexes of the export the converters read (written into $HKDATA)
  "$TOP/scripts/01_index_hkdata.sh" --hkdata "$HKDATA" --workers "$JOBS" || return 1
}

index_required() {
  [ "$FORCE_INDEX" = 1 ] && return 0
  for f in \
    "$HKDATA/metamaps.pkl" \
    "$HKDATA/tex2sprites.pkl" \
    "$HKDATA/meta_AudioClip.pkl" \
    "$HKDATA/hs_alpha4.pkl" \
    "$HKDATA/hs_texmatch.pkl" \
    "$HKDATA/uanim_index.pkl" \
    "$HKDATA/meta_GameObject.pkl" \
    "$HKDATA/meta_Material.pkl" \
    "$HKDATA/meta_MonoBehaviour.pkl" \
    "$HKDATA/meta_Sprite.pkl" \
    "$HKDATA/meta_Texture2D.pkl" \
    "$T/data/all_rooms.txt" \
    "$T/data/audio_map.json" \
    "$T/data/cs_sigs.json" \
    "$T/data/fsm_templates.json" \
    "$T/data/particle_textures.json" \
    "$T/data/gg_statues.txt" \
    "$T/data/music_cues.txt" \
    "$T/data/script_guids.json"
  do
    [ -f "$f" ] || return 0
  done
  return 1
}

step_objects() {
  # every room's live objects: enemies, NPCs, interactables, their state machines and the prefabs they spawn;
  # and the Knight's own (build_knight.py)
  rm -rf "$T/data/uoroots"
  mkdir -p "$T/data/uoroots"
  ( cd "$T" && "$PY" build_knight.py "$W/obj" ) || return 1
  run_rooms objects "$PY" "$T/build_objs.py" "$W/obj"
}

step_rooms() {
  # collision, exits, entities and the baked parallax art of every room (the slowest step)
  run_rooms rooms "$PY" "$T/build_room.py" "$W/rooms"
}

install_one() {   # install_one <room>: tools/install_rooms.sh for one room, without the exit linking (done once after)
  local r=$1 B=$W/rooms
  [ -f "$B/$r.room" ] && { "$PY" "$T/patch_room_audio.py" "$B" "$r" || return 1; }
  for ext in .room .lyr _ents.spr _enemies.spr; do [ -f "$B/$r$ext" ] && cp "$B/$r$ext" "$R/rooms/"; done
  [ -f "$R/rooms/$r.lyr" ] && { "$PY" "$T/lyr_v2.py" "$R/rooms/$r.lyr" || return 1; }
  for f in "$R/rooms/$r.room" "$R/rooms/$r.lyr" "$R/rooms/${r}_ents.spr" "$R/rooms/${r}_enemies.spr"; do
    [ -f "$f" ] && { "$PY" "$T/zwrap.py" "$f" || return 1; }
  done
  # invisible tiles dropped, one-colour tiles drawn as rectangles (the Old 3DS's tile cache holds a screen of them)
  [ -f "$R/rooms/$r.lyr" ] && { "$PY" "$T/lyr_solidify.py" "$R/rooms/$r.lyr" || return 1; }
  "$PY" "$T/camlocks.py" "$R/rooms" "$r" && "$PY" "$T/masks.py" "$R/rooms" "$r" || return 1
  [ -f "$R/rooms/${r}_msk.spr" ] && { "$PY" "$T/zwrap.py" "$R/rooms/${r}_msk.spr" || return 1; }
  return 0
}
step_install() {
  # into romfs/rooms: HKL2 layers, zlib wrapping, camera locks, secret masks; then link every exit to its room
  mkdir -p "$R/rooms"
  "$TOP/scripts/04_install_rooms.sh" --room-build "$W/rooms" --romfs "$R" --jobs "$JOBS" $(cat "$ROOMS") || return 1
}

step_pack() {
  mkdir -p "$R/sfx"
  "$PY" "$T/pack_rooms.py" --objdir "$W/obj" --romfs "$R" --jobs "$JOBS" _Knight $(cat "$ROOMS") || return 1
}

step_particles() {
  # the rooms' own particle systems (<Room>.ptc) and the area particles (_zones.ptc)
  run_rooms particles "$PY" "$T/particles.py" "$R/rooms"
  ( cd "$T" && "$PY" particles.py --zones "$R" ) || return 1
}

step_gamedata() {
  local G=$R/gfx X=$R/text; mkdir -p "$G" "$X" "$R/music"
  cd "$T" || return 1
  "$PY" build_sprites.py "$G" knight enemies hud \
  && "$PY" build_ui.py "$G" && "$PY" build_ui.py "$G" dialog \
  && "$PY" build_icons.py "$G" && "$PY" build_areas.py "$G" && "$PY" build_stagmenu.py "$G" \
  && "$PY" build_map.py "$R" && "$PY" build_journal.py "$R" \
  && "$PY" build_vignette.py "$R" && "$PY" build_herolight.py "$R" "$ROOMS" \
  && "$PY" text_sheets.py "$HKDATA/ex/Resources/languages" "$X" EN && "$PY" text_sheets.py "$HKDATA/ex/Resources/languages" "$X" ES \
  && "$PY" port_text.py "$X" \
  && "$PY" darkness.py "$R" "$ROOMS" && "$PY" nonslider.py "$R" "$ROOMS" \
  && "$PY" pantheons.py "$R" && "$PY" gg_doors.py "$R" \
  && "$PY" additive.py "$X/additive.txt" && "$PY" area_titles.py "$X/area_titles.txt" \
  && cp data/gg_statues.txt data/music_cues.txt "$X/" \
  && "$PY" build_music.py "$R/music" \
  && "$PY" zwrap.py "$G"/knight.spr "$G"/enemies.spr "$G"/hud.spr "$G"/ui.spr "$G"/dialog.spr "$G"/font_text.spr \
                    "$G"/font_menu.spr "$G"/font_big.spr "$G"/stagmenu.spr \
  || return 1
}

step_compile() {
  [ $COMPILE = 1 ] || { say "  (--no-compile: skipped)"; return 0; }
  cd "$TOP" || return 1
  "$PY" "$T/make_icons.py" "$TOP" || return 1      # icon.png / banners of your own if there are none yet
  make && make data || return 1                    # hk3ds.3dsx (the code) and hk3ds_std.romfs (the game data)
  "$PY" "$T/check_romfs.py" "$R" --rooms "$ROOMS" || return 1
  if command -v makerom >/dev/null && command -v bannertool >/dev/null; then make cia || return 1; fi
}

# HD: romfs_hd/ starts as a hard-linked copy of romfs/; these steps replace files in it with twice-the-detail ones
step_hd_copy() { rm -rf "$RH" && cp -al "$R" "$RH"; }
hd_room() {   # the room's art again at twice the detail, in a copy of its standard build (collision and exits stay)
  local r=$1; mkdir -p "$W/rooms_hd"
  [ -f "$W/rooms/$r.room" ] || return 0          # boss and other add-on scenes have no art of their own
  cp "$W/rooms/$r.room" "$W/rooms_hd/$r.room"
  HK_XD=2 "$PY" "$T/rebake_layers.py" "$W/rooms_hd" "$r" || return 1
  [ -f "$W/rooms_hd/$r.lyr" ] || return 0
  "$PY" "$T/lyr_v2.py" "$W/rooms_hd/$r.lyr" && "$PY" "$T/zwrap.py" "$W/rooms_hd/$r.lyr" && "$PY" "$T/lyr_solidify.py" "$W/rooms_hd/$r.lyr" \
    && rm -f "$RH/rooms/$r.lyr" && cp "$W/rooms_hd/$r.lyr" "$RH/rooms/$r.lyr"
}
step_hd_rooms() { run_rooms hd_rooms hd_room; }
step_hd_objects() { run_rooms hd_objects "$PY" "$T/pack_objs_hd.py" "$R" "$RH"; }
step_hd_gamedata() {
  local d=$W/hd_sprites; mkdir -p "$d"
  ( cd "$T" && HK_XD=2 "$PY" build_sprites.py "$d" knight enemies && "$PY" spr_to_rgba4.py "$d/knight.spr" "$d/enemies.spr" \
    && "$PY" zwrap.py "$d/knight.spr" "$d/enemies.spr" \
    && for f in knight enemies; do rm -f "$RH/gfx/$f.spr"; cp "$d/$f.spr" "$RH/gfx/$f.spr"; done \
    && "$PY" build_ui_hd.py "$R" "$RH" \
    && "$PY" zwrap.py "$RH"/gfx/hud.spr "$RH"/gfx/ui.spr "$RH"/gfx/dialog.spr "$RH"/gfx/font_text.spr "$RH"/gfx/font_menu.spr \
                      "$RH"/gfx/font_big.spr ) || return 1
}
step_hd_compile() {
  [ $COMPILE = 1 ] || { say "  (--no-compile: skipped)"; return 0; }
  cd "$TOP" && "$PY" "$T/make_icons.py" "$TOP" && make HD=1 && make HD=1 data || return 1
  "$PY" "$T/check_romfs.py" "$RH" --rooms "$ROOMS" || return 1
  if command -v makerom >/dev/null && command -v bannertool >/dev/null; then make HD=1 cia || return 1; fi
}

# ---------------------------------------------------------------------------------------------------------------
ALL="$STEPS"; [ $HD = 1 ] && ALL="$STEPS $HDSTEPS"
if [ -n "$FROM" ]; then
  case " $STEPS $HDSTEPS " in *" $FROM "*) ;; *) echo "no step '$FROM' (scripts/00_build.sh --list)"; exit 2 ;; esac
  on=0; for s in $ALL; do [ "$s" = "$FROM" ] && on=1; [ $on = 1 ] && rm -rf "$W/done/$s" "$W/done/$s.rooms"; done
fi
if [ -n "$ONLY" ]; then
  case " $STEPS $HDSTEPS " in *" $ONLY "*) ;; *) echo "no step '$ONLY' (scripts/00_build.sh --list)"; exit 2 ;; esac
  rm -rf "$W/done/$ONLY" "$W/done/$ONLY.rooms"; ALL=$ONLY
fi

case "$JOBS" in ''|*[!0-9]*|0) echo "--jobs needs a number of rooms, like --jobs 4"; exit 2 ;; esac
if [ "$ONLY" = "index" ]; then FORCE_INDEX=1; fi
step_check || fail "requirements: install what's marked MISSING above"
[ $CHECKONLY = 1 ] && {
  if index_required; then echo "All requirements found; index data is missing and scripts/01_index_hkdata.sh will run during a build."; else echo "All requirements found."; fi
  exit 0
}
if index_required; then
  rm -f "$W/done/index"
  rm -rf "$W/done/index.rooms"
  : > "$W/logs/index.log"
  if ! run_logged "index" "$W/logs/index.log" step_index; then fail "step 'index'" "$W/logs/index.log"; fi
  done_mark "index"
fi
[ -f "$ROOMS" ] || fail "room list missing after indexing" "$W/logs/index.log"
echo "HK3DS build: exports in $HKDATA, rooms from $ROOMS ($(grep -c . "$ROOMS")), $JOBS at a time"
for s in $ALL; do
  [ "$s" = check ] && continue
  is_done "$s" && { echo "== $s (done before)"; continue; }
  : > "$W/logs/$s.log"
  if ! run_logged "$s" "$W/logs/$s.log" "step_$s"; then fail "step '$s'" "$W/logs/$s.log"; fi
  done_mark "$s"
done
echo
echo "Done."
if [ -f "$TOP/hk3ds_std.romfs" ]; then
  echo "On your SD card (README.md, 'Installing'):"
  echo "  SD:/3ds/hk3ds/hk3ds_std.romfs     <- $TOP/hk3ds_std.romfs  (the game data)"
  echo "  SD:/3ds/hk3ds/hk3ds.3dsx          <- $TOP/hk3ds.3dsx       (for the Homebrew Launcher)"
  [ -f "$TOP/hk3ds.cia" ] && echo "  install with FBI                    $TOP/hk3ds.cia"
fi
if [ $HD = 1 ] && [ -f "$TOP/hk3ds_hd.romfs" ]; then
  echo "  SD:/3ds/hk3ds/hk3ds_hd.romfs      <- $TOP/hk3ds_hd.romfs   (the HD game data)"
  [ -f "$TOP/hk3ds_hd.cia" ] && echo "  install with FBI                    $TOP/hk3ds_hd.cia"
fi
exit 0
