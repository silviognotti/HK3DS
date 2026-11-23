# Changelog

Newest first. Paths like `romfs/` refer to your own converted copy; none of it is in this repository.

## v1.0: the whole game
Every area of Hollow Knight is in: King's Pass, Dirtmouth, the Forgotten Crossroads, Greenpath, Fog Canyon, the Fungal
Wastes, the City of Tears, the Royal Waterways, Crystal Peak, the Resting Grounds, Deepnest, Kingdom's Edge, the Hive,
Queen's Gardens, the Ancient Basin, the Abyss, the Howling Cliffs, the Grimm Troupe tent, the dream bosses, White
Palace and Godhome: 480 scenes, each converted straight from your copy of the game.

Everything in those rooms runs the game's own scripts (PlayMaker state machines and the game's components, ported):
- **NPCs and their voices**: Elderbug, Cornifer (buy his maps; they show on the bottom screen and fill in at benches
  once you have the Quill), Iselda, Sly, Salubra, Leg Eater, the Nailsmith, the Stag (ring the bell, ride between
  stations), the Grubfather (grub rewards), Seer, Bretta, Quirrel, Zote, Tiso, the Nailmasters... Their dialogue
  boxes, choices and voice lines come from their own conversation scripts.
- **Charms**: all 40, with the game's notch costs, the overcharm rule, fragile charms breaking on death, and their
  effects. **SELECT** opens the inventory (L/R switch between your items and the charms); equip charms at a bench.
- **Spells and abilities**: Vengeful Spirit / Shade Soul, Desolate Dive / Descending Dark, Howling Wraiths / Abyss
  Shriek, focus, Mothwing Cloak and Shade Cloak (shadow dash through enemies and shadow gates, Sharp Shadow), Mantis
  Claw, Crystal Heart, Monarch Wings, Isma's Tear, the Dream Nail (read minds, gain soul, open whispering roots and
  collect essence), nail arts, quick cast. These run on the Knight's own state machines.
- **The world reacts**: breakable walls, pots, lamps and poles, geo rocks that remember how many hits are left,
  lifeblood cocoons and scuttlers, soul totems, dropping and flipping platforms, conveyor belts, bounce shrooms,
  stalactites, bridge levers, lifts and trams (use the pass at the tram door, pull the lever inside), garpedes in
  Deepnest, steep slopes, jelly eggs, enemy hit effects and blood, dream plants. What you break stays broken.
- **The Hunter's Journal** fills in as you kill things ("Hunter's Journal updated" when an entry changes).
- **Bosses** load their arenas the way the game does (the boss scene is loaded on top of the room until you win).
- **Godhome**: the Hall of Gods statues (choose a statue, fight the boss, wake back at the statue) and all five
  pantheons: walk into a pantheon door, accept, and fight through its bosses back to back with the rest benches in
  between, exactly in the game's order. Each pantheon unlocks the next; dying in a dream or a pantheon wakes you where
  you went in, with no Shade.
- **Endings**: the five ending titles (The Hollow Knight, Sealed Siblings, Dream No More, Embrace the Void, Delicate
  Flower) followed by the game's credits. The ending cinematics aren't in the converted data.
- **Darkness**: dark rooms use the Knight's own vignette; the Lumafly Lantern widens it.
- **Music**: each room's music and ambience follow the game's music cues and music regions.
- Glow and light sprites are drawn with additive blending, like in the game.

## New in v1.1: the game data on the SD card, small installs
- **Two parts.** The CIA and the `.3dsx` hold only the code (under 1 MB). The game data is one RomFS image on the SD
  card, `SD:/3ds/hk3ds/hk3ds_std.romfs` (`hk3ds_hd.romfs` for HD), made by `make data` (`mkromfs3ds romfs/`). At
  start the game opens it and mounts it as `romfs:` (`romfsMountFromFile`), so nothing else reads data differently;
  without it, the program's own RomFS is used if it holds the data (a `make FULL=1` build), else the top screen says
  where the data file goes (`main.c`, `data_mount`). The small CIA's RomFS holds only `text/version.txt` and a note.
  Same title IDs, so the small CIA installs over a v1.0.x one.
- **Updates** are a small CIA and `.3dsx` plus the data files changed since the data file's release in
  `SD:/3ds/hk3ds/patch/<std|hd>/` (`tools/make_patch.py`: `hk3ds_update_v<ver>_<ed>.zip`; `--record-base` writes the
  release's file lists, `patches/base_v1.1.0/`). Luma game patching is no longer used; an update checks the CIA's code
  against the ELF. The title-screen warnings now speak of the data file (`patch.c`).
- `scripts/00_build.sh` makes the data files too and says where each file goes on the SD card.

## Fixes in v1.0.25 (patch)
- **False Knight's arena on the Old 3DS: no floor, no boss.** The standard build ran out of memory in Crossroads_10, the
  biggest room in the game: its objects alone took most of the Old 3DS's heap, and the boss scene (the arena floor,
  the husks and False Knight, `Crossroads_10_boss`) then failed to load without a word. The parsed object data is now
  a fifth smaller (32-byte JSON nodes instead of 40) and every repeated key and string of a room's file is stored once,
  which frees about 4 MB in that room; the boss scene loads with 10 MB to spare (checked in Azahar as an Old 3DS). The
  log now says when a room's object file or boss scene can't load (`json.c`, `uo.c`). The graphics memory is still
  tight there: the bottom screen's map may not draw in that one room on an Old 3DS.
- **Per-frame hitboxes lost in 25 rooms.** `tools/tk_colliders.py` writes each animated object's per-frame collider
  boxes (`tkbox`) into its room's `.obj`, but nothing ran it after `pack_objs.py`, which writes a fresh `.obj`: the 24
  re-packed rooms of v1.0.24's particle fix (and Crossroads_01) had lost them, so the Mantis Warriors', husks' and the
  Collector's attack boxes stayed as saved in the scene. Restored; `scripts/00_build.sh` runs it after every pack, and
  `tools/regress.sh` checks that running it again over the rooms changes nothing.
- **Build tools.** `scripts/00_build.sh` builds everything from the export in one command (see the public README);
  `pack_objs.py` creates `romfs/sfx/` itself (a fresh build converted no sounds at all, silently), the Knight's atlas
  comes from the texture dump (`hkcommon.KNIGHT_ATLAS`), `tools/data/` gains `gg_statues.txt` and `music_cues.txt`
  (no tool made them), and `tools/make_icons.py` makes stand-in icons and banners when there are none.
- **Boss names.** A boss fight shows the boss's name in the corner by activating the game's Area Title with
  "Area Event" (FALSE_KNIGHT, HORNET, MANTIS_LORDS, ...), "Visited" and "Display Right" set: Area Title Control's
  area branch. Only its NPC branch was handled, so no boss name ever showed. All 36 boss titles now show, bottom right
  (`dialogue.c`, `title_npc_now`).

## Fixes in v1.0.24 (patch)
- **Invisible enemy shots and effects in 42 rooms.** The Fungoons' gas, and every other particle effect on the
  objects of 42 rooms (most of the Fungal Wastes, Mantis Village, Kingdom's Edge, the Abyss, some of the White Palace
  and Godhome), was missing: those rooms' objects were packed again on Nov 17, and `pack_objs.py` writes a fresh
  `<Room>.obj` without the particle systems and sprite materials `tools/obj_particles.py` adds. The shots still hit;
  they just couldn't be seen. `obj_particles.py` has run on them again (145 particle systems in Fungus2_03 alone,
  plus the lit and additive sprite materials), and `scripts/03_full_rebuild.sh` and `scripts/02_export_all.sh` now run it after every
  `pack_objs.py`. The patch carries the 42 rooms' `.obj` files (and their `_pto.spr` where they changed).

## Fixes in v1.0.23 (patch)
- **The Hunter gave his Mark for an unfinished Journal.** His talk asks `GameManager.CountJournalEntries` how many
  entries and notes are complete, and that method did nothing, so every count read as 0 of 0 and any Journal passed as
  complete. It is now translated from the game's own `PlayerData.CountJournalEntries` (`tools/gen_journal_count.py`
  writes `source/journal_count.c`): the Hunter goes back to his usual lines until all 146 entries are done.
- **Stuck in the pit under his hut.** Taking the Mark early dropped you into the pit below his hut, whose only way out
  is a shaft that needs the Mantis Claw, and with the Mark his hut's floor stays open over it. Loading a save that has
  the Mark without a complete Journal now takes it back (the Mark, its Journal entry and the reward offer), so the
  hut is whole again and the Hunter gives the Mark when the Journal really is complete. If you are in the pit right
  now: Pause, Quit to Menu, Continue; you wake at your last bench.
- **One patch zip per version.** `hk3ds_patch_v1.0.23_std.zip` for the standard CIA and `_hd.zip` for the HD one,
  each with only its own title's folders.
- **The patch checks the installed CIA.** A patch's code reads the installed CIA's data, so it is only made to work
  over the CIA release it was made for (v1.0.21). Each patch's `files.txt` now lists a few room-art files of its base
  CIA with their sizes
  (`BASE` lines); if the installed ones differ, the title screen says to reinstall the v1.0.21 CIA, and `log.txt`
  names the file.

## New in v1.0.22: updates as small patches
- **No more downloading the whole game for a fix.** From v1.0.22 on, updates come as a small patch zip over the
  installed CIAs (v1.0.22 itself is under 1 MB, against 1.4 GB and 2.1 GB for the CIAs). Copy its `luma` and `3ds`
  folders to the root of the SD card and turn on Luma3DS's **Enable game patching** (hold SELECT while powering on).
  The title screen then shows the new version.
- How it works: the game code comes in through Luma3DS's game patching (`SD:/luma/titles/<title id>/code.bin` and
  `exheader.bin`; standard `000400000F4C3D00`, HD `000400000F4C3E00`), and data files changed since the CIA
  release sit in `SD:/3ds/hk3ds/patch/std/` and `patch/hd/` at their romfs paths, listed in `files.txt`. The game
  reads a listed file from there instead of the CIA. Patches are cumulative, so only the newest one is needed.
- A patch's data is only used by the version it was made for, so a leftover patch can't mix with a newer install. If
  a newer full CIA is installed while Luma still loads an old patch's code, the title screen says so; delete that
  title's folder in `SD:/luma/titles`.
- Making one: build both ELFs (`make`, `make HD=1`), then `python tools/make_patch.py patches/base_v1.0.21 OUT`. It
  extracts the code and exheader from a build made with the CIAs' own settings (`tools/ctrfile.py`), checks the code
  against the ELF, and copies every data file that differs from the released CIAs' contents
  (`patches/base_v1.0.21/std.txt` and `hd.txt`, read from the CIAs with `tools/ctrfile.py romfs`).
- Full CIAs now record their data version in `romfs/text/version.txt` (`make cia` writes it).

## New in v1.0.21: an HD version for the New 3DS family, and a Button Hints option
- **Two builds.** `hk3ds.cia` (standard, 400x240) runs on every 3DS and 2DS model. `hk3ds_hd.cia` (HD) is for the
  New 3DS, New 3DS XL and New 2DS XL: it switches the top screen to its 800x240 mode, and the room art, the Knight, the
  enemy and object sprites, the fonts, the HUD, the dialogue box and the title screen were converted again with twice
  the detail across, so outlines, fine art and text look sharper. Everything sits where it does in the standard build.
  On an Old 3DS or 2DS the HD build shows a message and exits (it needs the New 3DS's memory).
- They install side by side as two HOME Menu titles ("Hollow Knight 3DS" and "Hollow Knight 3DS HD"; the HD icon has
  an HD badge), and they use the same save files in `SD:/3ds/hk3ds/`, so you can switch between them.
- The title screen shows "v1.0.21 HD" in the HD build.
- **HD needs the New 3DS's full 124 MB.** The HD CIA asks for it, but a console can still start the game in the old
  64 MB mode: on a New 2DS XL, Luma3DS's "New 3DS CPU" setting (Clock+L2) did that (line 2 of
  `SD:/3ds/hk3ds/log.txt` then says `low memory`). In 64 MB the HD art doesn't fit: background tiles pop in and out
  and some sprites (even the Knight's) are missing, and enemies or a boss whose sprites didn't fit stay invisible or
  never appear (the False Knight's arena and Greenpath's plants were reported). With full memory those rooms were
  checked again: the False Knight drops into his arena and the husks and maggots in his room show and take hits (the
  real 3DS build, in an emulator), and Greenpath's Fool Eaters show, bite and die after four hits. The HD title screen now says so on the bottom screen when that
  happens. Set "New 3DS CPU" to Off in the Luma config (hold SELECT while the console starts): both builds already run
  at 804 MHz with the L2 cache by themselves. A Luma plugin can also cause it; the standard build runs fine in 64 MB.
- **HD background art comes in faster.** A screen of HD art takes twice the tiles of the standard art, and unpacking
  them only on the main processor, a few each frame, left them coming in late on a real console. Now:
  - a worker thread on the New 3DS's spare CPU core unpacks the tiles the main processor has no time for;
  - a room's whole first screen is unpacked while the screen is still black, so it fades in complete.

  The log also gets a line per room (`tiles <room>: ... with tiles missing`) to show how it does on your console.
- **Options → Button Hints (On / Off)**, in the title screen's Options and the pause menu's, in both builds. Off hides
  the button lines: the controls list under the map while playing (the map gets taller), the menus' "D-Pad: select"
  line, the inventory's and the Options pages' hint lines. It's saved with the other settings.
- **The nail slash is drawn at the game's size and place.** Each slash (side, alternate, up, down, wall) is now drawn
  where the game puts it, scaled as the game scales it, so the arc covers the area the nail actually hits (before, it
  was drawn about 60% of that size, centred on the Knight).
  - **Longnail and Mark of Pride** make the arc bigger along with the hit area (1.15x, 1.25x, 1.4x with both); before,
    only the hit area grew.
  - **Mark of Pride** swings its own wider slash, and **Fury of the Fallen** (at 1 mask) the red one, as in the game.
  - The wall slash is drawn on the side away from the wall, where it hits (it was drawn into the wall).
  - The hit area grows from the slash's own centre, as in the game, instead of from the Knight's; the down slash with
    both charms reaches a little less far below.

## Fixes in v1.0.21 (playtest of the HD build)
- **Wall cling** sometimes didn't take hold: the Knight's sideways movement stopped up to 0.05 units short of a wall,
  past the reach of his wall contact, so about one approach in three never touched the wall. He now comes to rest
  against it (with the same small gap a Unity body keeps), and clings every time.
- **Footsteps echoed and lagged**: the run sound is one ~2 s clip of six steps that the game loops while the Knight
  runs. The port played the whole clip again every 0.55 s, so three or four copies overlapped, out of step with his feet.
  It now loops one copy while he runs (or walks, in walk zones) and stops when he stops, jumps or the game pauses.
- **Fungal Wastes enemies**: Funglings and Fungoons (and Kingdom's Edge's Hoppers, Blow Flies) never woke up, so they
  hung in place and never attacked. Their wake-up trigger (EnemyWakeRegion) had been exported as an empty state
  machine; it now wakes them when the Knight comes near, as in the game. (The other Fungal Wastes enemies were checked
  too: Shrumelings, Shrumal Warriors and Ogres, Ambloom, Sporgs and Fungified Husks wake, move and attack.)
- **Mantis Warriors were invisible** (and a few other sprites: the Collector, Kingsmoulds, the Abyss tendrils, Waterways'
  crank, some debris, a Hunter's Journal icon): their texture match had been rejected by the converter. Fixed in both
  builds.
- **Thorn vines with more than four pieces** (Queen's Station, the City of Tears entrance in Fungus2_21): only the
  first four pieces of one object had a hitbox. Objects now keep every collider (a few City spikes and two story
  triggers had the same limit).
- **The City of Tears bridge (Fungus2_21)**: the levers raised the bridge but it had nothing to stand on. The
  bridge's collider, switched off until a lever is hit, wasn't exported; it is now.
- **Falling stalactites** left a cloud of dust that never went away where they landed: the trail stops now, as in the
  game, and fades.
- **The Hunter's Journal** had no page: it is now the Inventory's third pane (L / R) once the Hunter has given it.
  The bottom screen lists every creature you've met with its icon; the top screen shows its portrait, name and
  description, and either how many more to defeat or the Hunter's notes. Entries you haven't looked at yet carry the
  new dot, and the counts of encountered and completed entries are at the top.
- **Black boxes during the opening fall** (and wrongly shaped secret-area covers in 88 other rooms): a cover under a
  parent that is turned and scaled unevenly was converted with its scales on the wrong axes. King's Pass's cover came
  out 73 units wide instead of 31 and lay across the shaft the Knight falls down. The converter now composes these
  transforms exactly.

## Fixes in v1.0.20
- **Bounce shrooms (Fungal Wastes)**: a down-slash (pogo) on the purple bounce shrooms did nothing. It now sends the
  Knight up high, as in the game (higher than a pogo off an enemy), with the shroom's bounce animation, sound and
  particles; side slashes on one knock the Knight back. The game handles shrooms in the nail slash itself rather than
  as an enemy hit, which the port had missed.

## Fixes in v1.0.19 (eleventh playtest)
- **Greenpath's thorn vines** hurt across their whole bounding box instead of only where the vine is. They're long
  diagonal shapes, so their boxes covered the gaps you jump through, and some passages couldn't be crossed. Polygon
  and edge colliders now test their own outline (Queen's Gardens' thorns and every other slanted hazard too).
- **Exploding enemies** go off after they die, as in the game:
  - Volatile Mosskin (Greenpath) and Fungified Husks (Fungal Wastes): the corpse lands, steams and shakes, then bursts
    into a cloud of orange gas that hurts. Volatile Mosskin's corpse didn't even show up before (it's saved switched
    off; the game switches it on when the enemy dies).
  - Volatile Gruzzers and Furious Vengeflies (infected Crossroads): the corpse shakes and leaves a gas explosion.
  - Spiny Husks (Queen's Gardens, Deepnest): the corpse shoots out its spines.
  - The Aspid Mother lets out two hatchlings when she dies.
- **Hazards that use the game's older damage script** (a `damages_hero` state machine instead of the DamageHero
  component) didn't hurt at all: Volatile Mosskin's own gas attack, the corpse gas above, the infected gas explosions,
  the Shade's shadow balls, Crystal Peak's stompers and crystal shots, Grimm's flame pillars, Lost Kin's slashes. They do
  now.
- **Effects on objects were drawn in the wrong place**: particles on any object below another in the scene (the
  Mosskin's gas, corpse steam, lamp flames, enemy effects) took their position without the parent's, so many were drawn
  far away or off screen. Every room's object particles were re-exported.
- **Particle budget**: a burst from an object (an explosion's gas, a hit's dust) takes the place of area dust and motes
  when the budget is full, instead of coming out with a handful of particles.
- **Map pins**: the pins bought from Iselda show on the map (Bench, Stag Station, Tram, Vendor, Hot Spring, Warrior's
  Grave), with the game's own rules (a vendor's pin once you've met them, a grave's until you've beaten the warrior,
  toll benches greyed until paid). The Black Egg Temple shows on the Crossroads map.
- **Shop purchase prompt** (Sly, Salubra, Iselda and the rest): "Purchase this item? Yes / No" was a box laid across
  the item list and the description, which showed through around it. It now takes the description's place under the
  item's name.
- **Hornet stuck in the ground, again**: when her air dash set her down in a corner of the arena, against the closed
  battle gate, the v1.0.18 fix had nowhere to push her straight up. She's now pushed up and to the side, out of both.
- **Version number** at the bottom right of the title screen.
- **Room loading order**: a room's object sprite pages are now loaded before its art, into the space the last room's
  pages left. Loaded after the art, a page could fail to find room after a few rooms and its sprites went missing until
  a restart (a likely cause of Dirtmouth's Elderbug being invisible on a new game).
- The previous session's log is kept as `sdmc:/3ds/hk3ds/log_prev.txt`, so a problem seen before a restart can still be
  looked into.

## Fixes in v1.0.18
- **Hornet stuck in the ground** (Greenpath and Kingdom's Edge): after her diving air dash the game sets her down at
  a fixed height that is a unit inside the arena floor, and the game's physics pushes her back up out of it. Here she
  stayed wedged in the floor for the rest of the fight. Bodies sunk into the floor are now pushed back up the same way
  (0.2 a frame, like Box2D).

## Fixes in v1.0.17
- **Dirtmouth's ground** (and 29 other rooms': parts of Deepnest's east, Crystal Peak, Greenpath, Godhome's arenas,
  Waterways, Howling Cliffs): the black terrain under the walkways was missing, so the fog behind showed through as
  pale bands under the floor. Their terrain layer is named after the scene's template ("Town-TileMap Render Data"),
  which the converter didn't recognise.

## New in v1.0.16: Spanish
- **Language option** (Options, on the title screen and in the pause menu): English or Español. It's saved on the SD
  card (`sdmc:/3ds/hk3ds/settings.txt`, with the music and sound volumes, which are now kept too).
- **All of the game's text in Spanish**: the game's own official Spanish translation (dialogue, item and charm
  descriptions, the journal, lore tablets, area names, shops, menus, achievements, credits), with its accents, ñ, ü,
  ¿ and ¡. The fonts carry those letters now.
- **The port's own lines** (control hints, profile and option prompts, notices) are translated too.
- Long Spanish names shrink to fit (charm names, area titles, achievements); shop item names go onto two lines and
  long descriptions get smaller text so they stay above the Geo counter.
- Memory: only the chosen language's text is loaded, a few sheets at a time as before; switching language frees the
  other one. The extra letters add about 64 KB to the fonts, and the Spanish text about 0.5 MB to the install.
- Fixed on the way: shop descriptions could start with a few garbage characters (an uninitialised text buffer), and
  the save profile's area name for Crystal Peak, Joni's Repose and the like broke the save line in two.

## Fixes in v1.0.15 (tenth playtest: compared against Steam screenshots)
- **The ground was see-through.** The game draws every solid tile of a room's terrain as plain black (its tile map
  renders one black tile). The port never drew that layer: wherever no scenery covered the ground, walls or ceilings,
  the background showed through them, and the edge art in front looked like loose pieces floating over it. Every room
  now has its terrain layer, at the depth the game sorts it.
- **Foreground glows came out as black blobs.** Glows, haze and lamp light in the game's top sorting layers are drawn
  after everything else, over the foreground silhouettes (which they tint). They were baked in by depth, so the
  silhouettes in front covered them. They're now in layers of their own drawn last.
- **Room colours.** Each area's colour now comes from the game itself:
  - the scene's ambient light on scenery that uses the game's lit sprite material (Greenpath's teal, Crystal Peak's
    violet), on baked art and on room objects;
  - the scene's colour-correction curves and saturation, on the baked art;
  - additive materials found from the material itself, not guessed from sprite names (Greenpath's far fog had turned
    into a grey veil over the room).
- **Fog, haze and far backgrounds** are stored at 32-bit colour at half resolution: at 16 bits they were banded and
  grainy, with hard staircase edges where they faded out. Backgrounds behind the scene's blur plane are blurred
  slightly, like in the game. The foreground silhouettes are at full resolution again.
- **The Knight's light** is the game's own HeroLight (its sprite, size and each scene's colour), drawn behind the
  terrain like in the game. It had rings around it and lit the black ground under him grey with square edges.
- **Glows cut off square:** haze at the gameplay plane is drawn over the terrain, as in the game; the terrain covered it
  and cut glows off along the tile edges.
- **A bright ring around the Knight** (`white_light_donut`, always on in his prefab) was drawn as an additive glow over
  everything, black ground and foreground included. Its material isn't in the game data and the game doesn't show such
  a ring, so it isn't drawn; his light is the HeroLight above.
- **Dark rooms:** the darkness plate that dims the scene was drawn in front of the Knight, so in Deepnest and the Abyss
  he was a dim grey ghost, and the vignette went behind the foreground, which stayed bright at the screen edges. Both
  now follow the game's order.
- **Black smudges at room exits:** each exit has a sprite that only cuts the darkness open on ultrawide screens. It was
  drawn as a black shape; it isn't drawn now.
- **Particles** are drawn: the rooms' and areas' dust, motes and spores, and the particle systems on objects (enemy
  and breakable effects, lamps), from the game's own particle systems. At most 420 at once on a New 3DS, 160 on an Old
  3DS; the ones nearest the Knight get the budget.
- **Room objects** take the scene's ambient light when they use the game's lit sprite material, and are drawn
  additively when their material is additive (it was guessed from object names before).
- **Old 3DS: missing tiles.** Room art is cut into 128x128 tiles, and three quarters of the stored tiles turned out to
  be fully transparent (faint haze edges below what 16-bit colour can show): they're gone, and single-colour tiles are
  drawn as plain fills. The tile cache now holds a whole screen even in the Distant Village, where tiles kept going
  missing while you walked.

## Fixes in v1.0.14 (ninth playtest: side by side with the Steam version)
- **Holes in the walls, missing black and shading.** Nothing was cut to save memory. The scene converter skipped two
  kinds of scenery by name:
  - `mask_container`: the black covers over wall interiors and hidden passages.
  - `black_fader` / `darkeners`: the soft black gradients that shade edges and give rooms depth.

  Both are back now, in 128 rooms for the gradients. The covers behave like in the game:
  - Some fade when you walk in.
  - Some cover again when you leave.
  - Some only show while you're inside an area. They use the game's real trigger shapes, including the L-shaped ones.
  - The ones behind King's Pass's breakable walls clear when the wall breaks.
  - Plain black shapes nothing fades (doorways out of a room, the void past a wall) always stay. Before, they cleared
    as soon as the Knight walked near them, which opened views the game keeps black.
  - Each is drawn at its own depth among the layers, not on top of everything.

  Only gradients a script fades (a boss arena dimming, an inspect prompt) are still left out. The Ancestral Mound
  tunnel art (`_Scenery/Vignette`) was being skipped by name too and is back.
- **The Knight's vignette was never drawn.** The game sends the vignette a reset when a room starts; the port sent a
  different event, which skipped the step that switches its sprite on. Rooms now have the game's soft dark edges, and
  dark rooms (Deepnest, the Abyss...) are dark again around the Knight, as they should be without the Lumafly Lantern.
  It's drawn from its own 8-bit alpha texture so it stays smooth on an Old 3DS too (at 16 bits it showed rings).
- **Old 3DS: far backgrounds were missing in layered rooms.** The tile cache (40 tiles) couldn't hold one screen of a
  room like King's Pass (18 layers), so the farthest layers were left out. It now keeps 60 tiles and gives some back
  when a room's sprites need the memory, so the Old 3DS shows the same picture as the New 3DS.
- **Breakable walls, poles, barrels, mushrooms and webs made no sound.** The game picks these sounds at random from a
  sound table instead of giving each object one clip, and the port only played single clips. The tables are matched
  from the game's data (rock and wall breaks, poles, barrels, carts, mushrooms, webs, corpse splats, water drips).
- **Enemy corpses:**
  - Flying enemies (Vengefly, Squit, Gruzzer, Aspid Mother, Belfly...) now burst with a splat when they hit the ground
    instead of lying there.
  - Bigger ones (Shrumal Ogres, Husk Hives, Mawlurks...) break into chunks where they land.
  - Corpses play their own death sound (husks, crystal crawlers, Weavers, Moss Knights...).
- **Mushrooms** wobble and rustle when you walk through them (TouchShake).
- **Spore Shroom's and Defender's Crest's clouds did no damage.** The components that tick damage on enemies inside a
  cloud (DamageEffectTicker, ExtraDamageable) weren't in the port; they are now, with the game's damage intervals.

## Fixes in v1.0.13 (eighth playtest)
- **Enemies you'd already killed came back and couldn't be hurt.** A killed enemy is saved and stays gone until you
  rest at a bench. But many rooms, most of the Forgotten Crossroads among them, switch a whole group of enemies on as
  you enter, and that brought the saved-dead ones back: walking around, already dead, so the nail bounced off them
  without doing damage. That was 74 enemies in 26 rooms, the Husk Guard right of the False Knight and the Gruz Mother's
  room included. They now stay gone.
- **Every charm after the first showed "CHARM_NAME_0" when picked up.** The game sets up an object a script creates
  only after that script's step is done; here the pickup message set itself up before the item gave it the charm's
  number. Script-created objects now start the way the game starts them.
- `log.txt` now says why a nail swing that touched an enemy did no damage (`nail touched ...: <reason>`), if that
  ever happens again.

## Fixes in v1.0.12 (seventh playtest)
- **Stuck in the air after getting a spell, only able to move sideways, and no pause menu.** The cause wasn't found
  from the report alone, so every room was played by a test bot using every power at random (casts, double jumps,
  dashes, super dash, dive, shriek, focus), with enemies hitting the Knight and with uneven frame times like a busy
  3DS. That turned up these causes, all fixed:
  - **Casting at a room exit** (most likely what you hit). If the Knight went through an exit mid-cast, for example
    pushed by the fireball's recoil, the game cancels the cast as he leaves. Here the cast carried on into the next
    room, and its recoil state then held him for good: frozen in place or hanging in the air. Casts are now cancelled
    at exits, as in the game.
  - **Jumping out of water** left the Knight with no gravity. The water script relies on the game's HeroController
    turning gravity back on when swimming ends, and on its "start with a jump" call; neither was ported.
  - **Focus gave control back one frame in.** An old stand-in for focus, from before the Knight's own scripts ran,
    ended every focus at once. The Knight could walk, jump or cast while his focus script was still running.
  - **A cast could be left running after something else gave the Knight control back.** He then hung in the air with
    the spell's script holding his vertical speed at zero. Such a cast is now cancelled after a second, the same way
    leaving a room cancels it.
  - **Pausing could stay blocked until a restart.** Reading a tablet or starting a conversation in the same frame as
    walking through an exit was cut off by the room change, with pausing still switched off. As in the game, the
    Knight now takes no input once an exit fires, and entering a room always switches pausing back on. Getting hit
    blocks pausing only for the hit's freeze, as in the game, and loading a save always allows it.
  - **A cutscene cut off by a room change could leave gravity at zero.** Entering the next room now restores it.
  - **Safety net:** if the Knight is ever in the air with control and no gravity for half a second, and none of the
    moves that switch gravity off is running, gravity comes back. `SD:/3ds/hk3ds/log.txt` gets a `watchdog:` line
    saying what state he was in. If it still happens, that line says where to look.
- **The Ancestral Mound's gate could be walked through.** After you get Vengeful Spirit, the Bone Gate is meant to keep
  you in until the Elder Baldur is beaten. It sits on the game's Soft Terrain layer, which only the Knight collides with,
  and only the normal terrain layer was solid here. Soft Terrain now blocks the Knight. That also fixes the bone battle
  gates in Greenpath's Moss Knight arena, the Deepnest battle gates and the Failed Champion's terrain block.
- **The False Knight's mace and the Husk Guard's club didn't hurt** (their shockwaves did). In the game, a sprite's
  animation frames switch its hitbox on and off and reshape it frame by frame. The False Knight's swing turns the mace's
  hitbox off and relies on the swing animation to turn it back on, and the club's hitbox moves from above the Husk
  Guard down to the ground in front of him. Both now work as in the game, for every enemy built this way.
- **New 2DS XL: missing textures and sprites, and enemies with no death animation.** The game picked its full-colour
  graphics on any console with more than 40 MB of graphics memory. A New 3DS starting the `.3dsx` from the Homebrew
  Launcher can get 80 or 96 MB in total instead of the CIA's 124 MB. That passed the check but didn't have room for
  everything, so some sprite sheets and backgrounds failed to load. The game now picks by the memory it actually
  got: under 56 MB of graphics memory it uses the Old 3DS's leaner set. A sprite sheet that still doesn't fit at full
  colour is now loaded at 16 bits instead of being dropped. Tested in the emulator with a build limited to 80 MB.
  The log's first lines (`boot: heap ... linear ...` and `New 3DS, full/low memory`) show which mode a console runs in.
- **Salubra's shop and the Black Egg Temple couldn't be entered.** Their doors get their destination from their
  own door script, and that never made it into the room data. The tools now read it from the script
  (`tools/data/door_targets.txt`, applied by `fill_gate_targets.py`).
- **Old 3DS: floors and walls flickered in and out.** Layer tiles were unpacked back to front, so the far
  background took the tile cache and the frame's unpacking time first. Tiles on screen are now requested gameplay
  plane first (walls and floor), then the layers nearest to it; when memory or time runs short, a far background
  tile is what waits a frame. Enemy and effect sprites that can't get GPU memory now take it back from the tile
  cache instead of staying invisible.
- **Watcher Knights never appeared.** Their fight only loads when the Knight drops in through the top door, and the
  game compared that with the door used to enter the *previous* room. A Watcher Knight that was standing in the
  arena's gate as it closed got stuck in it; as in Box2D, terrain that moves into a body now pushes it out.
- **The Hollow Knight fell out of his arena** the moment his fight started. His own collider is off; a child collider
  holds him on the floor, and in Unity a child's colliders belong to the parent's body. Physics bodies now use them
  (flying enemies such as Fungus Flyers, Jellyfish and Mages also stop at walls now, and Vengeful Spirit bursts on
  walls instead of flying through them).
- **Crystal Peak's item stand (the Shopkeeper's Key) couldn't be picked up**: its pickup region starts just above the
  Knight's head. Like Box2D's shape skin, the Knight's trigger box now reaches 0.02 further.
- **The Abyss core room ran at 20 fps** on a New 3DS: each enemy's line-of-sight check tested every terrain object
  in the room at every step. Now it only tests the ones near its line (55 fps).
- Tested this time: the test bot played every room using every power at random, with enemies hitting the Knight (half
  the rooms also with uneven frame times), checking that the Knight never floats, no cast gets stuck and pausing never
  stays blocked by accident; the
  full regression suite (`scripts/05_regress.sh`), with new checks for everything above plus the earlier fixes (every shiny
  item pickup, every Godhome boss arena, the dream bosses, Mantis Lords, Watcher Knights, Traitor Lord, Hornet 2, the
  Hollow Knight with the first ending and the Radiance with Dream No More). In the 3DS emulator: the heaviest rooms on
  a build limited to 80 MB, with the old and the new memory rules, plus 249 of the 480 rooms on the CIA's 124 MB in
  an earlier pass. The emulator runs at a few frames a second here, so not every room was re-run on it.

## Fixes in v1.0.11 (sixth playtest)
- **Breakable walls couldn't be broken**, so secret areas stayed covered by their black mask (the grub behind the
  wall in the Crossroads among them): the nail only told objects on a few layers that they'd been hit, and the
  walls sit on the terrain layer. Anything whose script listens for a nail hit now gets it (walls, the Ancestral
  Mound's bone gate, grass...).
- **False Knight**: the barrels he brings down broke into big white shapes that stayed on the floor; they're the
  orange infection splat and fade out in a third of a second now (as do the stun burst and the spell's impact
  flash everywhere). After the last hit his armour, head and staff no longer drop out of the world when the death
  steam switches his collider off: the armour lies there while the head coughs, as on PC. A resting physics body
  now sleeps like in Unity's Box2D, which is what keeps it there.
- **Corpses slid along the floor forever** (Tiktiks, Husk Guards...): they get the ground friction Unity gives
  them and stop.
- **False Knight's arena ran at about 20 fps** (seen in the emulator, set up as a New 3DS with the CIA's 124 MB):
  it keeps 150 rubble pieces under the floor, each listening for the Knight, and the trigger check tested every
  pair of objects. Objects now only test those on layers they can collide with: about three times faster there and
  twice as fast in ordinary rooms.
- New checks in `scripts/05_regress.sh`: the Crossroads wall breaks and uncovers its area, a Husk Guard's corpse comes
  to rest, and a full False Knight fight ends with his armour still on the floor.

## Fixes in v1.0.10
- **Floating after Vengeful Spirit**: casting in the air left the Knight hovering until a nail hit reset him. The
  spell script turns his gravity off and back on through his Rigidbody2D (`SetGravity2dScale`), but only the
  script's stand-in object got the change, not the hero physics. The cast's recoil also ended after one frame
  (watching the Knight's animation finished at once); it now plays out, pushing him back a little as on PC.

## Fixes in v1.0.9 (bug sweep)
Found by sweeping every room, arena and boss with the test bot instead of waiting for playtests.
- **Freed grubs came back** when you left the room: a grub's bottle renames itself "Collected" when it breaks, and
  its saved state was keyed on that new name instead of the one it started with. Grubs freed with v1.0.8 or older
  are carried over in rooms with one grub (in a room with two, free the returned one again).
- **Soul Twisters froze** in the City of Tears (and the teleporting enemies in Deepnest): they teleport into one of the
  room's "Teleplane" areas, which scripts find by their tag, and those plain trigger areas weren't converted. Each of
  their 50 tries then failed and hit the script engine's loop limit, which was 100 transitions a frame instead of
  PlayMaker's 1000, so they stopped dead. The converter now keeps every object scripts look up by tag, and the limit
  matches PlayMaker.
- **The Collector's jars** now drop from the ceiling and break into the Vengeflies, Aspids and Baldurs inside them
  (`SpawnJarControl`).
- **City of Tears lifts** move again, stopping at their floors (`TranslateV2`).
- **The King's Pass great door** shows its cracks as you hit it (a sprite switched by name).
- **Grubfather stopped after his first reward**: before each further gift he checks whether too much loose geo is on
  screen (`CheckGeoCap`), and that step did nothing, so he waited forever. He now hands out every reward you've earned.
- Other script steps that did nothing before: random waits (a Soul Sanctum arena could stall on one), battle-scene
  links for summoned enemies (Greenpath arenas), special deaths and damage overrides (Traitor Lord, the
  Hollow Knight), the Dive pickup's grandparent lookup, line-of-sight casts, sprite colour fades, value compares,
  "any key" prompts and more.
- **The last White Palace arena** never opened: its check for remaining enemies (`GameObjectHasChildren`) did
  nothing. Xero's spinning swords (`iTweenRotateBy`) could also hang his fight.
- Scripts that look for an instantiated object by its Unity name ("Spawn Roller v2(Clone)") now find it, so Elder
  Baldur stops spawning rollers once one is out.
- The test bot fought every arena and boss it can reach: the arenas' waves end and open their gates, and these bosses
  die and record their defeat: Hornet, False Knight, Gruz Mother, Brooding Mawlek, Soul Master, Dung Defender,
  Crystal Guardian, Nosk, Uumuu, Flukemarm, Hive Knight, Broken Vessel and the Collector. Not checked this way: the
  Mantis Lords, Watcher Knights, Traitor Lord, Hornet at Kingdom's Edge, the dream bosses, Grimm and the endings.
- `scripts/05_regress.sh` gained checks for freed grubs staying freed, the Grubfather's rewards, the door, the lift, the
  Mawlek waking, the Collector's jars and the Soul Twister.

## Fixes in v1.0.8 (fifth playtest)
- **Gruz Mother soft lock**: when she dies her corpse bursts and releases her babies, which it finds by the name of
  the empty holder object "Fly Spawn". That holder wasn't converted (it has no components of its own), so no babies
  came, the battle never ended and the gates stayed shut. The converter now keeps every object a script looks up by
  name (`tools/build_objs.py`, `found_names`); the other rooms with such lookups were redone too.
- **White shapes and 15 fps after Gruz Mother**: her death burst's splat is driven by a Unity (Mecanim) Animator that
  flips its frames, tints it orange and fades it out in 0.35 s. Animators weren't supported, so it stayed on screen as
  two huge white shapes (heavy to draw). Animator clips are now converted (`tools/uanim.py`) and played: colour, alpha,
  scale, position, active flag and sprite flip-books. Spawned effects play theirs when they appear; in rooms, looping
  ones play (the pulsing infection blobs of the Crossroads).
- **Myla** animates from the start (every auto-played animation sat on its first frame until a script played
  something), **hums** (looping sounds with distance falloff, up to four at once: her humming, machinery, lifts, water)
  and swings her pickaxe with its sound (scripts that read the current sprite's id now get the real one).
- **Goams** (the spiky worms in the Crossroads walls) hurt again: their hitbox follows the sprite's per-frame collider,
  so it grows as they burst out instead of staying a thin strip at the base.
- **Grubfather's room**: the saved grubs sit in their holes (objects now use their world depth, not the depth inside
  their folder), and the Grubfather is drawn in front of the wall behind him.
- `scripts/05_regress.sh` gained checks for all of these, including a full Gruz Mother fight.

## Fixes in v1.0.7 (fourth playtest)
- **Menus that were dark boxes on a real 3DS**: the shop list, the pause menu, the inventory and charm screens, item
  messages and achievement popups draw a dimmer and then their text and icons. citro2d draws with a depth test, so
  everything drawn after a dimmer that sat "deeper" was hidden behind it; Sly's shop showed only its highlight bar.
  The 3DS now draws strictly in order, exactly like the PC build every test runs on.
- **Save profiles**: Start Game opens the game's profile screen with four save slots, each over the game's art for the
  area you saved in (`tools/build_areas.py` -> `romfs/gfx/areas.spr`), with your masks, geo and play time. X clears a
  slot. An existing save becomes profile 1.
- The ending titles, item messages and charm/map menus had the same hidden-behind-the-dimmer problem; all fixed by the
  same change.
- **Pause menu** like the game's: Continue, Options (music and sound volume) and Quit to Menu (which saves), in the
  menu font with the animated fleur pointers, fading in.
- **Elder Baldur**: it now spits its goop at you while it's open (the projectile was picked by a script step that
  stored nothing). This fixes every enemy that chooses its projectile that way. Hitting its closed shell bounces you off.
- **Gruzzers flying out of rooms**: the room's collision ends at its edge, and past a doorway the floor and ceiling ran
  on forever, so a Gruzzer that flew into a doorway zig-zagged off into nowhere (sooner at the 3DS's lower frame
  rates). Enemies and other objects now meet a wall just outside the room's sides; the Knight's doorways stay open.
- `scripts/05_regress.sh` gained checks for the Elder Baldur's goop, Gruzzers staying in their room at 20 fps and Sly's shop.

## Fixes in v1.0.6
- **The light around the Knight** was drawn as a stepped grey halo with hard rings (most visible in dark rooms such as
  the City of Tears and the False Knight's arena). It's a faint additive glow, and stored at 4 bits per channel it only
  had six brightness steps. The Knight's effects page (his light and the spell effects) is now kept at full colour, so
  the light fades smoothly. On an Old 3DS it is still reduced to 4 bits to save memory.

## Fixes in v1.0.5 (third playtest)
- **Camera in the Crossroads**: camera zones that leave some of their limits open (written as -1 in the game's data)
  now get the room's edges for those limits, as the game does. Before, leaving the first Crossroads room's entry
  zone sent the camera to a corner.
- **Divine's tent**: the Grimm Troupe's tent doors in Dirtmouth only work once the troupe has arrived, so there is no
  invisible door where the tent isn't.
- **Hot spring**: sitting in the Crossroads hot spring refills SOUL again. Script calls with whole-number arguments
  were read as 0; this affected many scripts across the game, not only the spring.
- **Baldurs, and enemy knockback in general**: a nail hit pushes enemies back again. The push was blocked by the floor
  the enemy stood on, so ground enemies hardly moved. A rolling Baldur now bounces back when you hit it, and still
  hurts you if it rolls into you.
- **Missing art (shadows, NPCs such as the Snail Shaman) on some systems**: the texture memory for the room art is now
  set aside once at boot instead of bit by bit while you play, and if a room's sprites still don't fit, cached sound
  effects are freed to make room. The log (`sdmc:/3ds/hk3ds/log.txt`) says if anything still fails to load.
- **Stray bits of art floating around rooms**: the game's texture atlases pack small sprites into the empty corners of
  big ones, and Unity only draws each sprite's own outline. The converter drew the whole rectangle, so pieces of
  neighbouring sprites showed up as scraps (in the room art and on objects). Every room's art and object sprites are
  now re-rendered with each sprite's own outline.
- **Hot spring water**: the water's caustic sprites are meant to glow (additive), but they were drawn as solid black
  squares that hid the pool and the Knight in it.
- **Room exits**: an exit at the edge of a room now also covers the space beyond it, so you can't get stuck outside a
  room (for example behind the closed shortcut wall between two Crossroads rooms). Falling out of the bottom of a room
  takes the nearest exit down.
- **Vengeful Spirit**: after taking the spell, the Knight fell out of the world (invisible) and the save it made put
  him there again. A script asked the Knight for the floor under him (FindGroundPoint) and got nothing back; it now
  works, so you wake up outside the Snail Shaman's hut. A save whose respawn point is outside its room is repaired
  when loaded (you start at the room's entrance), so saves from v1.0.4 work again.
- **False Knight invisible**: a big room plus its boss could run the GPU memory out, and the boss's sprites were the
  ones that didn't fit. The old room's objects are now freed before the new room loads, and the baked copies of
  objects that draw themselves are no longer loaded (they were also drawn on top of breakable poles and floors,
  which is why broken things seemed to stay).
- **Gate locked again after the False Knight**: the arena reopens its gates with a "battle gates open" event when you
  come back; that event was never sent. Quitting from the pause menu now also saves (Save & Quit), like the game.
- **Gruz Mother sliding on forever**: her corpse now counts as landed whenever it rests on the floor, so it bursts.
- **Benches**: benches run by their own script (Ancestral Mound, White Palace...) now save. Every bench in the game
  was sat on in a test: all 50 save.
- **Achievements**: the title screen's Achievements page lists the game's achievements, unlocked by the bosses,
  collections and endings (kept across saves). The unused Extras entry is gone.
- **Light beams and waterfalls** (Crossroads, Ancestral Mound, City of Tears, Waterways, Colosseum...) were drawn as
  dark rectangles; they glow now.
- **Black boxes over parts of rooms** (32 rooms, e.g. Deepnest, City of Tears, Waterways, Crossroads_18): secret
  areas have "remaskers" that cover the room outside a secret while you're inside it, and covers that come back when
  you leave. Both were drawn as permanent black covers; they now fade in and out with the Knight, as in the game.
- **Old 3DS**: the game now adapts to an Old 3DS's 64 MB (leaner caches, 16-bit full-colour textures, more memory
  for graphics). It runs, but expect a much lower frame rate than on a New 3DS in busy rooms.
- `scripts/05_regress.sh` gained checks for the Crossroads camera, Divine's tent, the Baldur's damage and knockback, the
  Snail Shaman, the hot spring, Vengeful Spirit, benches and the False Knight's gates.

## Fixes in v1.0.4 (second playtest)
- **The Dirtmouth well dropped you out of the world again**: rebuilding the room art had also rewritten the room files
  and lost 170 exit links. They're back, and installing a room now always relinks every exit.
- **The camera flew to one side** near the King's Pass chest (and in other rooms with slanted or L-shaped camera zones):
  zones were treated as their bounding rectangles, which overlapped their neighbours. The camera now uses their outline.
- **The first charm's message overlapped** ("Collected a Charm" over the charm's name): text is now placed inside its
  text box the way the game does, top-aligned text hanging from the top of the box.
- **The burst when you pick up an item, and the shiny item's ring, stayed on screen**: they fade and disappear now.
- **New Game played the landing animation and sound mid-air**: the opening script checked the Knight's height before he
  was placed in the room. It now waits for him, so he lands when he reaches the floor.
- Reading a lore tablet now fully finishes its script, so it can be read again.
- `scripts/05_regress.sh`: must-pass tests on the PC build (exits, the well, the King's Pass door, the chest and charm, the
  focus tablet, the camera, the opening fall, enemy blood clean-up, geo).

## Fixes in v1.0.1 and v1.0.2 (from playtesting on a real New 2DS XL)
- **v1.0.1: crash on New Game / Continue on a real 3DS.** The game only got 32 MB of the memory the GPU and sound can use (the
  default split), and sound effects were never unloaded, so loading a room's sounds ran out and crashed. The game now
  splits the New 3DS's memory evenly, keeps sound effects in a 14 MB cache that drops the least recently played ones,
  stores the Knight's sprites at half size, and skips a sound instead of crashing if memory is ever short. Startup and
  room loads are logged to `SD:/3ds/hk3ds/log.txt`.
- **Blood that never went away** (the orange infection blood) now flies, splats and disappears like in the game, and
  **geo** is picked up when you touch it instead of staying on the floor.
- **Slowdown the longer you played.** Every hit left blood drops with physics behind and every geo coin stayed in the
  room, so each fight made the game a little slower. Both now clean themselves up. Some hit effects whose animation name
  had a stray line break in the game's script also froze on screen; they play and vanish now.
- **Text piled up on screen and never faded.** Text objects (tutorial prompts, titles, menus) were drawn at a fixed size
  regardless of their scale, at the wrong place inside their text boxes, and the game's `ColorFader` (which fades
  prompts in and out) did nothing. Text is now sized and placed like in the game, prompts fade in and out, and the King's
  Pass focus tutorial appears on screen where the game puts it.
- **The lore tablets** ("white eggs") in King's Pass show their text when you read them.
- **The soul orb** fills up from the bottom like the game's instead of showing a small icon inside it.
- **Falling out of Dirtmouth's well.** Rooms converted one at a time only had exits into rooms converted together with
  them; every exit now leads to its room (908 exits fixed), including the well down to the Forgotten Crossroads.
- **Bosses**: False Knight's stagger, the Mantis Lords (they start, can be hit, and the fight ends), the Soul Warrior's
  teleports, Godhome's boss scenes and pantheon chains, and bosses that stood still in Godhome were fixed. The Hunter's
  Journal shows a "Journal updated" notice.
- **Washed-out colours.** Glows, light shafts and haze use additive materials in the game (they add light to what's
  behind them). The converter baked them into the scenery as a half-transparent grey veil, so many rooms looked greyer
  and flatter than the original. They're now baked into separate glow layers that the 3DS draws with additive blending,
  and every room was rebuilt. (Found by comparing screenshots of 24 rooms against renders of the original scene data.)
- A little faster: the room's objects are sorted for drawing once per frame instead of once per depth layer. The SD log
  also gets a line every 10 seconds of play with the frame rate and how long updating and drawing took, so slow spots
  can be found on real hardware.

## Fixes in v1.0
- **Missing art in whole areas.** Much of the scenery in White Palace, the Hive, Kingdom's Edge, the Royal Waterways,
  the Abyss, Deepnest and parts of other areas lives in sprite atlases that the AssetRipper export doesn't link to its
  sprites, so those rooms showed little more than their collision (or black). `tools/match_textures.py --all` now
  matches every one of those atlases (1,867 textures) by their sprite layout, and the 351 affected rooms and their
  objects are rebuilt with their real art.
- **Shadows in front of the screen.** Close-to-camera silhouettes (the dark foreground shapes in front of the play area)
  were baked into one layer that moved at an average speed, so in some rooms they drifted into the middle of the screen
  or covered it completely (Godhome's Atrium was black). They're now split by depth, so each one scrolls at its own
  parallax like in the game, and anything closer than the camera's near plane is dropped.
- **Bosses and NPCs that stood still.** Many characters share their state machine through a PlayMaker template
  (193 templates, used by the Hollow Knight, Grimm, the Hive Knight, the Mantis Lords, the Nailmasters, Uumuu, benches, geo rocks, inspect prompts and many more). Those templates store their
  event names in a newer format the converter didn't read, so their scripts lost every event and stalled in their
  first state (Oro stood in place forever). Templates now convert completely, and every room was re-exported.
- **Boss fights that never ended.** Most Godhome arenas end the fight from the boss's own script (`EndGGBossScene`),
  which did nothing before; now it ends the fight and returns you to the statue (or on to the next pantheon boss).
  Bosses with a "special death" (the Nailmasters) also count as beaten now.
- A crash in Fungal Wastes (Fungus3_28) when an object's setup turned on another object before that object was ready.
- Object sprite sheets are stored as RGBA4 (dithered) instead of RGBA8, which halves their size on the SD card and in
  memory with no visible difference, so the whole game fits in about 1 GB.
- Soul totems now give their soul (the orbs used to fly off and never home in).
- Quick cast no longer fires 50 times in one press. The Stag's blanker no longer flashes white.
- Rooms whose collision data used an unusual encoding (White Palace 18) and a Resting Grounds room that ran out of
  memory during conversion are built now.



## v0.5.0: enemies run the game's own AI
Every enemy in King's Pass, the Crossroads and Greenpath now runs the game's real behaviour scripts instead of the
stand-in AI. The tools convert each enemy straight from the scene (its PlayMaker state machines, colliders,
Rigidbody2D and components), and the 3DS runs them through a small PlayMaker/Unity runtime:
- `source/fsm.c`: the state machine engine (states, transitions, global transitions, FINISHED, delayed events, events
  sent between objects).
- `source/fsm_actions.c`: 212 of the actions those scripts use, ported from the decompiled sources (walking, chasing,
  buzzing, lunges, jumps, aiming, spawning projectiles and shockwaves, animation events, collision-side checks, triggers...).
- `source/comps.c`: the game's enemy components: HealthManager (hits, blocking, invincibility, death and geo), Recoil,
  Walker, Crawler, Climber, AlertRange, LineOfSightDetector, DamageHero, EnemyDeathEffects and corpses, ObjectBounce,
  PersistentBoolItem.
- `source/uo.c`: GameObjects with transforms, parenting, colliders, simple rigidbody physics on the room's collision,
  tk2d sprite animators (frame events and completion callbacks), trigger enter/stay/exit, prefab spawning.

So the Husk Guard wakes up, swings its club and stomps out shockwaves, husks lunge with their wind-ups, Aspids spit,
Squits aim and dive, Gruz Mother sleeps until you hit her, then bounces around the arena, charges and slams, and every
enemy plays its own attack, turn and death animations with its own sounds. Killed enemies stay dead until you rest at
a bench, like in the game. False Knight and Hornet still use their v0.4 controllers for now.

Also in v0.5.0:
- **The real camera.** `source/camera.c` is a port of the game's CameraController/CameraTarget, with every room's
  camera lock zones (`tools/camlocks.py` -> `<Room>.cam`). The camera now frames rooms the way the game does, so it no
  longer drifts into the dark filler the game keeps off-screen, and the foreground shadows sit where they should.
- **Secret area covers fade out.** The black covers over hidden areas (the game's "unmasker" masks and the covers behind
  breakable walls) are no longer baked into the room art as permanent black boxes. They're drawn at runtime
  (`source/masks.c`, data from `tools/masks.py`) and fade away when you walk in, and stay gone, like in the game.
- **Sound fixes.** The Focus charging sound now stops when you stop focusing (it used to keep whispering for about
  20 seconds). Sounds are loaded when a room loads instead of the first time they play, which removes the delay on the
  first hit, jump or enemy sound.
- Fixed a memory corruption crash when entering some rooms (text data files weren't terminated).

## v0.4.7
- **Title screen and main menu** rebuilt from the game's own menu scene: the Voidheart background, logo,
  Trajan menu text, the animated fleur pointers, and the original `Title` music.
  Start Game → Continue / New Game, Options (music and sound volume), Quit Game.
- **The Knight**, using his real sprite frames and animations. Movement uses the game's own HeroController values:
  run 8.3, jump 16.65 held for 10 physics steps, variable jump height, max fall speed 20, nail timing 0.35s / 0.41s
  cooldown, alternating slashes, up and down slashes, pogo off enemies and spikes, recoil, damage knockback with
  1.3s invulnerability, soul gain per hit, Focus to heal (hold A, 33 soul), 5 masks.
- **King's Pass**, the full room built from the original scene: parallax art layers, collision, Crawlids, Vengeflies,
  breakable barriers, geo rocks, the chest, collapsing floor, falling stalactites, spikes with hazard respawn,
  and the area title.
- **Dirtmouth**, reached through King's Pass's exit, with its music and wind ambience and a working bench
  (press Up to sit: heals and saves, and you respawn there when you die).
- **The Forgotten Crossroads** (43 rooms, including the Shaman Temple) and **Greenpath** (29 rooms): 78 rooms in total
  with King's Pass, Dirtmouth and 4 interiors, wired together with the game's own transition data. You reach the Crossroads by dropping down Dirtmouth's well. Every enemy that lives there
  (Crawlids, Tiktiks, Husks, Gruzzers, Vengeflies, Mosskin, Aspids...) uses its real sprites and HP, with simplified AI:
  walkers patrol, husks charge, flyers chase, Baldurs sit still.
- **False Knight boss fight** in its arena, on the game's own animations: jump, jump-slam, mace swing with shockwaves,
  armour that breaks into a stun where you hit the head, three phases, rage, and the floor collapsing after you win.
  The boss music plays during the fight.
- **Doors:** press Up at a door to go in. Jiji's hut and Iselda's map shop in Dirtmouth work (empty rooms for now: no shops
  or dialogue yet), and so do the ruin house and Nailmaster room in the Crossroads. You come out of the same door.
- **Hornet** in Greenpath (simplified: she charges you). Beating her gives you the **Mothwing Cloak** (R to dash).
- **The Shade:** when you die, your geo stays with your Shade where you fell. Beat it to get the geo back.
- HUD (soul vessel, masks, geo counter), a minimap on the bottom screen, pause menu, and autosave on room change
  (`sdmc:/3ds/hk3ds/save.dat`).
- Memory: room art is stored as per-tile zlib streams and inflated on demand, so even the biggest rooms stay small in RAM.

v0.4.7: the map is connected properly now.
- **Back up to Dirtmouth:** the well's exit in Crossroads_01 had its target scene and entry point swapped in the game data,
  so it led nowhere. Press Up at the rope under the well to climb back to Dirtmouth.
- **Side and top exits that couldn't be touched:** many exit triggers sit just outside the room's solid edge in the original.
  They now reach into the room, so you can walk through them.
- **Coming up through a floor exit** (for example back into Dirtmouth) now launches the Knight high enough to land
  beside the hole instead of dropping straight back down.
- **Greenpath's vine platforms** were treated as spikes. They're now real platforms: hit the vine with the nail and
  the platform drops.
- **Hard landings** only happen after a long fall now, like in HK (1.1 s spent falling). Before, an ordinary jump that
  landed on a ledge could trigger the hard-landing stagger and freeze you for half a second.
- Story-state walls (the infected Crossroads' wall to the Brooding Mawlek's room, Shaman Temple pillar) and disabled exits
  no longer block the way; doors with no target in the scene now lead back where they came from.
- **Thorns** (Greenpath, Sheo's path and a few Crossroads rooms) used their whole bounding box as the hazard, which turned
  entire corridors into spikes. They now follow the thorn shapes.
- **Hazard respawn** puts you on the game's own respawn marker for that stretch. Before, it could put you back right over the
  thorns you just hit, so you took damage again and again until you died.
- **Pogo** bounces last as long as in HK (0.25 s instead of 0.1 s), so bouncing off enemies and spikes lifts you properly.
- **Durandas** (the acid flyers on the path to Sheo) hover in place and can't be killed, like in HK, so you can pogo across them.
  Before, they chased you.
- `HK_REACH=<Room>` in the PC harness tries every movement it can from each entrance of a room and lists the exits it
  reaches. `tools/reach_graph.py` joins the results into a check of the whole map. From a new game, 69 of the 78 rooms can be
  reached with jump and dash. The rest are behind Mantis Claw climbs, the City of Tears lift, the Crossroads to
  Blue Lake climb, or the Duranda pogo path. The tester can't do that pogo chain, so that one is only tested by hand.

v0.4.6: the CIA crashed at launch because its permissions (`cia/hk3ds.rsf`) didn't map DSP memory, so the sound
system faulted on its first write. The .3dsx was fine because the Homebrew Launcher's host app has that mapping.

v0.4.5: sprites facing right were drawn one sprite-width too far right on real hardware (citro2d keeps a mirrored
quad to the right of x and only flips the texture), so the Knight and enemies looked offset from where they really were.
Fixed, and the test harness now mirrors exactly like citro2d. Also fixes a crash when resuming from the HOME menu or
sleep: libctru's sound init used unchecked DSP replies (patched libctru: `tools/patches/libctru-ndsp-resume.patch`;
apply it to libctru and rebuild if you build on your PC), and the game now pauses its audio around suspend/sleep.

v0.4.4: nail hitboxes now match the game's own slash colliders (3.3 units of reach, active for the whole swing, each
target hit once per swing); taking damage uses the game's HeroBox; walkers no longer turn around constantly (the ledge
check was inverted when facing left); enemies are placed at their own colliders (repeated names like "Spitter" used to
share one position); scripted gates (Raising Pillar, Battle Gates, Toll Gates...) no longer wall off doorways; arena
wave spawns are left out until battles are simulated; the Knight can never be left inside terrain.
Tested on the Azahar 3DS emulator core (software renderer) and with a bot that plays every room.

v0.4.3: the nail uses the Knight's real swing sounds (`sword_1`-`sword_5`, picked at random) instead of the Shade's swoosh;
enemies no longer get stuck in terrain or jitter, and walkers climb small steps and follow slopes.

v0.4.2 fixed graphics on real hardware. Textures are now stored top row first, the way tex3ds does it (v0.4 stored them
flipped, and v0.4.1's attempted fix made citro2d treat every image as rotated). `tools/fix_vflip.py` converted the existing data;
`tools/texpack.py` now writes the correct layout. The PC test harness samples textures like the real PICA200, rotation flag included.

Circle Pad or D-Pad to move.
