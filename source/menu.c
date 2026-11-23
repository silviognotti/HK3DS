// Title screen + main menu, laid out like Hollow Knight's (Voidheart edition) main menu.
#include "menu.h"
#include "game.h"
#include "audio.h"
#include "ach.h"
#include "lang.h"
#include "version.h"
#include "patch.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef enum { M_MAIN, M_OPTIONS, M_ACH, M_SLOTS, M_CLEAR } MenuPage;
static MenuPage page;
static int sel;
static float t, flash, pointerT;
static float fadeIn;
static int bgFrame = -1;
static bool hasSave;
// the four save profiles, as on the game's profile screen
typedef struct { bool used; char area[64]; int masks, geo, secs; } Slot;
static Slot slots[4];
static SpriteSet areaArt;       // romfs:/gfx/areas.spr: the game's area art behind each save profile (loaded on the profile screen)
static void load_slots(void) {
    for (int i = 0; i < 4; i++) { Slot *q = &slots[i]; q->used = game_slot_info(i + 1, q->area, sizeof q->area, &q->masks, &q->geo, &q->secs); }
}

static const char *mainKeys[] = { "MAIN_START", "MAIN_OPTIONS", "MAIN_ACHIEVEMENTS", "MAIN_QUIT" };     // the game's MainMenu sheet
#define NMAIN 4
static int achTop, clearSlot;

void menu_enter(void) {
    page = M_MAIN; sel = 0; t = 0; fadeIn = 1;
    lang_gc();          // nothing of a language switched away from is on show here
    bgFrame = spr_named(&A.ui, "title_bg");
    game_migrate_saves(); load_slots();
    hasSave = slots[0].used || slots[1].used || slots[2].used || slots[3].used;
    audio_play_stream(0, "title", true, 1.5f);
    audio_stop_stream(1, 0.5f);
}

static void move_sel(int d, int n) {
    sel = (sel + d + n) % n;
    pointerT = 0;
    sfx_play("button", 0.55f);
}

int menu_update(float dt, u32 down) {
    t += dt; pointerT += dt;
    if (fadeIn > 0) fadeIn -= dt * 0.8f;
    if (flash > 0) flash -= dt;
    switch (page) {
        case M_MAIN:
            if (down & KEY_DUP) move_sel(-1, NMAIN);
            if (down & KEY_DDOWN) move_sel(1, NMAIN);
            if (down & KEY_A) {
                sfx_play("begin_button", 0.9f); flash = 0.25f;
                if (sel == 0) { page = M_SLOTS; sel = g_saveSlot - 1; load_slots(); if (!areaArt.loaded) spr_load(&areaArt, "romfs:/gfx/areas.spr"); }
                else if (sel == 1) { page = M_OPTIONS; sel = 0; }
                else if (sel == 2) { page = M_ACH; sel = 0; achTop = 0; }
                else if (sel == 3) return MENU_QUIT;
            }
            break;
        case M_SLOTS:
            if (down & KEY_DUP) move_sel(-1, 4);
            if (down & KEY_DDOWN) move_sel(1, 4);
            if (down & KEY_B) { page = M_MAIN; sel = 0; sfx_play("button", 0.5f); }
            if ((down & KEY_X) && slots[sel].used) { page = M_CLEAR; clearSlot = sel; sel = 1; sfx_play("button", 0.5f); }
            else if (down & KEY_A) {
                sfx_play("begin_button", 0.9f);
                audio_stop_stream(0, 1.5f);
                game_set_slot(sel + 1);
                if (areaArt.loaded) spr_free(&areaArt);      // the game needs the memory
                return slots[sel].used ? MENU_CONTINUE : MENU_NEWGAME;
            }
            break;
        case M_CLEAR:     // "Clear save?" (No by default)
            if (down & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) move_sel(1, 2);
            if ((down & KEY_A) && sel == 0) { game_clear_slot(clearSlot + 1); load_slots(); sfx_play("begin_button", 0.9f); }
            if ((down & KEY_A) || (down & KEY_B)) { page = M_SLOTS; sel = clearSlot; if (!(down & KEY_A)) sfx_play("button", 0.5f); }
            break;
        case M_OPTIONS:
            if (down & (KEY_DUP | KEY_DDOWN)) move_sel(down & KEY_DUP ? -1 : 1, 6);
            if (down & (KEY_DLEFT | KEY_DRIGHT | ((sel >= 2) ? KEY_A : 0))) {
                if (sel == 2) { lang_set((g_lang + 1) % LANG_COUNT); ach_reload(); }     // Language
                else if (sel == 3) g_hints = !g_hints;                                    // Button Hints
                else if (sel == 4) g_lowEffects = !g_lowEffects;
                else if (sel == 5) g_lowParticles = !g_lowParticles;
                else {
                    float d = (down & KEY_DLEFT) ? -0.1f : 0.1f;
                    float *v = sel == 0 ? &g_musicVolume : &g_sfxVolume;
                    *v += d; if (*v < 0) *v = 0; if (*v > 1) *v = 1;
                }
                sfx_play("button", 0.5f);
            }
            if (down & KEY_B) { page = M_MAIN; sel = 1; sfx_play("button", 0.5f); settings_save(); }
            break;
        case M_ACH: {
            int n = ach_count();
            if ((down & KEY_DUP) && sel > 0) { sel--; sfx_play("button", 0.5f); }
            if ((down & KEY_DDOWN) && sel < n - 1) { sel++; sfx_play("button", 0.5f); }
            if (sel < achTop) achTop = sel;
            if (sel > achTop + 4) achTop = sel - 4;
            if (down & KEY_B) { page = M_MAIN; sel = 2; sfx_play("button", 0.5f); }
            break; }
    }
    return MENU_NONE;
}

void menu_draw_pointers(float cx, float y, float halfw, float pt) {     // also used by the pause menu
    float save = pointerT; pointerT = pt;
    void draw_pointers_(float, float, float); draw_pointers_(cx, y, halfw);
    pointerT = save;
}
static void draw_pointers(float cx, float y, float halfw) { void draw_pointers_(float, float, float); draw_pointers_(cx, y, halfw); }
void draw_pointers_(float cx, float y, float halfw) {
    // animated fleur pointers on both sides of the selected item
    int a = spr_anim(&A.ui, "pointer");
    if (a < 0) return;
    const Anim *an = &A.ui.anims[a];
    int fi = (int)(pointerT * an->fps);
    if (fi >= an->count) fi = an->count - 1;
    int f = A.ui.animFrames[an->first + fi];
    spr_draw(&A.ui, f, cx - halfw - 4, y, 1, 1, 1);
    spr_draw(&A.ui, f, cx + halfw + 4, y, -1, 1, 1);
}

static void draw_items(const char **items, int n, float y0, float dy) {     // items: text, or a MainMenu key
    for (int i = 0; i < n; i++) {
        float y = y0 + i * dy;
        u32 c = C2D_Color32(255, 255, 255, 255);
        font_draw_centered(&A.fontMenu, items[i], TOP_W / 2, y, 1, c);
        if (i == sel) draw_pointers(TOP_W / 2, y + A.fontMenu.lineh / 2, font_width(&A.fontMenu, items[i]) / 2);
    }
}

void menu_draw_top(void) {
    C2D_DrawRectSolid(0, 0, 0.1f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 255));
    if (bgFrame >= 0) spr_draw(&A.ui, bgFrame, 0, 0, 1, 1, 1);
    switch (page) {
        case M_MAIN: {
            const char *items[NMAIN];
            for (int i = 0; i < NMAIN; i++) items[i] = lang_get_or_key(mainKeys[i], "MainMenu");
            draw_items(items, NMAIN, 126, 21);
            font_draw(&A.fontText, "v" HK3DS_VERSION HK3DS_EDITION, TOP_W - 8 - font_width(&A.fontText, "v" HK3DS_VERSION HK3DS_EDITION) * 0.8f, TOP_H - 16, 0.8f, C2D_Color32(150, 150, 160, 255));
            {   // Luma is loading an old patch's code over a newer full install (patch.c)
                const char *w = hk_patch_warning();
                for (int ln = 0; w && *w; ln++) {
                    const char *e = strchr(w, '\n'); char line[128]; int n = e ? (int)(e - w) : (int)strlen(w);
                    if (n > 127) n = 127;
                    memcpy(line, w, n); line[n] = 0;
                    font_draw_centered(&A.fontText, line, TOP_W / 2, 4 + ln * 14, 0.8f, C2D_Color32(255, 200, 120, 255));
                    w = e ? e + 1 : NULL;
                }
            }
            break; }
        case M_SLOTS: case M_CLEAR: {
            C2D_DrawRectSolid(0, 0, 0.5f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 200));
            font_draw_centered(&A.fontMenu, lang_get_or_key("SCREEN_SAVE_PROFILES", "MainMenu"), TOP_W / 2, 4, 1, C2D_Color32(255, 255, 255, 255));
            int hi = spr_anim(&A.hud, "Health Idle"), ci = spr_anim(&A.hud, "Coin Idle");
            for (int i = 0; i < 4; i++) {
                const Slot *q = &slots[i];
                float x = 40, y = 30 + i * 52, w = TOP_W - 80, h = 46;
                bool on = (page == M_SLOTS && i == sel) || (page == M_CLEAR && i == clearSlot);
                u32 edge = on ? C2D_Color32(230, 230, 230, 255) : C2D_Color32(90, 90, 100, 255);
                C2D_DrawRectSolid(x, y, 0.5f, w, h, C2D_Color32(20, 20, 28, on ? 235 : 180));
                C2D_DrawRectSolid(x, y, 0.5f, w, 1, edge); C2D_DrawRectSolid(x, y + h - 1, 0.5f, w, 1, edge);
                C2D_DrawRectSolid(x, y, 0.5f, 1, h, edge); C2D_DrawRectSolid(x + w - 1, y, 0.5f, 1, h, edge);
                char num[4]; snprintf(num, sizeof num, "%d", i + 1);
                font_draw(&A.fontMenu, num, x + 8, y + 12, 1, C2D_Color32(200, 200, 200, 255));
                if (!q->used) { font_draw_centered(&A.fontMenu, lang_get_or_key("PROFILE_NEW_GAME", "MainMenu"), TOP_W / 2, y + 12, 1, C2D_Color32(255, 255, 255, 255)); }
                else {
                    int af = areaArt.loaded ? spr_named(&areaArt, q->area) : -1;
                    if (af < 0 && areaArt.loaded) af = spr_named(&areaArt, "Hallownest");
                    if (af >= 0) { const Frame *fr = &areaArt.frames[af]; spr_draw(&areaArt, af, x + 1, y + 1, (w - 2) / fr->w, (h - 2) / fr->h, on ? 1.0f : 0.55f); }
                    font_draw(&A.fontMenu, TR(q->area), x + 30, y + 3, 0.9f, C2D_Color32(255, 255, 255, 255));
                    for (int m = 0; m < q->masks && m < 12 && hi >= 0; m++) spr_draw(&A.hud, A.hud.animFrames[A.hud.anims[hi].first], x + 36 + m * 11, y + 33, 0.7f, 0.7f, 1);
                    if (ci >= 0) spr_draw(&A.hud, A.hud.animFrames[A.hud.anims[ci].first], x + 186, y + 33, 0.8f, 0.8f, 1);
                    char g[16]; snprintf(g, sizeof g, "%d", q->geo);
                    font_draw(&A.fontText, g, x + 196, y + 26, 0.9f, C2D_Color32(230, 230, 230, 255));
                    char tm[24]; snprintf(tm, sizeof tm, "%dh %02dm", q->secs / 3600, q->secs / 60 % 60);
                    font_draw(&A.fontText, tm, x + w - 8 - font_width(&A.fontText, tm) * 0.9f, y + 26, 0.9f, C2D_Color32(200, 200, 200, 255));
                }
                if (on && page == M_SLOTS) draw_pointers(TOP_W / 2, y + h / 2, w / 2 + 6);
            }
            if (page == M_CLEAR) {
                C2D_DrawRectSolid(0, 0, 0.5f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 170));
                char t[48]; snprintf(t, sizeof t, TR("Clear save %d?"), clearSlot + 1);
                font_draw_centered(&A.fontMenu, t, TOP_W / 2, 92, 1, C2D_Color32(255, 255, 255, 255));
                const char *yn[2] = { lang_get_or_key("NAV_YES", "MainMenu"), lang_get_or_key("NAV_NO", "MainMenu") };
                for (int k = 0; k < 2; k++) {
                    float xx = TOP_W / 2 + (k ? 50 : -50);
                    font_draw_centered(&A.fontMenu, yn[k], xx, 126, 1, C2D_Color32(255, 255, 255, 255));
                    if (k == sel) draw_pointers(xx, 126 + A.fontMenu.lineh / 2, font_width(&A.fontMenu, yn[k]) / 2);
                }
            }
            break;
        }
        case M_OPTIONS: {
            char a[64], b[64], c[64], d[64], e[64], f[64];
            snprintf(a, sizeof a, "%s  %d", lang_get_or_key("AUDIO_MUSIC_VOL", "MainMenu"), (int)roundf(g_musicVolume * 10));
            snprintf(b, sizeof b, "%s  %d", lang_get_or_key("AUDIO_SOUND_VOL", "MainMenu"), (int)roundf(g_sfxVolume * 10));
            snprintf(c, sizeof c, "%s  %s", lang_get_or_key("GAME_LANGUAGE", "MainMenu"), lang_name(g_lang));
            snprintf(d, sizeof d, "%s  %s", TR("Button Hints"), lang_get_or_key(g_hints ? "MOH_ON" : "MOH_OFF", "MainMenu"));
            snprintf(e, sizeof e, "Effects  %s", g_lowEffects ? "Low" : "Normal");
            snprintf(f, sizeof f, "Particles  %s", g_lowParticles ? "Low" : "Normal");
            const char *items[6] = { a, b, c, d, e, f };
            draw_items(items, 6, 116, 18);
            break;
        }
        case M_ACH: {      // the game's achievements page: unlocked ones bright, the rest dimmed
            C2D_DrawRectSolid(0, 0, 0.5f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 238));
            char hd[64]; snprintf(hd, sizeof hd, "%s  %d / %d", lang_get_or_key("MAIN_ACHIEVEMENTS", "MainMenu"), ach_unlocked(), ach_count());
            font_draw_centered(&A.fontMenu, hd, TOP_W / 2, 8, 1, C2D_Color32(255, 255, 255, 255));
            for (int i = achTop; i < achTop + 5 && i < ach_count(); i++) {
                const char *ti, *tx; bool got = ach_get(i, &ti, &tx);
                float y = 44 + (i - achTop) * 38;
                if (i == sel) C2D_DrawRectSolid(14, y - 3, 0.6f, TOP_W - 28, 36, C2D_Color32(255, 255, 255, 30));
                u32 c1 = got ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(110, 110, 110, 255), c2 = got ? C2D_Color32(200, 200, 200, 255) : C2D_Color32(90, 90, 90, 255);
                font_draw(&A.fontText, ti, 24, y, 1, c1);
                font_draw(&A.fontText, tx, 24, y + 16, font_fit(&A.fontText, tx, TOP_W - 48, 0.85f), c2);
            }
            if (g_hints) font_draw_centered(&A.fontText, TR("B: back"), TOP_W / 2, TOP_H - 18, 0.85f, C2D_Color32(160, 160, 160, 255));
            break; }
    }
    if (flash > 0) C2D_DrawRectSolid(0, 0, 1, TOP_W, TOP_H, C2D_Color32f(1, 1, 1, flash * 0.3f));
    if (fadeIn > 0) C2D_DrawRectSolid(0, 0, 1, TOP_W, TOP_H, C2D_Color32f(0, 0, 0, fadeIn > 1 ? 1 : fadeIn));
}

bool g_hdLowMem;      // HD build started in a New 3DS's old 64 MB mode (main.c): the title screen explains
void menu_draw_bottom(void) {
    C2D_DrawRectSolid(0, 0, 0.1f, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 255));
#ifdef __HOST__
    if (getenv("HK_HDLOWMEM")) g_hdLowMem = true;
#endif
    if (g_hdLowMem) {
        static const char *L[] = { "HD needs the New 3DS's full 124 MB of", "memory, but this console started the game",
            "in the old 64 MB mode: backgrounds will pop", "in and out and some sprites will be missing.", "",
            "Luma3DS's \"New 3DS CPU\" setting (Clock+L2)", "does this: set it to Off (hold SELECT while",
            "the console starts). The game runs at 804 MHz", "with the L2 cache by itself. A Luma plugin", "can also cause it.", "",
            "The standard version runs fine in 64 MB." };
        font_draw_centered(&A.fontText, "Low memory", BOT_W / 2, 14, 1, C2D_Color32(230, 200, 120, 255));
        for (unsigned i = 0; i < sizeof L / sizeof *L; i++) font_draw_centered(&A.fontText, L[i], BOT_W / 2, 44 + i * 15, 0.72f, C2D_Color32(200, 200, 210, 255));
        return;
    }
    int tc = spr_named(&A.ui, "team_cherry");
    if (tc >= 0) spr_draw(&A.ui, tc, BOT_W / 2 - A.ui.frames[tc].w / 2, 70, 1, 1, 0.8f);
    font_draw_centered(&A.fontText, TR("Fan demake for personal use"), BOT_W / 2, 170, 1, C2D_Color32(140, 140, 150, 255));
    if (g_hints) font_draw_centered(&A.fontText, TR(page == M_SLOTS ? "A: play    X: clear save    B: back" : "D-Pad: select    A: confirm    B: back"), BOT_W / 2, 210, 0.9f, C2D_Color32(110, 110, 120, 255));
}
