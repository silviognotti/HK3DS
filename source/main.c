#include <sys/stat.h>
// HK3DS - a Hollow Knight demake for the (New) Nintendo 3DS. Personal fan project.
#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "audio.h"
#include "patch.h"
#include "game.h"
#include "lang.h"
#include "ach.h"
#include "menu.h"
#include "room.h"
#include "version.h"

typedef enum { ST_MENU, ST_FADE_TO_GAME, ST_GAME } State;

// the game data (v1.1): the data file on the SD card, mounted as romfs:/; else the program's own RomFS (a full CIA or
// .3dsx with the data inside). Returns where it came from, or NULL when neither has it.
static const char *data_mount(void) {
#ifdef __HOST__
    romfsInit(); return "romfs/";
#else
    struct stat st;
    Handle fh;
    if (R_SUCCEEDED(FSUSER_OpenFileDirectly(&fh, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                                            fsMakePath(PATH_ASCII, "/3ds/hk3ds/" HK3DS_DATAFILE), FS_OPEN_READ, 0))) {
        if (R_SUCCEEDED(romfsMountFromFile(fh, 0, "romfs"))) {
            if (stat("romfs:/gfx/ui.spr", &st) == 0) return "SD:/3ds/hk3ds/" HK3DS_DATAFILE;
            romfsUnmount("romfs");        // not a data file of this game (a broken copy): try the program's own
        } else FSFILE_Close(fh);
    }
    if (R_SUCCEEDED(romfsMountSelf("romfs"))) {
        if (stat("romfs:/gfx/ui.spr", &st) == 0) return "the program's own RomFS";
        romfsUnmount("romfs");
    }
    return NULL;
#endif
}

int main(int argc, char **argv) {
    osSetSpeedupEnable(true);     // New 3DS: 804 MHz
    const char *dataSrc = data_mount();
    gfxInitDefault();
#ifndef __HOST__
    if (!dataSrc) {     // the small CIA / .3dsx without its data file
        consoleInit(GFX_TOP, NULL);
        printf("\n  Hollow Knight 3DS" HK3DS_EDITION " v" HK3DS_VERSION "\n\n"
               "  The game data is missing. Copy\n\n    " HK3DS_DATAFILE "\n\n  into the folder\n\n    SD:/3ds/hk3ds/\n\n"
               "  (it comes in the same download as\n  this game), then start it again.\n\n"
               "  Already there? Check its name: it\n  must be exactly " HK3DS_DATAFILE ".\n\n"
               "  Press START to exit.\n");
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
        gfxExit(); return 0;
    }
#endif
#ifdef HK_HD
#ifndef __HOST__
    { bool n3 = false; APT_CheckNew3DS(&n3);
      if (!n3) {     // the HD art needs a New 3DS's memory and speed; the standard build runs on every model
          consoleInit(GFX_TOP, NULL);
          printf("\n\n  Hollow Knight 3DS HD needs a New 3DS,\n  New 3DS XL or New 2DS XL.\n\n"
                 "  On this console, install the standard\n  version instead (hk3ds.cia).\n\n  Press START to exit.\n");
          while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
          gfxExit(); romfsExit(); return 0; }
      // a New 3DS that started the game in the old 64 MB mode instead of the 124 MB the CIA asks for (seen on a New 2DS
      // XL: a console setting or plugin overrode it). The HD art doesn't fit, so the title screen says so (menu.c)
      { extern bool g_hdLowMem; g_hdLowMem = envGetLinearHeapSize() < (56u << 20); } }
#endif
    gfxSetWide(true);     // HD build (New 3DS XL / New 2DS XL): the top screen at 800x240 (citro2d keeps 400x240 coordinates)
#endif
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(4096);
    C2D_Prepare();
#ifndef __HOST__
    // citro2d draws with a depth test (GEQUAL), so anything drawn after a dimmer at a higher depth was hidden behind it:
    // the shop list, pause menu, inventory/charms text and achievement popups showed as dark boxes on a real 3DS.
    // Everything here is drawn back to front already (as on the PC build), so draw order alone decides.
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
#endif
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/hk3ds", 0777);
    hk_patch_init();     // a data patch on the SD card for this version (patch.c)
#ifndef __HOST__
    hk_log("boot v" HK3DS_VERSION HK3DS_EDITION ": heap %luK linear %luK", (unsigned long)(envGetHeapSize() / 1024), (unsigned long)(envGetLinearHeapSize() / 1024));
    hk_log("game data: %s", dataSrc);
    { bool n3ds = false; APT_CheckNew3DS(&n3ds);
      // an Old 3DS (64 MB for everything), or a New 3DS that started the game with less than the CIA's 124 MB (a .3dsx
      // from a launcher running in 80 or 96 MB, a memory-hungry plugin): 16-bit sprite pages and leaner caches. The
      // full-colour set needs the whole 124 MB; with less, sprites and backgrounds failed to load.
      g_lowmem = envGetLinearHeapSize() < (56u << 20);
      extern bool g_lowEffects, g_lowParticles;
      g_lowEffects = g_lowmem;
      g_lowParticles = g_lowmem;
      hk_log("%s 3DS, %s memory", n3ds ? "New" : "Old", g_lowmem ? "low" : "full"); }
#endif
    audio_init();
    hk_log("audio ok");
    if (!assets_load()) {
        consoleInit(GFX_BOTTOM, NULL);
        printf("HK3DS: missing data in romfs.\nPress START to exit.\n");
        while (aptMainLoop()) { hidScanInput(); if (hidKeysDown() & KEY_START) break; gspWaitForVBlank(); }
        gfxExit(); return 0;
    }
    hk_log("assets ok");
    settings_load();     // language and volumes (sdmc:/3ds/hk3ds/settings.txt)
#ifdef __HOST__
    if (getenv("HK_LANG")) lang_set(atoi(getenv("HK_LANG")));     // tests: 1 = Spanish
#endif
    ach_reload();        // the achievement titles in that language
    room_tilecache_init();

#ifdef __HOST__
    if (getenv("HK_CAMTOUR")) { extern void game_camtour_main(void); game_camtour_main(); return 0; }
    if (getenv("HK_REACH")) { extern void game_reach_main(void); game_reach_main(); return 0; }
#endif
    State st = ST_MENU;
    menu_enter();
    float fade = 0;
    int pending = 0;
    u64 last = osGetTime();
    while (aptMainLoop()) {
        u64 tFrame = svcGetSystemTick();
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld(), up = hidKeysUp();
        circlePosition cp; hidCircleRead(&cp);
        u64 now = osGetTime();
        float dt = (now - last) / 1000.0f; last = now;
        if (dt > 1 / 20.0f) dt = 1 / 20.0f;
        if (dt <= 0) dt = 1 / 60.0f;
        audio_update(dt);

        if (st == ST_MENU) {
            int r = menu_update(dt, down);
            if (r == MENU_QUIT) break;
            if (r == MENU_NEWGAME || r == MENU_CONTINUE) { st = ST_FADE_TO_GAME; pending = r; fade = 0; }
        } else if (st == ST_FADE_TO_GAME) {
            fade += dt * 1.2f;
            if (fade >= 1) {
                if (pending == MENU_CONTINUE) { if (!game_continue()) game_new(); }
                else game_new();
                st = ST_GAME;
            }
        } else {
            if (game_update(dt, down, held, up, cp) == 1) { game_end(); st = ST_MENU; menu_enter(); }
        }
        u64 tUpd = svcGetSystemTick();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        u64 tBeg = svcGetSystemTick();
        { extern u32 game_clear_color(void); C2D_TargetClear(top, st == ST_GAME ? game_clear_color() : C2D_Color32(0, 0, 0, 255)); }
        C2D_SceneBegin(top);
        gfx_tint_mode(false);
        if (st == ST_GAME) game_draw_top(); else menu_draw_top();
        if (st == ST_FADE_TO_GAME) C2D_DrawRectSolid(0, 0, 1, TOP_W, TOP_H, C2D_Color32f(0, 0, 0, fade > 1 ? 1 : fade));
        C2D_TargetClear(bot, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(bot);
        if (st == ST_GAME) game_draw_bottom(); else menu_draw_bottom();
        if (st == ST_FADE_TO_GAME) C2D_DrawRectSolid(0, 0, 1, BOT_W, BOT_H, C2D_Color32f(0, 0, 0, fade > 1 ? 1 : fade));
        u64 tDraw = svcGetSystemTick();
        C3D_FrameEnd(0);
        {   // frame timing in the SD log every 10 s of play (update / GPU wait / draw CPU, ms per frame)
            static double aU, aW, aD; static int nF; static u64 t0;
            aU += (tUpd - tFrame) / 268111.856; aW += (tBeg - tUpd) / 268111.856; aD += (tDraw - tBeg) / 268111.856; nF++;
            if (!t0) t0 = tFrame;
            if (tDraw - t0 > 268111856ULL * 10) {
                if (st == ST_GAME) { extern void uo_log_stats(void); hk_log("perf: %.1f fps, update %.1f ms, gpu wait %.1f ms, draw %.1f ms", nF / ((tDraw - t0) / 268111856.0), aU / nF, aW / nF, aD / nF); uo_log_stats(); }
                aU = aW = aD = 0; nF = 0; t0 = tDraw;
            }
        }
    }
    game_end();
    audio_exit();
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
