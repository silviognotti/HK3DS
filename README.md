# HK3DS: Hollow Knight on the Nintendo 3DS

A fan port of Hollow Knight to the (New) Nintendo 3DS and 2DS family. The game's own rooms, art, music, scripts and
state machines are converted from **your own copy of Hollow Knight** and run on a 3DS engine written for this project.

> **No game files are included.** This repository contains only the port's code and the tools that convert your copy
> of the game. Hollow Knight and all of its assets belong to Team Cherry; buy the game to use this. The project is a
> personal, non-commercial fan work, not affiliated with or endorsed by Team Cherry. Don't upload converted assets,
> `romfs/` folders or built `.3dsx` / `.cia` files.

<!-- Showcase video: open this README in GitHub's editor and drag hk3ds_showcase.mp4 here to embed it. -->

## Two versions

| | Standard | HD |
|---|---|---|
| Consoles | every 3DS and 2DS | New 3DS, New 3DS XL, New 2DS XL |
| Top screen | 400x240 | 800x240 (the New 3DS's wide mode) |
| Art | room art, sprites, fonts and HUD at 3DS resolution | everything converted again with twice the detail across |
| Title | Hollow Knight 3DS | Hollow Knight 3DS HD |

Both install side by side and share the same save files (`SD:/3ds/hk3ds/`).

## What's in
- **The whole map**: King's Pass to Godhome, 480 scenes, each converted from the game's own scene.
- **The game's own logic**: rooms run the game's PlayMaker state machines and ported components, so enemies, bosses,
  NPCs, doors, levers, lifts and trams behave as they do in the game. The Knight runs on his own state machines too.
- **NPCs, shops and dialogue**: Elderbug, Cornifer and his maps, Iselda, Sly, Salubra, the Nailsmith, the Stags, the
  Grubfather, the Seer and the rest, with their voices and conversation scripts.
- **All 40 charms**, every spell and ability, nail arts, the Dream Nail and essence.
- **Bosses and Godhome**: arenas load the way the game does; the Hall of Gods and all five pantheons.
- **The Hunter's Journal**, the inventory, the map on the bottom screen (it fills in at benches with the Quill).
- **Endings** with their titles and the credits.
- **Music and sound**: each room's music cues and ambience, the game's sound effects.
- **English and Spanish**, and Options for Button Hints, sound and more.

Controls: **B** jump · **Y** nail (Up/Down to aim, hold for nail arts) · **A** focus (hold) / cast (tap) · **R** dash ·
**X** dream nail · **ZR** quick cast · **ZL** super dash · **L** quick map · **SELECT** inventory · **START** pause ·
**Up** talk / inspect / enter doors · Circle Pad or D-Pad to move.

## Building it

The build converts your own copy of Hollow Knight into the port's data, then compiles the game. It's one command, but
the first build takes a long time: it converts all 480 scenes, which takes hours (most of a day on a slow PC; `--jobs`
below converts several rooms at once). It needs about 15 GB of free disk space. It's tested on Linux (Ubuntu 24.04);
on Windows, use WSL.

### 1. Install the tools
On Ubuntu or Debian (or Ubuntu in WSL on Windows):
```
sudo apt update
sudo apt install git python3 python3-numpy python3-pil python3-yaml python3-cryptography ffmpeg
```
(On a new WSL install, `apt install` says *Unable to locate package* until `sudo apt update` has run. Use the apt
ffmpeg, not the snap one: the snap version isn't found by the build in WSL and can't read the Windows drives.)
Then **devkitPro**, which compiles 3DS programs: follow [devkitPro's Getting Started](https://devkitpro.org/wiki/Getting_Started)
to install its package manager, then run
```
sudo dkp-pacman -S 3ds-dev
```
and open a new terminal (that sets `DEVKITPRO` and `DEVKITARM`).

For the `.cia` (the installable version) you also need **makerom**
([Project_CTR releases](https://github.com/3DSGuy/Project_CTR/releases)) and **bannertool**
([bannertool](https://github.com/Steveice10/bannertool)). Put both programs in `/opt/devkitpro/tools/bin/` (or anywhere
on your `PATH`). Without them you still get the `.3dsx`, which runs from the Homebrew Launcher.
When it's done, the build lists the files and where each one goes on your SD card.

### 2. Export the game
You need Hollow Knight for PC (Steam or GOG) and [AssetRipper](https://github.com/AssetRipper/AssetRipper/releases).
1. Start AssetRipper, choose **File → Open Folder** and pick the game's `Hollow Knight_Data` folder (inside the game's
   install folder). Wait until it has loaded everything.
2. **Export → Export All Files**, pick an empty folder, and choose **Export Unity Project**. This takes a while.
3. **Export → Export All Files** again, pick another empty folder, and choose **Export Primary Content**.
4. Make a folder called `hkdata` next to this repository's folder (so `HK3DS/` and `hkdata/` sit side by side) and
   put the exports in it like this:
```
hkdata/
  ex/                 everything inside  <Unity project export>/ExportedProject/Assets/
                      (Scenes, GameObject, MonoBehaviour, Texture2D, Sprite, AudioClip, Font, Resources, ...)
  Assembly-CSharp/    <Unity project export>/ExportedProject/Assets/Scripts/Assembly-CSharp/  (the game's scripts)
  hs/Texture2D/       the Texture2D folder of the primary content export (every texture as a .png)
```
If you keep `hkdata` somewhere else, tell the build where: `export HKDATA=/path/to/hkdata`.

### 3. Build
From the repository folder:
```
tools/build.sh --check     # optional: checks the tools and the export folder, and says what's missing
tools/build.sh             # the standard version: hk3ds_std.romfs (the game data), hk3ds.3dsx and hk3ds.cia
tools/build.sh --hd        # the standard version and the HD one (hk3ds_hd.romfs, hk3ds_hd.cia: New 3DS family)
tools/build.sh --jobs 4    # converts 4 rooms at once: much faster with several CPU cores (1-2 GB of memory each)
```
It checks everything it needs first and tells you what's missing and how to install it. Then it runs each step in
order and shows its progress. If a step fails, the build stops and shows that step's log (`work/logs/<step>.log`).
Fix the problem and run the same command again: it carries on from where it stopped, so the rooms it already
converted aren't redone. `tools/build.sh --list` lists the steps, and `--from <step>` redoes a step and everything
after it.

What the steps do:
| Step | What it makes |
|---|---|
| `index` | indexes of your export that the converters read (`tools/index_export.py`, `alpha_cache.py`, `match_textures.py`) |
| `lookups` | the tables that go by asset IDs, found again in your export by name (`tools/make_lookups.py`, into `hkdata/lookups/`): music and ambience cues, state machine templates, lit and glow materials, a few sprites. AssetRipper gives assets new IDs on every export, so these can't come with the repository. Check the `WARNING` lines in `work/logs/lookups.log` |
| `objects` | every room's live objects: enemies, NPCs, levers, their state machines and the prefabs they spawn (`build_objs.py`); the Knight's own (`build_knight.py`) |
| `rooms` | every room's collision, exits and baked parallax art (`build_room.py`); the slowest step |
| `install` | the rooms in `romfs/rooms/`: compressed, with camera locks, secret masks and linked exits |
| `pack` | the objects' data, sprites and sounds (`pack_objs.py`), their particle systems (`obj_particles.py`) and per-frame hitboxes (`tk_colliders.py`) |
| `particles` | the rooms' own particle effects (`particles.py`) |
| `gamedata` | the Knight's and enemies' sprites, the HUD, fonts, menus, map, Journal, music, text in English and Spanish |
| `compile` | `hk3ds_std.romfs` (the game data file, `make data`), `hk3ds.3dsx` and `hk3ds.cia` (`make`, `make cia`) |
| `hd_*` (`--hd`) | `romfs_hd/`: the same data with twice the detail, then `hk3ds_hd.romfs` and `hk3ds_hd.cia` |

The build makes plain stand-in icons and banners (`icon.png`, `cia/banner.png`...) if you don't have your own; put
your own art there any time. `tools/data/` holds the port's own lookup tables (no game assets): which game script each
component is, the objects each room converts. The tables keyed by asset IDs there fit only the export the repository
was built from; the `lookups` step makes yours.

## Installing
From v1.1 the game comes in two parts: a small program (the CIA, or the `.3dsx`) and one big game data file that goes
on the SD card. Updates after that only replace the small program and a few changed files.
1. Copy the game data file to `SD:/3ds/hk3ds/`: `hk3ds_std.romfs` for the standard version, `hk3ds_hd.romfs` for HD.
2. Install `hk3ds.cia` (or `hk3ds_hd.cia`) with **FBI**; it replaces an older install, and your saves stay. Or, for
   the standard version, put `hk3ds.3dsx` in `SD:/3ds/hk3ds/` and start it from the Homebrew Launcher instead.
3. Sound needs `SD:/3ds/dspfirm.cdc` (dump it with DSP1).

Coming from v1.0.x? Delete the game's folders in `SD:/luma/titles/` (`000400000F4C3D00` for the standard version,
`000400000F4C3E00` for HD) if your earlier patches left them there: Luma would otherwise keep running the old code,
which can't find the new data file. The old `SD:/3ds/hk3ds/patch/` folder can go too.

If the game says its data is missing, the data file isn't in `SD:/3ds/hk3ds/` or has another name.

The HD build needs the New 3DS's full 124 MB. If line 2 of `SD:/3ds/hk3ds/log.txt` says `low memory`, turn off
Luma3DS's "New 3DS CPU" setting (hold SELECT at boot): both builds already run at 804 MHz with the L2 cache.
`make FULL=1` (and `make FULL=1 cia`) still builds the old all-in-one `.3dsx` / CIA with the data inside.

## Updating
Updates come as one zip per version, `hk3ds_update_v<version>_std.zip` or `_hd.zip`:
1. Copy the zip's `3ds` folder to the root of the SD card (merge it, replace files).
2. Install the zip's CIA with FBI (or use its `.3dsx`).
3. The title screen shows the new version.

The data file stays as it was. Files changed since its release go in `SD:/3ds/hk3ds/patch/std|hd/`, and the game reads
them from there instead of the data file (`source/patch.c`). Updates are cumulative, and an update's files are only
used by the version they were made for; the title screen warns you when the data file is another version than the
update expects.

`tools/make_patch.py` builds an update from the small CIAs (`make cia`, `make HD=1 cia`) and the file lists of the data
files you released (`tools/make_patch.py --record-base patches/base_v<version>` when you release one). Before v1.1, the
code went in through Luma3DS's game patching (`SD:/luma/titles/<title id>/code.bin`); the tool still makes those
patches for a base release older than v1.1.

## Testing without a 3DS
`host/` is a small software stand-in for libctru and citro2d that runs the real game code on a PC, headless, with
scripted input and screenshots:
```
gcc -O2 -w -Ihost -Isource -D__HOST__ source/*.c host/host.c -lm -lpthread -o host/hk3ds_host   # add -DHK_HD for HD
HK_SCRIPT="150:;1:A;40:;1:A;200:RIGHT" HK_SHOTS="300" ./host/hk3ds_host     # -> shots/f00300.png
HK_START="Crossroads_10 -1 -1" ./host/hk3ds_host                             # start a new game in any room
```
`tools/regress.sh` runs the regression scenes. `tools/emu/` has a libretro front end (`hkrec.c`) that runs the real
`.3dsx` in the Azahar core with scripted input and records video and sound (`HK_REC_LEN` starts when a room fades in).
Azahar's software renderer shows only half of the New 3DS's 800-pixel wide mode; `tools/emu/azahar-wide-mode.patch`
fixes that for HD recordings.

## Layout
```
source/    the game (C, libctru + citro2d); source/ext/ has the bundled libraries
host/      the PC stand-in for testing
tools/     the converters (Python) and test scripts
cia/       CIA metadata (.rsf)
install/   the Wi-Fi installer (bundles segno for the QR code)
```

## Licenses
The port's code and tools are under the MIT license (`LICENSE`). Bundled libraries keep their own licenses:
libdeflate (MIT), tinf (zlib), stb_vorbis and stb_image_write (public domain / MIT), segno (BSD-3-Clause).
Hollow Knight © Team Cherry. None of its assets are in this repository.

See `CHANGELOG.md` for the version history.
