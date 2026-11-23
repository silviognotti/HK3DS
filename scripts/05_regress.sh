#!/bin/bash
# Regression tests on the PC build (host/hk3ds_host): every one must pass before a release.
#   scripts/05_regress.sh [--build DIR]                             (prints PASS / FAIL per test)
H=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$H/build
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${2:?usage: regress.sh [--build DIR]}"); shift 2 ;;
    --build=*) BUILD=$(python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "${1#*=}"); shift ;;
    *) echo "usage: regress.sh [--build DIR]" >&2; exit 2 ;;
  esac
done
B=$BUILD
ROMFS=$BUILD/romfs
W=$(mktemp -d)
fails=0
run() {   # run <name> <HK_START> <script> [extra env...]: leaves the log in $W/<name>/log
  local n=$1 st=$2 sc=$3; shift 3
  rm -rf "$W/$n"; mkdir -p "$W/$n"; ln -s "${HK_ROMFS:-$ROMFS}" "$W/$n/romfs"
  (cd "$W/$n" && env HK_FAST=1 HK_FSMTRACE=1 HK_START="$st" HK_SCRIPT="150:;1:A;40:;1:A;30:;$sc" HK_OUT=shots "$@" timeout 600 "${HK_HOST:-$H/host/hk3ds_host}" > log 2>&1)
}
check() { if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; fails=$((fails+1)); fi; }

# exits: every gate whose destination is installed has its target
n=$(CHECK_ONLY=1 python3 "$H/tools/fill_gate_targets.py" "$ROMFS/rooms" "$B" | awk '{print $2}')
check "all room exits linked ($n missing)" '[ "$n" = 0 ]'

# pack_objs.py writes a fresh <Room>.obj: a room with object particle art (<Room>_pto.spr) whose .obj has no particle
# systems was packed again without tools/obj_particles.py (Fungoons' gas went invisible that way)
np=$(python3 - "${HK_ROMFS:-$ROMFS}/rooms" <<'EOF'
import sys, os, glob, zlib
d = sys.argv[1]; bad = []
for p in glob.glob(f'{d}/*_pto.spr'):
    o = p[:-len('_pto.spr')] + '.obj'
    if not os.path.exists(o): continue
    b = open(o, 'rb').read(); b = zlib.decompress(b[8:]) if b[:4] == b'HKZ1' else b
    if b'"k":"ps"' not in b and b'"k": "ps"' not in b: bad.append(os.path.basename(o)[:-4])
print(len(bad), ' '.join(sorted(bad)[:5]))
EOF
)
check "rooms with object particle art keep their particle systems (${np})" '[ "${np%% *}" = 0 ]'

# the same for the per-frame collider tables (tools/tk_colliders.py: enemies' attack boxes follow their animation):
# running it again over a copy of the rooms must change nothing (25 re-packed rooms had lost them before v1.0.25)
mkdir -p "$W/tk"; cp "${HK_ROMFS:-$H/romfs}"/rooms/*.obj "$W/tk/"
python3 "$H/tools/tk_colliders.py" "$W"/tk/*.obj > /dev/null 2>&1
nt=0; tkl=""; for f in "$W"/tk/*.obj; do cmp -s "$f" "${HK_ROMFS:-$H/romfs}/rooms/$(basename "$f")" || { nt=$((nt+1)); tkl="$tkl $(basename "$f" .obj)"; }; done
check "rooms keep their per-frame collider tables ($nt:$(echo $tkl | cut -c1-60))" '[ "$nt" = 0 ]'
rm -rf "$W/tk"

run well "Town 185 4" "600:"
check "Dirtmouth well drops into the Crossroads" 'grep -q "enter_room(Crossroads_01, top1)" $W/well/log'

run kp_exit "Tutorial_01 189 66" "300:;4:RIGHT;10:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;1:Y;25:;80:RIGHT;400:"
check "King's Pass great door breaks into Dirtmouth" 'grep -q "enter_room(Town" $W/kp_exit/log'

# King's Pass chest: open it, take the charm, read the tutorial; afterwards no burst or ring left on screen
run chest "Tutorial_01 181 18" "200:;4:LEFT;10:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;80:;1:UP;200:;1:A;150:;1:A;150:;1:A;150:;1:A;600:" HK_OBJDUMP=1500
check "chest opens" 'grep -q "Chest Control: Range? -> Open" $W/chest/log'
check "charm picked up (tutorial shown and closed)" 'grep -q "Charm Tute Msg: Regain Control? -> Destroy" $W/chest/log'
check "item burst gone afterwards" '! grep "^\[dump f1500\]" $W/chest/log | grep -q " Soul Burst "'
check "shiny ring gone afterwards" '! grep "^\[dump f1500\]" $W/chest/log | grep " White Wave " | grep -qv "a=0.00"'
check "charm owned" 'grep -q "Shiny Control: Get Charm" $W/chest/log'

# focus tablet: read it, the prompt fades in after its delay and out after reading
run tablet "Tutorial_01 110 34" "250:;1:UP;500:;1:A;150:;1:A;150:;1:A;150:;400:" HK_OBJDUMP=700
check "tablet text read and control regained" 'grep -q "Inspection: Turn Back -> Regain Control" $W/tablet/log'
check "focus prompt shown while reading" 'grep "^\[dump f700\]" $W/tablet/log | grep "Collect SOUL" | grep -qv "ta=0.00"'

# an enemy kill: blood and effects clean themselves up
run kill "Tutorial_01 118 11" "1500:" HK_GOD=1 HK_AUTOFIGHT=1 HK_OBJDUMP=1300
check "Crawler killed" 'grep -q "Crawler 1 dmg=.* hp=-" $W/kill/log'
check "no blood left after the fight" '! grep "^\[dump f1300\]" $W/kill/log | grep -q "Spatter Orange"'

# camera: walking from the chest toward the hole keeps the chest room's camera zone (polygon zones next to it)
run cam "Tutorial_01 177 18" "250:;70:LEFT;200:" HK_TRACE=1
check "camera stays in the chest room's zone" '[ "$(grep "^t=" $W/cam/log | tail -1 | sed "s/.*cam=(\([0-9.]*\),.*/\1/" | cut -d. -f1)" -ge 172 ]'

# New Game: the opening fall lands on the floor (the landing animation and sound play at the bottom, not mid-air)
run intro "" "400:" HK_TRACE=1 HK_TRACEN=1
check "opening fall lands at the floor" '[ "$(awk "/Initial Fall Impact\/Control: Idle -> Land/{print y; exit} /^t=/{split(\$2,a,\",\"); y=a[2]+0}" $W/intro/log | cut -d. -f1)" -le 13 ]'

# Crossroads entry: walking left from the well's landing keeps the camera on the Knight (lock zones fall back properly)
run xcam "Town 185 4" "300:;80:LEFT;200:" HK_TRACE=1
check "Crossroads camera follows the Knight after leaving the entry zone" '[ "$(grep "^t=" $W/xcam/log | tail -1 | sed "s/.*cam=(\([0-9.]*\),.*/\1/" | cut -d. -f1)" -ge 30 ]'

run xcamR "Town 185 4" "300:;300:RIGHT;100:" HK_TRACE=1
check "Crossroads camera follows the Knight walking right" '[ "$(python3 $H/tools/camcheck.py $W/xcamR/log 70)" = 1 ]'

# Divine's tent door in Dirtmouth only works once the troupe has arrived
run divine "Town 66 11" "200:;150:RIGHT;300:"
check "no invisible door where Divine's tent isn't" '! grep -q "enter_room(Grimm_Divine" $W/divine/log'

# Baldur (Roller) in the Ancestral Mound: rolls into the Knight and hurts him; a nail hit knocks it back
run roller_dmg "Crossroads_ShamanTemple 95.5 24.5" "400:" HK_TRACE=1
check "rolling Baldur damages the Knight" 'grep "^t=" $W/roller_dmg/log | tail -1 | grep -q "hp=4 "'
S=""; for i in $(seq 20); do S="$S;1:Y;12:"; done
run roller_kb "Crossroads_ShamanTemple 95.5 24.5" "20:$S;100:" HK_UOTRACE="Roller 3"
check "nail hit knocks the rolling Baldur back" '[ "$(awk "/^\[hit [0-9]+\] Roller 3/{h=1; next} /^\[uo [0-9]+\] Roller 3/{split(\$5,a,\"[(,]\"); x=a[2]+0; if(h==1){h=2; x0=last} else if(h==2 && x>x0+1.5) ok=1; last=x} END{print ok+0}" $W/roller_kb/log)" = 1 ]'

# Ancestral Mound: the Snail Shaman is there, entering through the real door from the Crossroads
run shaman "Crossroads_ShamanTemple:left1 -1 0" "250:" HK_OBJDUMP=120
check "Snail Shaman present in the Ancestral Mound" 'grep "^\[dump f120\]" $W/shaman/log | grep -q " Shaman Meeting pos="'

# Vengeful Spirit: talk to the Snail Shaman, take the spell; the Knight wakes outside the hut, visible and in control
S=";1:UP;120:"; for i in $(seq 12); do S="$S;1:A;60:"; done
run spell "Crossroads_ShamanTemple 22 9.5" "150:$S;300:;14:LEFT;25:B;800:;1:A;100:;1:A;100:;1:A;300:;60:RIGHT;100:" HK_TRACE=1
check "Vengeful Spirit cutscene finishes and gives control back" 'grep -q "Check Fall: Get Up -> Regain Control" $W/spell/log'
check "the Knight wakes up in the room (not fallen out of it)" 'grep "^t=" $W/spell/log | tail -1 | grep -q "P=(4[0-9]\.[0-9]*,[0-9]\."'

# Crossroads hot spring: sitting in the water refills SOUL
run spa "Crossroads_30 25 5" "600:" HK_TRACE=1
check "hot spring refills SOUL" 'grep "^t=" $W/spa/log | tail -1 | grep -q "soul=99 "'

# benches save: a plain one (Crossroads hot spring) and a script-driven one (Ancestral Mound)
run bench1 "Crossroads_30 37.94 5.3" "100:;1:UP;300:"
check "Crossroads hot spring bench saves" 'sed -n 5p $W/bench1/sdmc/3ds/hk3ds/save1.dat | grep -q "^Crossroads_30 .* 1$"'
run bench2 "Crossroads_ShamanTemple 15.09 8.3" "100:;1:UP;300:"
check "Ancestral Mound bench saves" 'sed -n 5p $W/bench2/sdmc/3ds/hk3ds/save1.dat | grep -q "^Crossroads_ShamanTemple .* 1$"'

# False Knight beaten: the arena's battle gates open again when you come back
run fkgates "Crossroads_10 11 27" "100:;150:RIGHT;300:" HK_PD=falseKnightDefeated
check "battle gates stay open after the False Knight" 'grep -q "Battle Gate 2/BG Control: Quick Close -> Quick Open" $W/fkgates/log'

# secret-area covers: an inverse remasker (a cover shown only while you're inside a secret) no longer blacks out the room
run remask "Deepnest_44:top1 -1 0" "200:" HK_SHOTS=380 HK_OUT=shots
# (a cover leaves only the Knight showing: judged by how much of the view shows anything, not by its average
# brightness, which the black terrain and the darker Deepnest of v1.0.15 brought down to ~8)
check "no black cover over Deepnest_44's entrance" '[ "$(python3 -c "from PIL import Image; im=Image.open(\"$W/remask/shots/f00380.png\").crop((0,60,400,240)).convert(\"L\"); d=list(im.getdata()); print(int(sum(v > 12 for v in d) / len(d) > 0.08))")" = 1 ]'

# Dirtmouth's ground under the walkway is the terrain's black (its tilemap is named "Town-TileMap Render Data": missed,
# the fog behind showed through as pale bands under the floor)
run townground "Town 40 11" "300:" HK_SHOTS=500 HK_OUT=shots HK_GOD=1
check "Dirtmouth's ground under the walkway is black" '[ "$(python3 -c "from PIL import Image; im=Image.open(\"$W/townground/shots/f00500.png\").crop((0,220,400,240)).convert(\"L\"); d=list(im.getdata()); print(int(sum(d)/len(d) < 8))")" = 1 ]'

# the Knight's light and spell effects (faint additive glows) stay 8-bit: at 4 bits the light around him shows hard rings
check "Knight effects page is full colour" '[ "$(python3 -c "import zlib,struct; d=open(\"$ROMFS/rooms/_Knight_objs.spr\",\"rb\").read(); d=zlib.decompress(d[8:]) if d[:4]==b\"HKZ1\" else d; print(struct.unpack_from(\"<B\",d,16)[0]&0x7f)")" = 0 ]'

# Elder Baldur: while open it spits goop (the projectile is picked with SetGameObject from a prefab)
run blocker "Crossroads_ShamanTemple 60 23" "500:" HK_SPAWNDBG=1
check "Elder Baldur spits goop" 'grep -q "\[spawn\] Blocker Control ti=[0-9]* -> Shot Mawlek" $W/blocker/log'

# Gruzzers bounce inside their room, also at the 3DS's lower frame rates (a doorway used to let them fly off forever)
run gruzzer "Crossroads_07 14 40" "1200:" HK_FPS=20 HK_GOD=1 HK_UOTRACE="Fly 4"
check "Gruzzer stays in the room at 20 fps" '[ "$(grep "^\[uo" $W/gruzzer/log | sed "s/.*pos=(\([-0-9.]*\),.*/\1/" | awk "\$1<-0.5||\$1>43.5{bad=1} END{print bad+0}")" = 0 ]'

# Sly's shop opens with its stock (and the PC build draws it; the 3DS no longer hides it behind the dimmer)
run shop "Room_shop 15.6 7.2" "100:;1:UP;200:;1:A;60:;1:A;60:;1:A;60:;1:A;120:" HK_PD="geo=500"
check "Sly's shop opens with items" 'grep -q "\[shop\] open: [1-9]" $W/shop/log'

# Myla: her idle plays from the start (auto-played clips used to sit frozen) and her humming loop runs
run myla "Crossroads_45 40 4" "300:" HK_LOOPDBG=1 HK_UOTRACE="Miner"
check "Myla animates before you talk to her" 'grep "^\[uo [0-9]*\] Miner " $W/myla/log | grep -q " fr=[1-9]"'
check "Myla hums" 'grep -q "\[loop\] start Miner_idle_hum on Miner" $W/myla/log'
check "Myla's pickaxe cycle runs (sprite ids)" 'grep -q "Miner/Miner FX: Choice -> " $W/myla/log'

# Goam: its box follows the sprite's per-frame collider, so it hurts when it bursts out of the ground
run goam "Crossroads_13 45.5 9" "60:;20:LEFT;300:" HK_TRACE=1
check "Goam hurts the Knight" 'grep "^t=" $W/goam/log | tail -1 | grep -qv "hp=5 "'

# Grubfather's room: the saved grubs sit at their holes' depth and the Grubfather is drawn (not behind the wall layer)
check "saved grubs at their holes' depth" '[ "$(python3 -c "import sys; sys.path.insert(0,\"$H/tools\"); from merge_obj_update import load_obj; t=load_obj(\"$ROMFS/rooms/Crossroads_38.obj\"); print(min(float(o.get(\"z\",0)) for o in t[\"objects\"] if o.get(\"p\")==-1 and o[\"n\"].startswith(\"Grub Saved\")) > 1)")" = True ]'
run gf "Crossroads_38 30 6" "300:" HK_SHOTS=400
run gf_hidden "Crossroads_38 30 6" "300:" HK_SHOTS=400 HK_HIDEOBJ="Grub King"
check "Grubfather visible" '[ "$(python3 -c "from PIL import Image, ImageChops; A=Image.open(\"$W/gf/shots/f00400.png\"); k=(A.width-320)//400; r=(280*k,30,340*k,90); a=A.crop(r).convert(\"L\"); b=Image.open(\"$W/gf_hidden/shots/f00400.png\").crop(r).convert(\"L\"); d=list(ImageChops.difference(a,b).getdata()); print(int(sum(d)/len(d) > 3))" 2>/dev/null)" = 1 ]'

# Gruz Mother: her corpse bursts into babies (it finds "Fly Spawn" by name); killing them opens the gates, and the
# death burst's splat (a Mecanim Animator clip) turns orange and fades instead of staying as a big white shape
run gruzmom "Crossroads_04 90 17" "3000:" HK_GOD=1 HK_AUTOFIGHT=1 HK_OBJDUMP=2000
check "Gruz Mother's babies hatch" 'grep -q "^\[hit [0-9]*\] Fly [1-6] " $W/gruzmom/log'
check "Gruz Mother battle ends (gates open)" 'grep -q "Battle Scene/Battle Control: End Wait -> End" $W/gruzmom/log'
check "no white splat left after Gruz Mother" '! grep "^\[dump f2000\]" $W/gruzmom/log | grep " Splat Explode Orange " | grep -qv "a=0.00"'

# King's Pass great door: each nail hit shows the next, more cracked frame (Tk2dSpriteSetId by sprite name)
S=""; for i in $(seq 4); do S="$S;1:Y;25:"; done
run door "Tutorial_01 189 66" "300:;4:RIGHT;10:$S$S;100:" HK_OBJDUMP=-100
check "great door cracks as you hit it" '[ "$(grep "^\[dump" $W/door/log | grep " Door pos" | sed "s/.*srf=\([-0-9]*\).*/\1/" | sort -u | wc -l)" -ge 3 ]'

# City of Tears lift: hitting the call lever brings it down (TranslateV2, stopping at its yMin)
run lift "Ruins1_02 8.6 12.9" "100:;2:RIGHT;5:;1:Y;30:;1:Y;30:;1:Y;600:" HK_UOTRACE="Ruins Lift 1"
check "City of Tears lift answers its call lever" 'grep "^\[uo" $W/lift/log | tail -1 | grep -q "pos=(5.[0-9]*,15.9"'

# Brooding Mawlek: walking into its arena wakes it and starts the battle
run mawlek "Crossroads_09 44 7" "100:RIGHT;300:"
check "Brooding Mawlek wakes when you walk in" 'grep -q "Battle Scene/Battle Control: Detect -> Start" $W/mawlek/log'

# the Collector's jars drop and break into the enemies inside them (SpawnJarControl)
run collector "Ruins2_11 54.34 104.75" "4000:" HK_GOD=1 HK_AUTOFIGHT=1 HK_HITALL="21:20"
check "Collector's jars release enemies" 'grep -qE "^\[hit [0-9]+\] (Spitter|Roller|Buzzer) R" $W/collector/log'

# Soul Twister: picks a "Teleplane" (an object found by its tag) and teleports there, instead of freezing
run mage "Ruins1_09 31.33 26.5" "2000:" HK_GOD=1 HK_AUTOFIGHT=1
check "Soul Twister teleports" 'grep -q "Mage/Mage: .* -> Teleport$" $W/mage/log && ! grep -q "WARN fsm loop Mage" $W/mage/log'

# Grubfather: with several grubs saved he hands out every reward in a row (each one checks the loose-geo cap first)
run grubdad "Crossroads_38 36 6" "700:" HK_PD="grubsCollected=10"
check "Grubfather gives more than one reward" '[ "$(grep -c "King Control: At cap? -> Gift Anim" $W/grubdad/log)" -ge 2 ]'

# a freed grub stays freed: its bottle renames itself "Collected", but it is saved under its starting name
run grub "Crossroads_48 48 4" "100:;2:RIGHT;5:;1:Y;30:;1:Y;200:;600:LEFT;200:" HK_PDTRACE=1
check "freed grub is remembered after leaving the room" 'grep -q "\[pd\] pbi:Crossroads_48:Grub Bottle = 1" $W/grub/log'

# focus holds the Knight still even with a direction held (an old stand-in ended every focus one frame in: he ran off mid-focus)
run focus "Crossroads_ShamanTemple 30 9.5" "60:;40:A;110:A+RIGHT;1:" HK_TRACE=1 HK_PD="MPCharge=99"
check "the Knight stays put while focusing" 'grep "^t=" $W/focus/log | tail -1 | grep -q "P=(30\.00,"'
# entering a room always switches pausing back on (a tablet read cut off by an exit left pausing blocked until a restart)
run pauseent "Crossroads_ShamanTemple:left1 -1 0" "150:" HK_PD="disablePause" HK_PDTRACE=1
check "entering a room re-enables pausing" '[ "$(grep "\[pd\] disablePause" $W/pauseent/log | tail -1)" = "[pd] disablePause = 0" ]'
# no casting once an exit has fired (the cast used to start during the fade and finish in the next room)
run exitcast "Crossroads_ShamanTemple:left1 -1 0" "200:;12:LEFT;1:LEFT+A;60:LEFT;200:" HK_PD="fireballLevel=1,MPCharge=99"
check "no cast starts after an exit fires" '! awk "/\[gate\] left1 fired/{g=1} g && /Spell Control: Can Cast\? -> Spell Choice/{bad=1} /enter_room\(Crossroads_06/{g=0} END{exit !bad}" $W/exitcast/log'

# after Vengeful Spirit the Ancestral Mound's Bone Gate keeps the Knight in until the Elder Baldur is beaten (it's on the
# Soft Terrain layer, which only the Knight collides with: it used to let him walk straight out)
run bonegate "Crossroads_ShamanTemple 22 9.5" "150:;1:UP;120:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;1:A;60:;300:;14:LEFT;25:B;800:;1:A;100:;1:A;100:;1:A;300:;60:RIGHT;100:;200:LEFT;30:" HK_TRACE=1
check "the Bone Gate stops the Knight after he gets the spell" 'grep "^t=" $W/bonegate/log | tail -1 | awk -F"[(,]" "{exit !(\$2 > 42.8)}"'
# enemy hitboxes that tk2d switches on per animation frame: the False Knight's mace and the Husk Guard's club
run fkmace "Crossroads_10 11 27" "150:RIGHT;2500:" HK_TANK=1 HK_DMGTRACE=1
check "the False Knight's mace hurts" 'grep -q "^\[dmg [0-9]*\] Hitter (False Knight New)" $W/fkmace/log'
run hgclub "Crossroads_48 28 5" "20:RIGHT;1:Y;20:;1:Y;20:;1:Y;1500:" HK_TANK=1 HK_DMGTRACE=1
check "the Husk Guard's club hurts" 'grep -q "^\[dmg [0-9]*\] Swipe (Zombie Guard)" $W/hgclub/log'

# a fireball's recoil pushing the Knight through an exit: the cast is cancelled as he leaves (it used to carry on into the
# next room with its recoil state holding him for good: stuck in place or in the air)
run castexit "Crossroads_ShamanTemple:left1 -1 0" "200:;10:LEFT;1:RIGHT;1:A;300:" HK_PD="fireballLevel=1,MPCharge=99"
check "a cast through an exit ends cleanly" 'grep -q "\[gate\] left1" $W/castexit/log && grep -q "Spell Control: Cancel Some" $W/castexit/log && ! grep -q watchdog $W/castexit/sdmc/3ds/hk3ds/log.txt && grep "Knight/Spell Control" $W/castexit/log | tail -1 | grep -q "Inactive"'

# a charm picked up after the first one: the message names it (the message's script ran before the shiny gave it the
# charm's number: CHARM_NAME_0)
run charmname "Crossroads_ShamanTemple 37.81 21.12" "120:;1:UP;10:;1:UP;200:;1:A;120:;1:A;120:" HK_GOD=1 HK_TEXTTRACE=1 HK_PD="hasCharm"
check "Soul Catcher's pickup message names it" 'grep -q "^\[text [0-9]*\] Text: Soul Catcher" $W/charmname/log'
# Spanish: the same pickup in the game's Spanish text (romfs/text/es), and the menus from its MainMenu sheet
run charmname_es "Crossroads_ShamanTemple 37.81 21.12" "120:;1:UP;10:;1:UP;200:;1:A;120:;1:A;120:" HK_GOD=1 HK_TEXTTRACE=1 HK_PD="hasCharm" HK_LANG=1
check "in Spanish the pickup message reads Atrapaalmas" 'grep -q "^\[text [0-9]*\] Text: Atrapaalmas" $W/charmname_es/log'

# a Husk Guard the save records as killed stays gone when the room switches its group on (it came back already dead:
# walking around, the nail bouncing off it)
run guarddead "Crossroads_21 64 4" "600:" HK_GOD=1 HK_PD="pbs:Crossroads_21:Zombie Guard"
check "a killed Husk Guard doesn't come back" '! grep -q "Zombie Guard/Zombie Guard: Dormant -> Wake" $W/guarddead/log'
run guardalive "Crossroads_21 64 4" "600:" HK_GOD=1
check "a living Husk Guard still wakes up" 'grep -q "Zombie Guard/Zombie Guard: Dormant -> Wake" $W/guardalive/log'

# Vengeful Spirit cast in the air: the Knight hangs for the recoil, is pushed back, then falls (gravity comes back)
run aircast "Crossroads_ShamanTemple 30 9.5" "120:;12:B;1:A;200:" HK_TRACE=1 HK_PD="fireballLevel=1,MPCharge=99"
check "Knight falls again after an air cast" 'grep "^t=" $W/aircast/log | tail -1 | grep -q "ground=1"'
check "fireball recoil plays out" 'awk "/Spell Control: Fireball 1 -> Fireball Recoil/{split(\$2,a,\"]\"); s=a[1]+0} /Spell Control: Fireball Recoil -> Spell End/{split(\$2,a,\"]\"); e=a[1]+0} END{exit !(e-s>=10)}" $W/aircast/log'

# breakable walls on the terrain layer take nail hits: Crossroads_03's wall breaks and the secret area (and its grub) is uncovered
S=""; for i in $(seq 8); do S="$S;1:Y;20:"; done
run bwall "Crossroads_03 17.6 39" "60:;2:LEFT;5:$S;100:"
check "breakable wall breaks and uncovers the secret area" 'grep -q "Break Wall 2/FSM: Pause Frame -> Break" $W/bwall/log && grep -q "crossroads_03_mask/unmasker: Idle -> Fade" $W/bwall/log'

# corpses stop sliding once they land (ground friction): a Husk Guard's corpse used to slide across the room
run guardcorpse "Crossroads_48 36 5.9" "1500:" HK_GOD=1 HK_AUTOFIGHT="Zombie Guard" HK_OBJDUMP=-60
check "Husk Guard corpse comes to rest" '[ "$(grep "^\[dump" $W/guardcorpse/log | grep " Corpse Zombie Guard pos" | tail -3 | sed "s/.*pos=(\([-0-9.]*\),.*/\1/" | sort -u | wc -l)" = 1 ]'

# False Knight: the barrels' splats are orange and fade (they stayed as big white shapes), and after the death his
# armour, head and staff stay on the floor (they fell out of the world once the death steam switched his collider off)
run fkfight "Crossroads_10 11 27" "150:RIGHT;4300:" HK_GOD=1 HK_AUTOFIGHT=1 HK_OBJDUMP=-200
check "False Knight dies (death sequence completes)" 'grep -q "FalseyControl: Decrement Battle Enemies -> Cough" $W/fkfight/log'
check "False Knight's armour stays on the floor after death" 'grep "^\[dump" $W/fkfight/log | grep " False Knight New pos" | tail -1 | grep -q "pos=([0-9.]*,[0-9]"'

# Watcher Knights: their boss scene only loads when the Knight drops in through the top door (it was checked against the
# previous room's arrival door, so the fight never appeared); dropping into the arena starts it, and all six can be beaten
run watchers "Ruins2_03:top1 60.2 85" "14000:" HK_GOD=1 HK_AUTOFIGHT=1 HK_AUTOFIGHT_AFTER=300 HK_HITALL="21:20" HK_PDTRACE=1
check "Watcher Knights' fight starts when dropping in from the top" 'grep -q "Battle Control/Battle Control: Idle -> Close" $W/watchers/log'
check "Watcher Knights' fight ends and is remembered" 'grep -q "Battle Control/Battle Control: Music End -> End" $W/watchers/log && grep -q "\[pd\] pbi:Ruins2_03:Battle Control = 1" $W/watchers/log'

# the Hollow Knight: breaking the four chains starts the fight, he stays in the arena (his root collider is off and a child
# collider holds him: he fell out of the world), and after the fight focusing on his body gives the first ending
S=""; for i in $(seq 12); do S="$S;3000:A;30:"; done
run thk "Room_Final_Boss_Core 40.58 9.88" "6000:$S" HK_GOD=1 HK_AUTOFIGHT=1 HK_BOTTARGETS="hollow_knight_chain_base" HK_HITALL="21:20:Slash|Dash|Jump|Evade|Dstab|Counter|Shot|Recover" HK_PDTRACE=1 HK_UOTRACE="Hollow Knight Boss"
check "Hollow Knight stays in his arena" '! grep "^\[uo .*Hollow Knight Boss pos" $W/thk/log | grep -q "pos=([-0-9.]*,-"'
check "Hollow Knight is beaten and focusing on him ends the game" 'grep -q "Boss Corpse/Corpse: Ending? -> To Ending A" $W/thk/log && grep -q "\[pd\] completedEndingA = 1" $W/thk/log'

# the Radiance (dream): the challenge prompt starts the fight, her phases run to the climb, and the final nail hit gives
# the "Dream No More" ending (the bot can't climb: it is put on the arena's upper floor and next to her for the last hit;
# her hurtbox is a hexagon, not its bounding box: he is put under its bottom point, where an up-slash reaches it)
run radiance "Dream_Final_Boss:door1 60.4 33.2" "200:;6:UP;16000:" HK_PDSTR="dreamReturnScene=Room_Final_Boss_Core" HK_TPSTATE="Abyss Up=60.4,34.5;Ascend Cast=71.6,140.0" HK_GOD=1 HK_AUTOFIGHT=1 HK_AUTOFIGHT_AFTER=1500 HK_HITALL="21:20" HK_PDTRACE=1
check "Radiance fight runs from the challenge to the final hit" 'grep -q "Boss Control/Control: Knight Ready -> Battle Start" $W/radiance/log && grep -q "Radiance/Control: Ascend Cast -> Final Impact" $W/radiance/log'
check "beating the Radiance gives the Dream No More ending" 'grep -q "\[pd\] completedEndingC = 1" $W/radiance/log'

# dreams: dream-nailing the False Knight's body enters his dream, falling out of it wakes the Knight back in the Crossroads,
# and beating the Failed Champion also returns there
run dreamfk "Crossroads_10 31.2 47.6" "60:;3:RIGHT;20:;200:X;900:;400:LEFT;3000:" HK_PD="falseKnightDefeated,hasDreamNail"
check "dream nail on the False Knight's body enters his dream" 'grep -q "enter_room(Dream_01_False_Knight, door1)" $W/dreamfk/log'
check "falling out of a dream returns to the Crossroads" 'grep -q "enter_room(Crossroads_10, door_dreamReturn)" $W/dreamfk/log'
run champion "Dream_01_False_Knight:door1 40 27.5" "11000:" HK_PDSTR="dreamReturnScene=Crossroads_10" HK_GOD=1 HK_AUTOFIGHT=1 HK_HITALL="21:20" HK_PDTRACE=1
check "Failed Champion beaten, the Knight wakes in the Crossroads" 'grep -q "\[pd\] falseKnightDreamDefeated = 1" $W/champion/log && grep -q "enter_room(Crossroads_10, door_dreamReturn)" $W/champion/log'

# doors whose destination is set by their own Door Control script (it wasn't in the room data): Salubra's shop, the Black
# Egg Temple; Salubra sells a charm once inside
run salubra "Crossroads_04 144.06 11.9" "120:;1:UP;600:"
check "Salubra's door opens her shop" 'grep -q "enter_room(Room_Charm_Shop, left1)" $W/salubra/log'
run temple "Crossroads_02 46.47 4.93" "120:;1:UP;600:"
check "the Black Egg Temple door opens" 'grep -q "enter_room(Room_temple, left1)" $W/temple/log'
S=""; for i in $(seq 10); do S="$S;1:A;60:"; done
run salubra2 "Room_Charm_Shop:left1 -1 0" "100:;30:RIGHT;20:;1:UP;200:$S;1:DOWN;30:;1:A;60:;1:A;200:" HK_PD="geo=2000" HK_PDTRACE=1
check "Salubra talks and sells a charm" 'grep -q "\[pd\] metCharmSlug = 1" $W/salubra2/log && grep -q "\[pd\] gotCharm_18 = 1" $W/salubra2/log'

# Crystal Peak's item stand: its inspect region starts just above the Knight's head (Box2D's shape skin reaches it)
run slykey "Mines_11 34.8 41.27" "200:;1:UP;30:;1:UP;30:;1:UP;200:;1:A;120:;1:A;120:;1:A;300:" HK_PDTRACE=1
check "Crystal Peak item stand can be picked up" 'grep -q "\[pd\] hasSlykey = 1" $W/slykey/log'

# the Knight never floats with control and no gravity (playtest: hovering after jumping out of water, only able to move
# sideways): random play with every power in rooms with pools; the game's safety net logs "watchdog" if it had to step in
PDALL="hasDash,canDash,hasWalljump,hasSuperDash,hasDoubleJump,hasAcidArmour,hasShadowDash,fireballLevel=1,quakeLevel=1,screamLevel=1,MPCharge=99,maxMP=99,MPReserveMax=99"
run float1 "Abyss_Lighthouse_room -1 -1" "$(python3 "$H/tools/bot_script_powers.py" 5 4000 | cut -d';' -f6-)" HK_GOD=1 HK_PD="$PDALL"
run float2 "Waterways_06 -1 -1" "$(python3 "$H/tools/bot_script_powers.py" 7 4000 | cut -d';' -f6-)" HK_GOD=1 HK_PD="$PDALL"
run float3 "Fungus1_12 -1 -1" "$(python3 "$H/tools/bot_script_powers.py" 11 4000 | cut -d';' -f6-)" HK_GOD=1 HK_PD="$PDALL"
check "the Knight never floats after swimming or spells (3 rooms of random play with every power)" '! grep -q watchdog $W/float1/sdmc/3ds/hk3ds/log.txt $W/float2/sdmc/3ds/hk3ds/log.txt $W/float3/sdmc/3ds/hk3ds/log.txt'

# Hornet in Kingdom's Edge: the bot talks through her dialogue, the fight starts, and she ends up on the ground to talk to
run hornet2 "Deepnest_East_Hornet 25.28 29.54" "9000:" HK_GOD=1 HK_AUTOFIGHT=1 HK_HITALL="21:20" HK_PDTRACE=1 HK_UOTRACE="Hornet Boss 2"
check "Hornet 2 fight starts after her dialogue" 'grep -q "Encounter: Set Hero Active -> Start Fight" $W/hornet2/log'
check "Hornet 2 beaten and her body can be talked to" 'grep -q "\[pd\] hornetOutskirtsDefeated = 1" $W/hornet2/log && grep -q "Corpse Hornet Outskirts/Control: Land -> Can Talk" $W/hornet2/log'
# her air dash sets her down at the FSM's Floor Y, a unit into the arena floor: the physics pushes her back out (she stayed
# stuck in the ground for the rest of the fight). No two traced samples in a row with her box bottom under the floor (27)
check "Hornet 2 never stays sunk in the floor" '[ "$(grep "^\[uo [0-9]*\] Hornet Boss 2 " $W/hornet2/log | sed -n "s/.*box=(\([-0-9.]*\),\([-0-9.]*\)\.\..*/\2/p" | awk "BEGIN{n=0;m=0} {if (\$1 > 1 && \$1 < 26.9) {n++; if (n>m) m=n} else n=0} END{print (m<2)}")" = 1 ]'

# Mantis Lords: up at the challenge prompt starts the fight; beating the lords opens the way to Deepnest
run mantislords "Fungus2_15 30.2 7.5" "150:;6:UP;9000:" HK_GOD=1 HK_AUTOFIGHT=1 HK_AUTOFIGHT_AFTER=900 HK_HITALL="21:20" HK_PDTRACE=1
check "Mantis Lords challenge, fight and gate opening" 'grep -q "Mantis Throne Main: Idle -> Music" $W/mantislords/log && grep -q "\[pd\] defeatedMantisLords = 1" $W/mantislords/log && grep -q "deepnest_mantis_gate/Gate Control: Challenge -> Open" $W/mantislords/log'

# the Abyss core room's line-of-sight checks: each raycast tests only the terrain objects near its segment (they cost
# a third of a 3DS frame there)
run abyss "Abyss_06_Core 2.6 3.4" "1200:" HK_PROF=600
check "Abyss core room object update stays cheap" '[ "$(grep -o "uo avg [0-9]*" $W/abyss/log | tail -1 | awk "{print \$3}")" -lt 600 ]'

# geo pickup
run geo "Tutorial_01 161 6.5" "200:;6:RIGHT;10:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;1:Y;20:;60:LEFT;60:RIGHT;300:" HK_TRACE=1
check "geo collected" 'grep "^t=" $W/geo/log | tail -1 | grep -qv "geo=0 "'

# King's Pass walls: breaking one sounds (breakable_hit table) and uncovers the passage behind it (mask HIT link)
run kpwall "Tutorial_01 40 12" "8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;8:RIGHT;2:RIGHT+Y;4:RIGHT;" HK_GOD=1 HK_SNDTRACE=1
check "a broken wall makes its sound" 'grep -q "^\[snd [0-9]*\] breakable_wall_hit_1" $W/kpwall/log'
check "breaking Tute Door 1 uncovers the passage behind it" 'grep -q "\[mask\] HIT uncovers Tut_msk_03" $W/kpwall/log'
check "King's Pass has its black covers" '[ -f $ROMFS/rooms/Tutorial_01.msk ] && grep -q "^G .* Secret_Mask " $ROMFS/rooms/Tutorial_01.msk'

# a flying enemy's corpse bursts when it hits the ground (Corpse.breaker -> Smash), with a splat
run flyer "Fungus1_02 28.9 43" "600:" HK_GOD=1 HK_KILLALL=60 HK_SNDTRACE=1
check "a killed Squit's corpse smashes on landing" 'grep -q "\[corpse [0-9]*\] Corpse Mosquito v2 smashes" $W/flyer/log'
check "with a splat" 'grep -qE "^\[snd [0-9]*\] (splat_01|Splat_02|Splat_03)" $W/flyer/log'

# a Shrumal Ogre bursts into chunks where it lands (CorpseDeathStunChunker)
run ogre "Fungus2_05 30 12" "600:" HK_GOD=1 HK_KILLALL=60 HK_SNDTRACE=1
check "a killed Shrumal Ogre bursts" 'grep -q "\[corpse [0-9]*\] Corpse Mushroom Brawler bursts into chunks" $W/ogre/log'

# mushrooms shake with a rustle when the Knight walks through them (TouchShake)
run mush "Fungus2_05 48 13" "60:;80:RIGHT;60:" HK_GOD=1 HK_SNDTRACE=1
check "walking through a mushroom rustles it" 'grep -q "^\[snd [0-9]*\] mushroom_walk_through_" $W/mush/log'

# the Knight's vignette: on in every room (SceneManager sends RESET), small in dark rooms
run vignette "Deepnest_01b -1 -1" "150:" HK_GOD=1 HK_OBJDUMP=90
check "the vignette is drawn (dark room: Idle Scale 2.2)" 'grep -E "^\[dump f90\] [0-9]+ bank=1 Vignette " $W/vignette/log | grep -q "sr=1 .*sc=(-\{0,1\}2.20"'

# Spore Shroom: focusing next to a Crawlid, the spore cloud damages it (DamageEffectTicker -> ExtraDamageable)
run spore "Tutorial_01 116 11" "120:;300:A;100:" HK_PD="equippedCharm_17,gotCharm_17,MPCharge=99,health=3"
check "the spore cloud hurts enemies" 'grep -q "extra damage (type 0) to Crawler" $W/spore/log'

# corpses that go off after landing: Volatile Mosskin gas (which hurts: a damages_hero FSM), Fungified Husk gas,
# Spiny Husk spines, the Aspid Mother's young
run mossgas "Fungus1_02 31 30.6" "700:" HK_KILLALL=10 HK_DMGTRACE=1
check "a Volatile Mosskin's corpse bursts into gas" 'grep -q "\[corpse [0-9]*\] Corpse_Mossman_Shaker explodes" $W/mossgas/log'
check "the Mosskin's gas hurts the Knight (damages_hero)" 'grep -q "Gas Hit Box .*hits the Knight (damages_hero)" $W/mossgas/log'
run fungas "Fungus2_10 14 14.6" "700:" HK_GOD=1 HK_KILLALL=60
check "a Fungified Husk's corpse bursts into gas" 'grep -q "\[corpse [0-9]*\] Corpse Zombie Fung A explodes" $W/fungas/log'
run spiny "Fungus3_22 13 17.6" "800:" HK_GOD=1 HK_KILLALL=400
check "a Spiny Husk's corpse shoots its spines" 'grep -q "\[corpse [0-9]*\] Corpse Garden Zombie bursts its spines" $W/spiny/log'
run aspid "Crossroads_19 40 34.5" "700:" HK_GOD=1 HK_KILLALL=60
check "an Aspid Mother's corpse lets out her young" 'grep -q "\[corpse [0-9]*\] Corpse Hatcher v2 releases" $W/aspid/log'

# Greenpath's thorn vines are long diagonal polygons: they hurt where the vine is, not across their whole bounding box
# (the Knight falls through the open air inside one's box before he lands in the thorns)
run thorns "Fungus1_09 30 12" "120:" HK_DMGTRACE=1
check "thorns hurt only where the vine is" '! grep -qE "^\[dmg ([0-9]|1[0-9])\] Thorn Collider" $W/thorns/log && grep -q "^\[dmg [0-9]*\] Thorn Collider" $W/thorns/log'

# Iselda's Bench Pin: the benches show on the map
run pins "Fungus1_01b 10 5" "200:" HK_PD="mapGreenpath=1,hasPinBench=1" HK_PINTRACE=1
check "the Bench Pin shows benches on the map" 'grep -q "^\[pin\] .* hasPinBench" $W/pins/log'
run nopins "Fungus1_01b 10 5" "200:" HK_PD="mapGreenpath=1" HK_PINTRACE=1
check "no bench pins without it" '! grep -q "^\[pin\] .* hasPinBench" $W/nopins/log'

# Fungal Wastes' purple bounce shrooms: a down-slash (pogo) on one sends the Knight up fast (25 up; a jump is 16.65)
run shroom "Fungus2_10 53.6 42" "20:;2:DOWN;3:DOWN+Y;40:DOWN;20:" HK_TRACE=1 HK_TRACEN=2
check "pogo off a bounce shroom" 'grep "^t=" $W/shroom/log | grep -qE "v=\([-0-9.]*,2[0-4]\.[0-9]+\)"'
run noshroom "Fungus2_10 53.6 42" "20:;2:DOWN;43:DOWN;20:" HK_TRACE=1 HK_TRACEN=2
check "no shroom bounce without the slash" '! grep "^t=" $W/noshroom/log | grep -qE "v=\([-0-9.]*,2[0-4]\.[0-9]+\)"'

# Options: Button Hints off (from the pause menu) hides the control lines and is saved in settings.txt
# the nail slash: Mark of Pride + Longnail swing the " M" slash at 1.4x the slash object's scale; Fury at 1 mask the red one
run slashfx "Town 120 11" "200:;1:Y;27:;1:Y;40:" HK_SLASHFX=1 HK_PD="gotCharm_13,equippedCharm_13,gotCharm_18,equippedCharm_18"
check "Mark of Pride + Longnail: the M slash, 1.4x bigger" 'grep -q "^\[slashfx\] SlashEffect M at .* scale -\?2\.24[12]," $W/slashfx/log && grep -q "^\[slashfx\] SlashEffectAlt M at .* scale -\?1\.76[0-9]," $W/slashfx/log'
run slashfury "Town 120 11" "200:;1:Y;40:" HK_SLASHFX=1 HK_PD="gotCharm_6,equippedCharm_6,health=1"
check "Fury of the Fallen at 1 mask: the red slash" 'grep -q "^\[slashfx\] SlashEffect\(Alt\)\? F at" $W/slashfury/log'

run hints "Town 120 11" "200:;1:START;30:;1:DOWN;10:;1:A;30:;1:DOWN;5:;1:DOWN;5:;1:DOWN;5:;1:RIGHT;30:;1:B;20:;1:START;100:"
check "Button Hints switches off from the pause menu and is saved" 'grep -qx "hints 0" $W/hints/sdmc/3ds/hk3ds/settings.txt'

# wall cling: the Knight's sideways move stopped up to 0.05 short of a wall (his contacts reach 0.03), so about one
# approach in three never touched it and he didn't cling; these two start offsets were such misses (right and left walls)
run clingR "Fungus2_14 46.84 25.5" "130:;30:RIGHT;1:RIGHT+B;25:RIGHT+B;80:RIGHT" HK_WALLTRACE=1 HK_PD=hasWalljump
check "wall cling on a right-hand wall (any approach)" 'grep -q "^\[wall\] .* sliding=1" $W/clingR/log'
run clingL "Tutorial_01 58.16 23.5" "130:;30:LEFT;1:LEFT+B;25:LEFT+B;80:LEFT" HK_WALLTRACE=1 HK_PD=hasWalljump
check "wall cling on a left-hand wall (any approach)" 'grep -q "^\[wall\] .* sliding=1" $W/clingL/log'

# the City bridge (Fungus2_21): the lever enables "Bridge Collider 1", exported only because the lever refers to it
run bridge "Fungus2_21 67.8 13.5" "130:;20:;1:Y;150:;400:LEFT" HK_HEROTRACE=1
check "the bridge holds the Knight after the lever" 'grep -qE "^\[hero\] x=2[3-9]\.[0-9]+ y=12\." $W/bridge/log'
run nobridge "Fungus2_21 67.8 13.5" "130:;20:;1:;150:;400:LEFT" HK_HEROTRACE=1
check "no bridge before the lever" '! grep -qE "^\[hero\] x=2[3-9]\.[0-9]+ y=12\." $W/nobridge/log'

# thorn vines with more than 4 polygons on one object (Fungus2_21 has 11): the ones past the 4th hurt too
run thorns9 "Fungus2_21 63.45 17.77" "140:" HK_DMGTRACE=1
check "the 9th thorn polygon of Fungus2_21 hurts" 'grep -q "^\[dmg [0-9]*\] Thorn Collider" $W/thorns9/log'

# a fallen stalactite's dust trail stops emitting when it lands (it smoked there for good)
run staltrail "Tutorial_01 58.5 48" "130:;20:RIGHT;700:" HK_PTCTRACE=2
check "the stalactite trail stops after landing" 'grep "Dust Trail" $W/staltrail/log | tail -1 | grep -q "emit 0 alive 0"'

# footsteps: one looping run sound while the Knight runs, stopped when he stops (one-shots every 0.55 s echoed)
run steps "Crossroads_04 120 3" "130:;60:RIGHT;60:" HK_STEPTRACE=1
check "one footstep loop per run" '[ "$(grep -c "^\[steps\] none -> run" $W/steps/log)" = 1 ] && [ "$(grep -c "^\[steps\] run -> none" $W/steps/log)" = 1 ]'

# Fungal Wastes floaters (Funglings, Fungoons) wake: their EnemyWakeRegion was exported as an empty FSM
run fwwake "Fungus2_03 56 17" "200:"
check "Funglings wake when the Knight comes near" 'grep -q "Fungoon Baby[^/]*/Fungoon baby: Sleep -> Wake" $W/fwwake/log'

# the Hunter's Journal: the Inventory's third pane once the Hunter has given it, listing what's been killed
run journal "Town -1 -1" "150:;2:SELECT;30:;2:R;30:;2:R;60:" HK_JOURNALTRACE=1 HK_PD="hasJournal,killedCrawler,killedBuzzer,newDataBuzzer,killedMawlek"
check "the Journal pane lists the killed creatures (the Shade shares the Crawlid's entry, as in the game)" 'grep -q "^\[journal\] 4 of 170 entries listed, first new 1" $W/journal/log'

# King's Pass's Secret Mask (turned 90 degrees, scaled unevenly) covers only the hidden area left of the opening shaft:
# with its scales composed per axis it came out over the shaft, black boxes around the falling Knight
run kpfall "Tutorial_01 -1 -1" "200:" HK_MASKTRACE=1
check "no secret mask over the opening fall" '! grep -q "^\[maskdraw\] Secret_Mask" $W/kpfall/log'

# the Hunter counts the Journal (GameManager.CountJournalEntries): an unfinished one gets his usual talk, not the Mark
run hunter "Fungus1_08 46.8 39.8" "150:;1:UP;20:;1:A;20:;1:A;20:;1:A;100:" HK_PD="metHunter,hasJournal,hunterRoared,killedCrawler"
check "the Hunter doesn't offer his Mark for an unfinished Journal" 'grep -q "Conversation Control: Convo Choice -> Convo 1" $W/hunter/log && ! grep -q "Convo Choice -> Completed" $W/hunter/log'

# a save given the Mark early (before v1.0.23) loads without it, and the Hunter's hut floor stays in place
mkdir -p "$W/hmark/sdmc/3ds/hk3ds"; ln -s "${HK_ROMFS:-$ROMFS}" "$W/hmark/romfs"
printf 'HK3DS2\nFungus1_08\nleft1\n5 0 0\n- 0.00 0.00 0\nF 0\nPT 0\nB hasJournal 1\nB metHunter 1\nB hasHuntersMark 1\nB killedHunterMark 1\nI killsHunterMark 0\nB hunterRewardOffered 1\nB pbi:Fungus1_08:Shiny|Item|HunterMark 1\nFungus1_08\n' > "$W/hmark/sdmc/3ds/hk3ds/save1.dat"
(cd "$W/hmark" && env HK_FAST=1 HK_FSMTRACE=1 HK_SCRIPT="150:;1:A;40:;1:A;30:;100:" HK_OUT=shots timeout 600 "${HK_HOST:-$H/host/hk3ds_host}" > log 2>&1)
check "an early Hunter's Mark is taken back on loading" 'grep -q "Hunter.s Mark given early" $W/hmark/sdmc/3ds/hk3ds/log.txt && ! grep -q "break_floor_grass/FSM: Check -> Destroy" $W/hmark/log'

echo "$fails failed"; rm -rf "$W"
exit $fails
