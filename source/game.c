// HK3DS gameplay: the Knight, enemies, room entities, HUD, camera, transitions.
// Movement constants are the values serialized on the game's HeroController (Knight prefab).
#include "game.h"
#include "audio.h"
#include "hero.h"
#include "world.h"
#include "ach.h"
#include "uo.h"
#include "comps.h"
#include "pd.h"
#include "camera.h"
#include "masks.h"
#include "particles.h"
#include "dialogue.h"
#include "lang.h"
#include "menu.h"
#include "menus.h"
#include "msgs.h"
#include "shop.h"
#include "input.h"
#include "charms.h"
#include "inv.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

Assets A;

/* ---------------- HeroController constants ---------------- */
#define RUN_SPEED            8.3f
#define JUMP_SPEED           16.65f
#define JUMP_TIME            (10 * 0.02f)   // JUMP_STEPS 9 -> 10 fixed steps
#define JUMP_TIME_MIN        (5 * 0.02f)    // JUMP_STEPS_MIN 4
#define GRAVITY              (60.0f * 0.79f) // DEFAULT_GRAVITY scale
#define MAX_FALL_VELOCITY    20.0f
#define ATTACK_DURATION      0.35f
#define ATTACK_COOLDOWN_TIME 0.41f
#define ALT_ATTACK_RESET     0.5f
#define BOUNCE_VELOCITY      12.0f
#define BOUNCE_TIME          0.25f   // HeroController BOUNCE_TIME
#define RECOIL_HOR_VELOCITY  8.0f
#define RECOIL_HOR_TIME      0.1f
#define RECOIL_VELOCITY      15.0f          // knockback when damaged
#define RECOIL_DURATION      0.2f
#define INVUL_TIME           1.3f
#define DAMAGE_FREEZE        0.25f
#define DASH_SPEED           20.0f
#define DASH_TIME            0.25f
#define DASH_COOLDOWN        0.6f
// HeroBox: the Knight's damage hurtbox from the game (0.455 x 1.170, offset -0.694), a bit smaller than his collider
#define NAIL_DAMAGE          charm_nail_damage()
#define SOUL_PER_HIT         11
#define SOUL_MAX             99
#define FOCUS_COST           33
#define FOCUS_TIME           0.891f
#define COYOTE_TIME          0.1f


typedef struct {
    bool alive, active, dying;
    int type;                   // AI: 0 walker, 1 flyer, 2 walker that charges the Knight
    int aWalk, aIdle, aChase, aTurn, aDAir, aDLand, aAttack;   // anim indices in R.enemyset
    float speed;
    char lib[24];
    int bstate; float bt; int phase, armor, headDmg; bool bossActive, slamDone;   // boss state
    float x, y, vx, vy;         // x,y = pivot
    int facing;
    int hp, geo;
    float hitFlash, stun, startle;
    bool alert, onGround;
    float hw, hh;               // collider half size
    float pox, poy;             // sprite pivot relative to collider centre
    AnimPlayer anim;
    float deathTimer;
    int ent;                    // room ent index
    float turnCool;             // walkers: minimum time between turns (stops jitter)
    bool grounded;              // walkers: on the ground last frame
    int lastSwing;
} Enemy;

typedef struct { bool alive; float x, y, vx, vy, t; int size; } Geo;
static void boss_hit(Enemy *en);
static void shade_hit(void);

typedef struct {
    bool alive;                 // breakables
    int hp;
    float flash, t;
    float dy, vy;               // falling (stalactites / collapse)
    int state;
    int lastSwing;
} EntState;

#define MAX_ENEMIES 32
#define MAX_GEO 64
#define MAX_ENTS 1024

static float freezeT;           // GameManager.FreezeMoment: everything but the hero waits
static float fixedAcc;          // 50 Hz physics accumulator
static float KX, KY;            // interpolated Knight position for drawing
static u32 keysDownCache;
static bool nearDoor;
static Room R;
static bool roomLoaded;
static Enemy E[MAX_ENEMIES];
static Geo G[MAX_GEO];
static EntState ES[MAX_ENTS];
static float camx, camy;
static int focusSfx = -1;        // the looping focus charge sound, stopped when Focus ends
static bool g_noFront;           // host camera tour: draw without the foreground layers
static void snap_camera(void);
static float fade = 1, fadeTarget = 0;
static char pendingRoom[32], pendingGate[32];
static bool transitioning;
static float areaTitleT = 0; static char areaTitle[32];
static float hazardX, hazardY;
static bool paused; static int pauseSel, pausePage; static float pauseT, pausePtrT;   // pause menu: 0 main, 1 options
static int blueHealth; static float hiveT;
GameInput gin;
static SpriteSet icons;
const SpriteSet *game_icons(void) { return icons.loaded ? &icons : NULL; }
// AudioManager.ApplyMusicCue with the cue's asset guid (romfs/text/music_cues.txt, tools/data/audio_map.json)
void game_music_cue(const char *guid, float delay, float transition) {
    static char (*keys)[33], (*vals)[32]; static int n = -1;
    (void)delay;
    if (n < 0) {
        n = 0; keys = calloc(128, 33); vals = calloc(128, 32);
        FILE *f = fopen("romfs:/text/music_cues.txt", "rb"); char ln[128];
        if (f) { while (n < 128 && fgets(ln, sizeof ln, f)) if (sscanf(ln, "%32s %31s", keys[n], vals[n]) == 2) n++; fclose(f); }
    }
    for (int i = 0; i < n; i++) if (!strcmp(keys[i], guid)) {
        if (!strcmp(vals[i], "none")) audio_stop_stream(0, transition > 0 ? transition : 1.0f);
        else audio_play_stream(0, vals[i], true, transition > 0 ? transition : 0.5f);
        return;
    }
}
const char *game_map_zone(void) { return roomLoaded ? R.title : ""; }
float game_room_w(void) { return roomLoaded ? (float)R.w : 0; }
float game_room_h(void) { return roomLoaded ? (float)R.h : 0; }
static bool hudOut; static float hudAlpha = 1;
static float stagTravelT;          // LoadLevel("Cinematic_Stag_travel"): the video isn't in the data, so a black wait
static float shakeT;
static char visited[16][32]; static int nvisited;
static float msgT; static char msg[96];
static char benchRoom[32]; static float benchX, benchY; static bool hasBench;
static bool loadAtBench;
static bool fkDefeated;
static char areaMusic[32];
static char curGate[32];
// the Shade: left where the Knight died, holds the lost geo
static bool hasShade; static char shadeRoom[32]; static float shadeX, shadeY; static int shadeGeo;
typedef struct { bool active, dying, alert; float x, y, vx, vy, t, flash, atk; int hp, facing, lastSwing; AnimPlayer anim; } Shade;
static Shade SH;
static float dbgDt[60]; static int dbgDtI;
static bool dbgBoxes;           // draw collision/hitboxes (host HK_BOXES, or 'boxes' in the SD test file)

/* ---------------- assets ---------------- */
static void preload_sounds(void) {     // the Knight's and the UI's sounds, so the first play isn't held up by a file read
    static const char *names[] = { "sword_1", "sword_2", "sword_3", "sword_4", "sword_5", "hero_damage", "hero_dash", "hero_jump",
        "hero_land_hard", "hero_land_soft", "hero_run_footsteps_stone", "hero_wall_jump", "hero_double_jump", "focus_health_charging",
        "focus_health_heal", "enemy_damage", "enemy_death_sword", "sword_tink", "geo_small_collect_1", "geo_hit_ground_1", "button",
        "begin_button", "bench_rest", "hero_death_v2", "ui_button_confirm", "ui_change_selection", "hero_walk_footsteps_stone", "geo_deplete_count_down", "geo_rock_hit_1", "breakable_wall_hit_1", "breakable_wall_death", "chest_open" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) sfx_preload(names[i]);
}
bool assets_load(void) {
    preload_sounds();
    bool ok = true;
    ok &= spr_load(&A.knight, "romfs:/gfx/knight.spr");
    ok &= spr_load(&A.enemies, "romfs:/gfx/enemies.spr");
    ok &= spr_load(&A.hud, "romfs:/gfx/hud.spr");
    ok &= spr_load(&A.ui, "romfs:/gfx/ui.spr");
    ok &= font_load(&A.fontMenu, "romfs:/gfx/font_menu.spr");
    ok &= font_load(&A.fontBig, "romfs:/gfx/font_big.spr");
    ok &= font_load(&A.fontText, "romfs:/gfx/font_text.spr");
    dlg_init();
    menus_init();
    spr_load(&icons, "romfs:/gfx/icons.spr");
    return ok;
}

/* ---------------- collision ---------------- */
static bool ent_is_solid(int i) {
    RoomEnt *e = &R.ents[i];
    switch (e->kind) {
        case EK_PLATFORM: case EK_SOLID: return true;
        case EK_BREAKABLE_WALL: case EK_COLLAPSE_FLOOR: return ES[i].alive;
        case EK_VINE: return true;          // hanging or dropped, it is always a platform (ES.dy moves it)
        default: return false;
    }
}

static bool box_overlap(float ax0, float ay0, float ax1, float ay1, float bx0, float by0, float bx1, float by1) {
    return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

// While objects update (enemies, projectiles, debris), a wall stands just outside the room's left and right edges: the
// room's collision ends there, and past a doorway its floor and ceiling rows run on forever, so a Gruzzer that flew into
// a doorway bounced off into the void. The Knight's own movement doesn't see it (doorways stay open for transitions).
static bool edgeWalls;
void w_edge_walls(bool on) { edgeWalls = on; }
static short entCand[MAX_ENTS]; static int nEntCand = -1;
static bool solid_box(float x0, float y0, float x1, float y1) {
    if (edgeWalls) { float W = (float)R.w; if ((x0 < -0.25f && x1 > -1.25f) || (x1 > W + 0.25f && x0 < W + 1.25f)) return true; }
    float rs = (float)R.res;
    int tx0 = (int)floorf(x0 * rs), tx1 = (int)floorf(x1 * rs - 1e-4f);
    int ty0 = (int)floorf(y0 * rs), ty1 = (int)floorf(y1 * rs - 1e-4f);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++)
            if (room_solid_cell(&R, tx, ty)) return true;
    if (uo_terrain_hit(x0, y0, x1, y1)) return true;
    if (nEntCand >= 0) {     // inside a raycast: only the solid entities its bounds touch
        for (int k = 0; k < nEntCand; k++) { int i = entCand[k]; RoomEnt *e = &R.ents[i];
            if (box_overlap(x0, y0, x1, y1, e->x - e->w / 2, e->y - e->h / 2 + ES[i].dy, e->x + e->w / 2, e->y + e->h / 2 + ES[i].dy)) return true; }
        return false;
    }
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        if (!ent_is_solid(i)) continue;
        RoomEnt *e = &R.ents[i];
        if (box_overlap(x0, y0, x1, y1, e->x - e->w / 2, e->y - e->h / 2 + ES[i].dy, e->x + e->w / 2, e->y + e->h / 2 + ES[i].dy)) return true;
    }
    return false;
}

// move a box (centre cx,cy half hw,hh) by dx,dy; returns flags 1=hit x, 2=hit floor, 4=hit ceiling.
// stepUp lets grounded walkers climb small steps/slopes (the collision grid is 1/4 unit).
static int move_box_ex(float *cx, float *cy, float hw, float hh, float dx, float dy, float stepUp) {
    int flags = 0;
    const float STEP = 0.05f;
    float rem = dx;
    while (fabsf(rem) > 1e-6f) {
        float s = fabsf(rem) > STEP ? copysignf(STEP, rem) : rem;
        if (solid_box(*cx + s - hw, *cy - hh, *cx + s + hw, *cy + hh)) {
            bool stepped = false;
            if (stepUp > 0) {
                for (float up = 0.0625f; up <= stepUp + 1e-4f; up += 0.0625f)
                    if (!solid_box(*cx + s - hw, *cy - hh + up, *cx + s + hw, *cy + hh + up)) { *cy += up; stepped = true; break; }
            }
            if (!stepped) {
                // creep up to the wall, as the vertical move does: stopping a whole step short left the Knight up to 0.05
                // from it, past the 0.03 his contacts reach, so about one approach in three never touched the wall and he
                // didn't cling. Like a Unity body it stops about 0.01 short (its contact offset): flush, the Knight's
                // downward ground rays at his edge start on the wall and he never counts as airborne
                float t = s, k0 = s > 0 ? 0 : -0.01f, k1 = s > 0 ? 0.01f : 0;
                for (int k = 0; k < 6; k++) { t *= 0.5f; if (!solid_box(*cx + t - hw + k0, *cy - hh, *cx + t + hw + k1, *cy + hh)) *cx += t; }
                flags |= 1; break; }
        }
        *cx += s; rem -= s;
    }
    rem = dy;
    while (fabsf(rem) > 1e-6f) {
        float s = fabsf(rem) > STEP ? copysignf(STEP, rem) : rem;
        if (solid_box(*cx - hw, *cy + s - hh, *cx + hw, *cy + s + hh)) {
            // creep up to the contact point in small steps
            float t = s;
            for (int k = 0; k < 6; k++) { t *= 0.5f; if (!solid_box(*cx - hw, *cy + t - hh, *cx + hw, *cy + t + hh)) { *cy += t; } }
            flags |= (s < 0) ? 2 : 4;
            break;
        }
        *cy += s; rem -= s;
    }
    return flags;
}

static int move_box(float *cx, float *cy, float hw, float hh, float dx, float dy) { return move_box_ex(cx, cy, hw, hh, dx, dy, 0); }

static bool on_ground_box(float cx, float cy, float hw, float hh) {
    return solid_box(cx - hw + 0.02f, cy - hh - 0.06f, cx + hw - 0.02f, cy - hh);
}

// Push a box out of terrain to the nearest free spot (enemy colliders in the game often sit slightly inside the floor
// or against walls). Tries up first, then sideways, then down, in 1/16-unit steps.
static bool unstick_box(float *cx, float *cy, float hw, float hh, float maxd) {
    if (!solid_box(*cx - hw, *cy - hh, *cx + hw, *cy + hh)) return true;
    for (float d = 0.0625f; d <= maxd + 1e-4f; d += 0.0625f) {
        if (!solid_box(*cx - hw, *cy - hh + d, *cx + hw, *cy + hh + d)) { *cy += d; return true; }
        if (!solid_box(*cx - hw + d, *cy - hh, *cx + hw + d, *cy + hh)) { *cx += d; return true; }
        if (!solid_box(*cx - hw - d, *cy - hh, *cx + hw - d, *cy + hh)) { *cx -= d; return true; }
        if (!solid_box(*cx - hw, *cy - hh - d, *cx + hw, *cy + hh - d)) { *cy -= d; return true; }
    }
    return false;
}

/* ---------------- world API (world.h) ---------------- */
bool w_solid_box(float x0, float y0, float x1, float y1) { return solid_box(x0, y0, x1, y1); }
#ifdef __HOST__
// tests: HK_SOLIDPROBE="x0,y0,x1,y1" names what's solid in that box, every 60 frames
void game_solid_probe(void) {
    {   // HK_LISTGATES: the room's gates once per room (name, box, target)
        static char listed[32];
        if (getenv("HK_LISTGATES") && strcmp(listed, R.id)) { snprintf(listed, sizeof listed, "%s", R.id);
            for (int k = 0; k < R.nents; k++) { const RoomEnt *q = &R.ents[k]; if (q->kind != EK_GATE) continue;
                fprintf(stderr, "[gate] %s %s at (%.2f,%.2f) size %.2fx%.2f -> %s\n", R.id, q->name, q->x, q->y, q->w, q->h, q->target); } }
    }
    const char *e = getenv("HK_SOLIDPROBE"); static int fr; if (!e || ++fr % 60) return;
    float x0, y0, x1, y1; if (sscanf(e, "%f,%f,%f,%f", &x0, &y0, &x1, &y1) != 4) return;
    float rs = (float)R.res; int cells = 0;
    for (int ty = (int)floorf(y0 * rs); ty <= (int)floorf(y1 * rs - 1e-4f); ty++)
        for (int tx = (int)floorf(x0 * rs); tx <= (int)floorf(x1 * rs - 1e-4f); tx++) if (room_solid_cell(&R, tx, ty)) cells++;
    fprintf(stderr, "[probe] %s (%.2f,%.2f..%.2f,%.2f): %d solid grid cells\n", R.id, x0, y0, x1, y1, cells);
    extern void uo_terrain_report(float, float, float, float); uo_terrain_report(x0, y0, x1, y1);
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) { if (!ent_is_solid(i)) continue; RoomEnt *en = &R.ents[i];
        if (box_overlap(x0, y0, x1, y1, en->x - en->w / 2, en->y - en->h / 2 + ES[i].dy, en->x + en->w / 2, en->y + en->h / 2 + ES[i].dy))
            fprintf(stderr, "[probe] entity %d at (%.2f,%.2f) size %.2fx%.2f\n", i, en->x, en->y, en->w, en->h); }
}
#endif
int w_move_box(float *cx, float *cy, float hw, float hh, float dx, float dy, float stepUp) { return move_box_ex(cx, cy, hw, hh, dx, dy, stepUp); }
bool w_unstick_box(float *cx, float *cy, float hw, float hh, float maxd) { return unstick_box(cx, cy, hw, hh, maxd); }
float w_room_w(void) { return (float)R.w; }
float w_room_h(void) { return (float)R.h; }
static bool solid_point(float x, float y) { return solid_box(x - 0.001f, y - 0.001f, x + 0.001f, y + 0.001f); }
bool w_raycast(float x, float y, float dx, float dy, float dist, float *hitDist) {
    float l = sqrtf(dx * dx + dy * dy); if (l <= 0) return false;
    dx /= l; dy /= l;
    // every point tested lies on the segment, so only the terrain objects and solid entities its bounds touch can be hit
    float ex = x + dx * fmaxf(dist, 0), ey = y + dy * fmaxf(dist, 0);
    float bx0 = fminf(x, ex) - 0.01f, by0 = fminf(y, ey) - 0.01f, bx1 = fmaxf(x, ex) + 0.01f, by1 = fmaxf(y, ey) + 0.01f;
    uo_terrain_narrow(true, bx0, by0, bx1, by1);
    nEntCand = 0;
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        if (!ent_is_solid(i)) continue;
        RoomEnt *e = &R.ents[i];
        if (box_overlap(bx0, by0, bx1, by1, e->x - e->w / 2 - 0.5f, e->y - e->h / 2 + ES[i].dy - 0.5f, e->x + e->w / 2 + 0.5f, e->y + e->h / 2 + ES[i].dy + 0.5f)) entCand[nEntCand++] = (short)i;
    }
    bool hit = false;
    const float step = 0.02f;
    for (float t = 0; t <= dist + 1e-5f; t += step) {
        float tt = t > dist ? dist : t;
        if (solid_point(x + dx * tt, y + dy * tt)) {
            // refine
            float lo = tt - step < 0 ? 0 : tt - step, hi = tt;
            for (int k = 0; k < 6; k++) { float m = (lo + hi) / 2; if (solid_point(x + dx * m, y + dy * m)) hi = m; else lo = m; }
            *hitDist = hi; hit = true; break;
        }
    }
    uo_terrain_narrow(false, 0, 0, 0, 0); nEntCand = -1;
    return hit;
}

/* ---------------- helpers ---------------- */
static float world_to_sx(float x) { return (x - camx) * PPU + TOP_W / 2; }
static float world_to_sy(float y) { return (camy - y) * PPU + TOP_H / 2; }

static void spawn_geo(float x, float y, int n) {
    for (int k = 0; k < n; k++)
        for (int i = 0; i < MAX_GEO; i++) if (!G[i].alive) {
            G[i] = (Geo){ true, x, y, ((rand() % 200) - 100) / 25.0f, 6 + (rand() % 100) / 20.0f, 0, 0 };
            break;
        }
}

double g_uoProf;
/* hooks for the object runtime (uo.c / comps.c) */
void game_spawn_geo(float x, float y, int n) { spawn_geo(x, y, n); }
void uo_hero_box(float *x0, float *y0, float *x1, float *y1) { *x0 = P.x - HALF_W; *x1 = P.x + HALF_W; *y0 = P.y + COL_OFF_Y - HALF_H; *y1 = P.y + COL_OFF_Y + HALF_H; }
void uo_hero_hurtbox(float *x0, float *y0, float *x1, float *y1) { *x0 = P.x - HURT_HW; *x1 = P.x + HURT_HW; *y0 = P.y + HURT_OY - HURT_HH; *y1 = P.y + HURT_OY + HURT_HH; }
float uo_hero_x(void) { return P.x; }
float uo_hero_y(void) { return P.y; }
bool uo_hero_slash_box(float *x0, float *y0, float *x1, float *y1) { if (!hero_slash_active()) return false; hero_nail_box(x0, y0, x1, y1); return true; }

static void play_knight(const char *name, bool restart) { anim_play(&P.anim, &A.knight, name, restart); }

static void set_message(const char *m) { snprintf(msg, sizeof msg, "%s", TR(m)); msgT = 3.0f; }     // in the chosen language

/* ---------------- save profiles (the game's four save slots) ---------------- */
int g_saveSlot = 1;
static float playTime;          // seconds played on this profile (shown on the profile screen)
static void slot_path(int slot, char *buf, int n) { snprintf(buf, n, "sdmc:/3ds/hk3ds/save%d.dat", slot); }
// saves from before profiles existed (save.dat) become profile 1
void game_migrate_saves(void) {
    FILE *f = fopen("sdmc:/3ds/hk3ds/save1.dat", "rb"); if (f) { fclose(f); return; }
    f = fopen("sdmc:/3ds/hk3ds/save.dat", "rb"); if (!f) return; fclose(f);
    rename("sdmc:/3ds/hk3ds/save.dat", "sdmc:/3ds/hk3ds/save1.dat");
}
void game_set_slot(int slot) { g_saveSlot = slot < 1 ? 1 : slot > 4 ? 4 : slot; }
void game_clear_slot(int slot) { char p[64]; slot_path(slot, p, sizeof p); remove(p); }
// what the profile screen shows: the area, masks, geo and play time
static const char *zone_of_room(const char *room) {
    static const struct { const char *pre, *name; } Z[] = { {"Tutorial","King's Pass"}, {"Town","Dirtmouth"}, {"Crossroads","Forgotten Crossroads"},
        {"Fungus1","Greenpath"}, {"Fungus2","Fungal Wastes"}, {"Fungus3","Fog Canyon"}, {"Ruins","City of Tears"}, {"Waterways","Royal Waterways"},
        {"Mines","Crystal Peak"}, {"RestingGrounds","Resting Grounds"}, {"Deepnest","Deepnest"}, {"Abyss","Ancient Basin"}, {"Cliffs","Howling Cliffs"},
        {"Hive","The Hive"}, {"White_Palace","White Palace"}, {"GG","Godhome"}, {"Grimm","Dirtmouth"}, {"Room_Town","Dirtmouth"}, {"Room_shop","Dirtmouth"}, {NULL,NULL} };
    for (int i = 0; Z[i].pre; i++) if (!strncmp(room, Z[i].pre, strlen(Z[i].pre))) return Z[i].name;
    return "Hallownest";
}
bool game_slot_info(int slot, char *area, int an, int *masks, int *geo, int *seconds) {
    char p[64]; slot_path(slot, p, sizeof p);
    FILE *f = fopen(p, "rb"); if (!f) return false;
    char line[128], room[32] = "", zone[64] = "", br[32] = ""; int mh = 5, g = 0, d = 0, hb = 0; float bx, by;
    *seconds = 0;
    if (fgets(line, sizeof line, f) && !strncmp(line, "HK3DS", 5)) {
        if (fgets(room, sizeof room, f)) room[strcspn(room, "\r\n")] = 0;
        if (fgets(line, sizeof line, f)) {}
        if (fgets(line, sizeof line, f)) sscanf(line, "%d %d %d", &mh, &g, &d);
        if (fgets(line, sizeof line, f)) sscanf(line, "%31s %f %f %d", br, &bx, &by, &hb);
        while (fgets(line, sizeof line, f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "PT ", 3)) *seconds = atoi(line + 3);
            else if (!strncmp(line, "Z ", 2)) snprintf(zone, sizeof zone, "%s", line + 2);
            else if (!strncmp(line, "I maxHealthBase ", 16)) mh = atoi(line + 16);
        }
    }
    fclose(f);
    snprintf(area, an, "%s", zone[0] ? zone : zone_of_room(hb && br[0] && strcmp(br, "-") ? br : room));
    *masks = mh; *geo = g;
    return true;
}

static void save_game(const char *room, const char *gate) {
    ach_check_all();
    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/hk3ds", 0777);
    char sp[64]; slot_path(g_saveSlot, sp, sizeof sp);
    FILE *f = fopen(sp, "wb");
    if (!f) return;
    fprintf(f, "HK3DS2\n%s\n%s\n%d %d %d\n%s %.2f %.2f %d\n", room, gate, P.maxHealth, P.geo, P.hasDash,
            hasBench ? benchRoom : "-", benchX, benchY, hasBench);
    fprintf(f, "F %d\n", fkDefeated ? 1 : 0);
    fprintf(f, "PT %d\n", (int)playTime);
    {   // the area in English whatever the language: it also names the profile's area art (shown translated)
      const char *zn = roomLoaded ? lang_get_en(R.title, "Map Zones") : NULL; char zb[64]; snprintf(zb, sizeof zb, "%s", zn ? zn : zone_of_room(room));
      for (char *q = zb; *q; q++) { if (*q == '\n') *q = ' '; else if (q[0] == '\\' && q[1] == 'n') { q[0] = ' '; memmove(q + 1, q + 2, strlen(q + 2) + 1); } }
      fprintf(f, "Z %s\n", zb); }
    if (hasShade) fprintf(f, "S %s %.2f %.2f %d\n", shadeRoom, shadeX, shadeY, shadeGeo);
    pd_save(f);
    for (int i = 0; i < nvisited; i++) fprintf(f, "%s\n", visited[i]);
    fclose(f);
}

static bool was_visited(const char *id) {
    for (int i = 0; i < nvisited; i++) if (!strcmp(visited[i], id)) return true;
    return false;
}

const char *game_entry_gate(void) { return curGate; }
void comps_save_persistent(void);
void game_save_now(void) {
    if (!roomLoaded) return;
    // benches run by their own Bench Control script (Ancestral Mound, White Palace...) set the respawn through PlayerData
    // (atBench, respawnScene) and then call GameManager.SaveGame: make that bench the respawn point
    if (pd_bool("atBench") && !strcmp(pd_str("respawnScene"), R.id)) { hasBench = true; strncpy(benchRoom, R.id, 31); benchX = P.x; benchY = P.y; }
    comps_save_persistent(); save_game(R.id, curGate);
}

/* ---------------- room setup ---------------- */
static void setup_entities(void) {
    memset(E, 0, sizeof E); memset(G, 0, sizeof G); memset(ES, 0, sizeof ES);
    int ne = 0;
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        ES[i].alive = true; ES[i].hp = e->hp;
        if (e->kind == EK_COLLAPSE_FLOOR && !strcmp(e->name, "Boss Floor") && fkDefeated) ES[i].alive = false;
        if (e->kind == EK_ENEMY && ne < MAX_ENEMIES) {
            Enemy *en = &E[ne++];
            memset(en, 0, sizeof *en);
            en->alive = true; en->active = true; en->ent = i;
            en->type = e->ai;
            en->hp = e->hp > 0 ? e->hp : 8; en->geo = e->geo;
            en->facing = -1;
            en->hw = e->w / 2; en->hh = e->h / 2;
            en->x = e->x; en->y = e->y;
            sscanf(e->target, "%f %f", &en->pox, &en->poy);
            en->anim.anim = -1; en->anim.set = &R.enemyset;
            // roles: "lib|walk|idle|chase|turn|deathAir|deathLand|attack"
            char lib[32] = "", roles[7][32]; memset(roles, 0, sizeof roles);
            { char tmp[96]; strncpy(tmp, e->extra, 95); tmp[95] = 0; char *sv; char *t = strtok_r(tmp, "|", &sv);
              if (t) strncpy(lib, t, 31);
              for (int k = 0; k < 7; k++) { t = strtok_r(NULL, "|", &sv); if (t) strncpy(roles[k], t, 31); } }
            int *slots[7] = { &en->aWalk, &en->aIdle, &en->aChase, &en->aTurn, &en->aDAir, &en->aDLand, &en->aAttack };
            strncpy(en->lib, lib, 23);
            if (en->type == 4) {
                if (fkDefeated) { en->alive = false; continue; }
                en->armor = 65; en->phase = 0; en->bstate = 0; en->bossActive = false;
            }
            for (int k = 0; k < 7; k++) {
                char nm[72]; snprintf(nm, sizeof nm, "%s:%s", lib, roles[k]); nm[31] = 0;
                *slots[k] = roles[k][0] ? spr_anim(&R.enemyset, nm) : -1;
            }
            en->speed = en->type == 2 ? 2.0f : 2.0f;
            anim_play_idx(&en->anim, (en->type == 1 || en->type == 5) && en->aIdle >= 0 ? en->aIdle : en->aWalk, true);
            if (en->type != 4) {
                unstick_box(&en->x, &en->y, en->hw, en->hh, 3.0f);
                if (en->type == 0 || en->type == 2 || en->type == 3) {     // settle onto the floor below
                    float cx = en->x, cy = en->y;
                    move_box(&cx, &cy, en->hw, en->hh, 0, -2.0f);
                    if (on_ground_box(cx, cy, en->hw, en->hh)) { en->x = cx; en->y = cy; en->grounded = true; }
                }
            }
        }
    }
}

// Exit triggers often sit just outside the room (x < 0, x > width, y > height). The room edge is solid, so the
// Knight could press against it without ever overlapping the trigger; extend such triggers 0.6 units inward.
static void gate_box(const RoomEnt *e, float *x0, float *y0, float *x1, float *y1) {
    *x0 = e->x - e->w / 2; *x1 = e->x + e->w / 2; *y0 = e->y - e->h / 2; *y1 = e->y + e->h / 2;
    if (*x1 <= 0.05f) *x1 = 0.6f;
    if (*x0 >= R.w - 0.05f) *x0 = R.w - 0.6f;
    if (*y0 >= R.h - 0.05f) *y0 = R.h - 0.6f;
    if (*y1 <= 0.05f) *y1 = 0.3f;
}

static RoomEnt *find_gate(const char *name) {
    for (int i = 0; i < R.nents; i++) if (R.ents[i].kind == EK_GATE && !strcmp(R.ents[i].name, name)) return &R.ents[i];
    // fall back to another gate on the same side (top1 -> top2 ...)
    int n = 0; while (name[n] && (name[n] < '0' || name[n] > '9')) n++;
    for (int i = 0; i < R.nents; i++) if (R.ents[i].kind == EK_GATE && n > 0 && !strncmp(R.ents[i].name, name, n)) return &R.ents[i];
    return NULL;
}

static GatePos gate_pos_of(const char *n) {   // TransitionPoint.GetGatePosition
    if (strstr(n, "top")) return GP_TOP;
    if (strstr(n, "right")) return GP_RIGHT;
    if (strstr(n, "left")) return GP_LEFT;
    if (strstr(n, "bot")) return GP_BOTTOM;
    return GP_DOOR;                                  // "door..." and the few other named doorways
}

static void place_at_gate(const char *gate) {
    RoomEnt *g = gate && gate[0] ? find_gate(gate) : NULL;
    if (!g && strcmp(R.id, "Tutorial_01")) {     // no entry gate: use the room's first doorway instead of a raw spawn point
        const char *pref[] = { "left", "right", "top", "bot" };
        for (int k = 0; k < 4 && !g; k++)
            for (int i = 0; i < R.nents; i++) if (R.ents[i].kind == EK_GATE && !strncmp(R.ents[i].name, pref[k], strlen(pref[k]))) { g = &R.ents[i]; gate = g->name; break; }
    }
    if (!g) { hero_set_position(R.spawnx, R.spawny); P.vx = P.vy = 0; P.acceptingInput = true; return; }
    float ox = 0, oy = 0; int fl = 0;
    const char *o = strstr(g->extra, " O ");
    if (o) sscanf(o, " O %f %f F %d", &ox, &oy, &fl);
    hero_enter_scene(gate_pos_of(g->name), g->x, g->y, ox, oy, fl);
}

static void enter_room(const char *id, const char *gate) {
    hk_log("enter %s (gate %s)", id, gate ? gate : "");
    lang_gc();          // text of a language switched away from in the pause menu: nothing shows it any more
#ifdef __HOST__
    fprintf(stderr, "enter_room(%s, %s)\n", id, gate ? gate : "");
#endif
    // free the old room's objects and their sprites before loading the new room's art: holding both at once could run
    // the GPU memory out (an Old 3DS, or a big boss room), and the new room's sprites then went missing
    // the arrival gate first: scene objects ask for it while they load (Watcher Knights' boss scene only loads when the
    // Knight drops in through the top door; it was compared with the previous room's arrival gate)
    strncpy(curGate, gate ? gate : "", 31);
    uo_unload();
    hk_log("  unloaded objects");
    if (roomLoaded) room_free(&R);
    room_tilecache_reset();
    hk_log("  freed room");
    uo_preload_sprites(id);
    hk_log("  preloaded sprites");
    roomLoaded = room_load(&R, id);
    setup_entities();
    dlg_reset();
    menus_reset();
    msgs_reset();
    shop_reset();
    P.controlRelinquished = false; P.animCtrlStopped = false; P.forceFootstep = 0;   // a new scene: the HeroController takes over again
    hk_log("room %s: art loaded", id);
    // converted scene objects (enemies running their own FSMs); they replace the stand-in enemies
    particles_load(id, NULL);           // before the objects: their particle systems start as they're enabled
    bool uok = uo_load_room(id);
    hk_log("room %s: objects %s (%d)", id, uok ? "ok" : "none", uo_count());
    if (uok) {
        for (int i = 0; i < MAX_ENEMIES; i++) if (E[i].alive && E[i].type != 4) E[i].alive = false;
        // the room's objects draw themselves now: a baked entity image of the same object would be drawn on top of it
        // and stay when the object breaks or leaves (breakable poles, floors...). Drop those images, and free the
        // image sets nothing uses any more (GPU memory: up to several MB per room)
        int kept = 0;
        for (int i = 0; i < R.nents; i++) { if (R.ents[i].image >= 0 && uo_find(R.ents[i].name)) R.ents[i].image = -1; if (R.ents[i].image >= 0) kept++; }
        bool natives = false; for (int i = 0; i < MAX_ENEMIES; i++) if (E[i].alive) natives = true;
        room_load_entity_art(&R, kept > 0, natives);
        hk_log("room %s: entity images kept %d", id, kept);
    } else if (roomLoaded) room_load_entity_art(&R, true, true);     // a room without objects: its baked entities and enemies
#ifdef __HOST__
    if (getenv("HK_SOLIDMAP")) { float a, b, c, d; if (sscanf(getenv("HK_SOLIDMAP"), "%f %f %f %f", &a, &b, &c, &d) == 4)
        for (float y = d; y >= b; y -= 0.5f) { fprintf(stderr, "%5.1f ", y); for (float x = a; x < c; x += 0.5f) fputc(room_solid_cell(&R, (int)floorf(x * R.res), (int)floorf(y * R.res)) ? '#' : solid_box(x, y, x + 0.25f, y + 0.25f) ? 'o' : '.', stderr); fputc('\n', stderr); } }
#endif
    {   // SceneManager.Start: the Knight's vignette (Darkness Control) gets the scene's darkness level
        extern void uo_set_darkness(int level); uo_set_darkness(game_darkness_level());
    }
    memset(&SH, 0, sizeof SH); SH.anim.anim = -1;
    if (hasShade && !strcmp(shadeRoom, id)) {
        SH.active = true; SH.x = shadeX; SH.y = shadeY; SH.hp = 15; SH.facing = -1;
        anim_play(&SH.anim, &A.enemies, "Shade Idle", true);
    }
    place_at_gate(gate);
    // gates the Knight arrives inside stay disarmed until he walks out of them
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        if (e->kind == EK_GATE) { float a, b, c, d; gate_box(e, &a, &b, &c, &d); if (box_overlap(P.x - 1.0f, P.y - 3.0f, P.x + 1.0f, P.y + 1.0f, a, b, c, d)) ES[i].state = 1; }
    }
    hazardX = P.x; hazardY = P.y;
    for (int i = 0; i < R.nents; i++)      // the gate's own HazardRespawnMarker (safer than a mid-launch arrival spot)
        if (R.ents[i].kind == EK_GATE && gate && !strcmp(R.ents[i].name, gate)) { float hx, hy; if (sscanf(R.ents[i].extra, "R %f %f", &hx, &hy) == 2) { hazardX = hx; hazardY = hy; } }
    KX = P.x; KY = P.y;
    cam_load(id, (float)R.w, (float)R.h);
    masks_load(id);
    snap_camera();
    // SceneManager music / atmos cues: "-" keeps what is playing, "none" fades to silence
    if (R.music[0] && strcmp(R.music, "-")) { if (!strcmp(R.music, "none")) audio_stop_stream(0, 1.5f); else audio_play_stream(0, R.music, true, 2.0f); }
    if (R.ambience[0] && strcmp(R.ambience, "-")) { if (!strcmp(R.ambience, "none")) audio_stop_stream(1, 1.0f); else audio_play_stream(1, R.ambience, true, 2.0f); }
    area_titles_enter(id, gate);
    map_enter_scene(id);
    save_game(id, gate ? gate : "");
}

// a gate whose object the game hides by PlayerData (DeactivateIfPlayerdataFalse / True: "D bool" / "T bool" in its
// extra field), e.g. the Grimm Troupe tents in Dirtmouth, only works while its object would be active
bool gate_enabled(const RoomEnt *e) {
    for (const char *p = e->extra; (p = strchr(p, ' ')) != NULL; p++) {
        if ((p[1] == 'D' || p[1] == 'T') && p[2] == ' ') {
            char b[48] = { 0 }; sscanf(p + 3, "%47s", b);
            if (b[0] && pd_bool(b) != (p[1] == 'D')) return false;
        }
    }
    return true;
}

/* ---------------- lifecycle ---------------- */
static void reset_player(void) { hero_reset(); freezeT = 0; fixedAcc = 0; }

bool game_any_key_down(void);     // AnyKey (the PlayMaker action)
static bool g_testAuto;    // test_start.txt "autofight": the test bot plays (emulator runs of boss fights)
static int g_testWalk;     // test_start.txt "walk:R" / "walk:L": the bot also travels that way, fighting what it meets (videos)
bool g_testHeroLog;        // test_start.txt "herolog": the Knight's state in the log twice a second (emulator tests)
// test starts (HK_START / test_start.txt): put the Knight somewhere directly, skipping the entry walk
static void test_place(float x, float y) {
    hero_set_position(x, y); camx = x; camy = y;
    KX = x; KY = y; snap_camera();
    P.enterPhase = 0; P.c.transitioning = false; P.trans = TS_WAITING_TO_TRANSITION; P.acceptingInput = true;
    P.gravityScale = 0.79f; P.state = AS_AIRBORNE; P.vx = P.vy = 0; KX = x; KY = y;
}
static void sit_at_bench(void) {
    {   // a respawn point outside the room or with no floor under it (saves made while a script had moved the Knight
        // off-screen, e.g. the v1.0.4 Vengeful Spirit bug) would drop him out of the world: use the room's own entry point
        float h; bool ok = benchX > 0.5f && benchX < R.w - 0.5f && benchY > 0.5f && benchY < R.h - 0.5f &&
                           w_raycast(benchX, benchY + 0.5f, 0, -1, 6.0f, &h) && !solid_box(benchX - 0.2f, benchY, benchX + 0.2f, benchY + 1.0f);
        if (!ok) {
            benchX = hazardX; benchY = hazardY;
            for (int i = 0; i < R.nents; i++) { float hx, hy;     // the first exit's own respawn marker is inside the room
                if (R.ents[i].kind == EK_GATE && sscanf(R.ents[i].extra, "R %f %f", &hx, &hy) == 2 && hx > 0 && hx < R.w && hy > 0 && hy < R.h) { benchX = hx; benchY = hy; break; } }
            hk_log("bench respawn outside the room, using %.1f %.1f", benchX, benchY); }
    }
    test_place(benchX, benchY + 0.2f); hero_relinquish_control(); P.c.onGround = true; P.onGround = true;
    P.sitting = true; play_knight("Sit Idle", true);
}

void game_new(void) {
    reset_player(); pd_reset();
    hasBench = false; fkDefeated = false; hasShade = false; playTime = 0;
    nvisited = 0;
    fade = 1; fadeTarget = 0; paused = false; transitioning = false;
    {   // a test start (test_start.txt, below) goes straight to its room rather than loading King's Pass first
        char first[32] = "Tutorial_01"; FILE *tf = fopen("sdmc:/3ds/hk3ds/test_start.txt", "rb");
        if (tf) { char rm[32]; if (fscanf(tf, "%31s", rm) == 1 && rm[0] != '-') snprintf(first, sizeof first, "%s", rm); fclose(tf); }
        enter_room(first, "");
    }
    P.vy = -5;   // the Knight drops into King's Pass from above
#ifdef __HOST__
    if (getenv("HK_GEO")) P.geo = atoi(getenv("HK_GEO"));
    if (getenv("HK_SOUL")) P.soul = atoi(getenv("HK_SOUL"));
    if (getenv("HK_PD")) {     // "boolName,intName=3,..." test PlayerData
        char t[512]; snprintf(t, sizeof t, "%s", getenv("HK_PD"));
        for (char *sv, *x = strtok_r(t, ",", &sv); x; x = strtok_r(NULL, ",", &sv)) { char *eq = strchr(x, '='); if (eq) { *eq = 0; pd_set_int(x, atoi(eq + 1)); } else pd_set_bool(x, true); }
    }
    if (getenv("HK_PDSTR")) { char t[256]; snprintf(t, sizeof t, "%s", getenv("HK_PDSTR")); char *eq = strchr(t, '='); if (eq) { *eq = 0; pd_set_str(t, eq + 1); } }   // "name=value" test string
    if (getenv("HK_PANTHEON")) { int t = 0, i = 0; sscanf(getenv("HK_PANTHEON"), "%d %d", &t, &i); extern void gg_pantheon_test(int t, int i); gg_pantheon_test(t, i); pd_set_str("dreamReturnScene", "GG_Atrium"); }
    if (getenv("HK_START")) { char rm[64]; float x, y; if (sscanf(getenv("HK_START"), "%63s %f %f", rm, &x, &y) == 3) {
        char *g = strchr(rm, ':'); if (g) *g++ = 0;     // "Room:gate -1 0" enters through that gate like a real transition
        if (strcmp(rm, R.id) || g) enter_room(rm, g ? g : ""); if (x >= 0) test_place(x, y); } }
    if (getenv("HK_FINDWALL")) {   // tests: print right-hand walls 5+ units tall with 3 units of open floor to their left
        int rs = R.res, n = 0;
        for (int cy = rs; cy < R.ch - 6 * rs && n < 40; cy += rs)
            for (int cx = 3 * rs; cx < R.cw - 1 && n < 40; cx++) {
                bool ok = true;
                for (int k = 0; k < 6 * rs && ok; k++) { if (!room_solid_cell(&R, cx + 1, cy + k)) ok = false;
                    for (int j = 0; j < 3 * rs && ok; j++) if (room_solid_cell(&R, cx - j, cy + k)) ok = false; }
                for (int j = 0; j < 3 * rs && ok; j++) if (!room_solid_cell(&R, cx - j, cy - 1)) ok = false;
                if (ok) { fprintf(stderr, "[findwall] %s wall at x=%.2f floor y=%.2f\n", R.id, (cx + 1) / (float)rs, cy / (float)rs); n++; cx += 4 * rs; }
                ok = true;     // and left-hand walls (open floor to the right)
                for (int k = 0; k < 6 * rs && ok; k++) { if (!room_solid_cell(&R, cx - 1, cy + k)) ok = false;
                    for (int j = 0; j < 3 * rs && ok; j++) if (room_solid_cell(&R, cx + j, cy + k)) ok = false; }
                for (int j = 0; j < 3 * rs && ok; j++) if (!room_solid_cell(&R, cx + j, cy - 1)) ok = false;
                if (ok) { fprintf(stderr, "[findwall] %s leftwall at x=%.2f floor y=%.2f\n", R.id, cx / (float)rs, cy / (float)rs); n++; cx += 4 * rs; }
            }
    }
#endif
    // test hook: sdmc:/3ds/hk3ds/test_start.txt = "Room x y" starts a new game there (used for emulator testing)
    { FILE *tf = fopen("sdmc:/3ds/hk3ds/test_start.txt", "rb");
      if (tf) { char rm[32]; float x, y;
                if (fscanf(tf, "%31s %f %f", rm, &x, &y) == 3) { if (strcmp(rm, R.id)) enter_room(rm, ""); if (x >= 0) test_place(x, y); }
                char opt[64] = "";
                while (fscanf(tf, "%63s", opt) == 1) {
                    if (!strncmp(opt, "pd:", 3)) {   // pd:name=value (a number) or pd:name (true): test PlayerData
                        char *eq = strchr(opt + 3, '='); if (eq) { *eq = 0; pd_set_int(opt + 3, atoi(eq + 1)); } else pd_set_bool(opt + 3, true); }
                    if (!strcmp(opt, "herolog")) g_testHeroLog = true;
                    if (!strcmp(opt, "boxes")) dbgBoxes = true;
                    if (!strcmp(opt, "noenemies")) for (int i = 0; i < MAX_ENEMIES; i++) E[i].alive = false;
                    if (!strcmp(opt, "god")) g_testGod = true;
                    if (!strcmp(opt, "autofight")) g_testAuto = true;
                    if (!strncmp(opt, "walk:", 5)) { g_testAuto = true; g_testWalk = opt[5] == 'L' ? -1 : 1; }
                }
                fclose(tf); } }
}

bool game_has_save(void) {
    char sp[64]; slot_path(g_saveSlot, sp, sizeof sp);
    FILE *f = fopen(sp, "rb");
    if (!f) return false;
    fclose(f); return true;
}

bool game_continue(void) {
    char sp[64]; slot_path(g_saveSlot, sp, sizeof sp);
    FILE *f = fopen(sp, "rb");
    if (!f) return false;
    char line[64], room[32] = "Tutorial_01", gate[32] = "";
    reset_player(); pd_reset(); playTime = 0;
    if (fgets(line, sizeof line, f) && !strncmp(line, "HK3DS", 5)) {
        bool v2 = line[5] == '2';
        if (fgets(room, sizeof room, f)) room[strcspn(room, "\r\n")] = 0;
        if (fgets(gate, sizeof gate, f)) gate[strcspn(gate, "\r\n")] = 0;
        int mh = 5, geo = 0, dash = 0;
        if (fgets(line, sizeof line, f)) sscanf(line, "%d %d %d", &mh, &geo, &dash);
        P.maxHealth = mh; P.health = mh; P.geo = geo; P.hasDash = dash;
        pd_set_int("maxHealthBase", mh);
        if (v2 && fgets(line, sizeof line, f)) { int hb = 0; char br[32] = ""; if (sscanf(line, "%31s %f %f %d", br, &benchX, &benchY, &hb) == 4 && hb) { hasBench = true; strcpy(benchRoom, br); } }
        nvisited = 0;
        char ln[128];
        while (fgets(ln, sizeof ln, f)) {
            ln[strcspn(ln, "\r\n")] = 0;
            if (!strncmp(ln, "F ", 2)) { fkDefeated = atoi(ln + 2) & 1; continue; }
            if (!strncmp(ln, "PT ", 3)) { playTime = (float)atoi(ln + 3); continue; }
            if (!strncmp(ln, "Z ", 2)) continue;
            if ((ln[0] == 'B' || ln[0] == 'I' || ln[0] == 'T') && ln[1] == ' ' && pd_load_line(ln)) continue;
            if (!strncmp(ln, "S ", 2)) { if (sscanf(ln + 2, "%31s %f %f %d", shadeRoom, &shadeX, &shadeY, &shadeGeo) == 4) hasShade = true; continue; }
            if (ln[0] && nvisited < 16) { strncpy(visited[nvisited], ln, 31); visited[nvisited][31] = 0; nvisited++; }
        }
    }
    fclose(f);
    pd_set_bool("disablePause", false);     // a save made mid-cutscene or mid-hit never loads with pausing blocked
    // up to v1.0.22 GameManager.CountJournalEntries did nothing, so the Hunter took any Journal as complete and gave his
    // Mark early. With the Mark his hut's floor stays open over a pit that only the Mantis Claw climbs out of: take back
    // a Mark given that way (the Hunter gives it again when the Journal really is complete, the test his talk makes)
    if (pd_bool("hasHuntersMark")) {
        extern void journal_count_entries(void); journal_count_entries();
        if (pd_int("journalNotesCompleted") < pd_int("journalEntriesTotal")) {
            pd_set_bool("hasHuntersMark", false); pd_set_bool("killedHunterMark", false); pd_set_int("killsHunterMark", 1);
            pd_set_bool("hunterRewardOffered", false); pd_set_bool("pbi:Fungus1_08:Shiny Item HunterMark", false);
            hk_log("save: Hunter's Mark given early (journal %d/%d), taken back", pd_int("journalNotesCompleted"), pd_int("journalEntriesTotal"));
        }
    }
    game_charm_update(); game_max_health();
    fade = 1; fadeTarget = 0; paused = false; transitioning = false;
    char r2[32]; strncpy(r2, room, 31); r2[31] = 0;
    if (hasBench) { strcpy(r2, benchRoom); gate[0] = 0; }
    enter_room(r2, gate);
    if (!roomLoaded) { enter_room("Tutorial_01", ""); }
    if (hasBench && !strcmp(R.id, benchRoom)) sit_at_bench();
    areaTitleT = 0;
    return true;
}

void game_end(void) {
    hero_footsteps_stop();
    uo_shutdown();
    masks_unload();
    particles_unload();
    if (roomLoaded) room_free(&R);
    roomLoaded = false;
    audio_stop_stream(0, 1.0f); audio_stop_stream(1, 1.0f);
}

/* ---------------- damage ---------------- */
static void hurt_player(float fromx, bool hazard) { hero_take_damage(fromx, 1, hazard ? 2 : 1); }

// hero.c hooks
void game_shake(float t) { if (t > shakeT) shakeT = t; }
void game_take_geo(int n) { if (n > 0) { P.geo -= n; if (P.geo < 0) P.geo = 0; sfx_play("geo_deplete_count_down", 0.8f); } }
void game_add_geo(int n) { if (n > 0) { P.geo += n; if (P.geo > 9999999) P.geo = 9999999; } }
void game_add_health(int n) { P.health += n; if (P.health > P.maxHealth) P.health = P.maxHealth; }
void game_max_health(void) { P.health = P.maxHealth; blueHealth = charm_blue_max(pd_int("maxHealthBase")); }
// HeroController.CharmUpdate: max health from maxHealthBase and the equipped charms; lifeblood refills
void game_charm_update(void) {
    int base = pd_int("maxHealthBase"); if (base < 1) base = 5;
    int mh = base + (charm_act(23) ? 2 : 0);
    if (charm_joni()) mh = 1;
    P.maxHealth = mh; if (P.health > mh) P.health = mh;
    int bm = charm_blue_max(base);
    if (blueHealth > bm) blueHealth = bm;
}
void game_add_max_health(int n) {     // PlayerData.AddToMaxHealth
    pd_set_int("maxHealthBase", pd_int("maxHealthBase") + n);
    game_charm_update(); P.health = P.maxHealth;
}
// damage soaks the lifeblood masks first (PlayerData.TakeHealth)
void game_add_blue(int n) {               // lifeblood (Health Scuttler / cocoon): extra blue masks
    blueHealth += n; if (blueHealth > 40) blueHealth = 40;
    sfx_play("health_scuttler_heal", 0.8f);
}
void game_cam_shake(int kind) { game_shake(kind >= 2 ? 0.3f : kind == 1 ? 0.15f : 0.08f); }
// SceneManager.darknessLevel per scene (romfs:/text/darkness.txt)
int game_darkness_level(void) {
    static char cur[40]; static int lvl;
    if (strcmp(cur, R.id)) {
        snprintf(cur, sizeof cur, "%s", R.id); lvl = 0;
        FILE *f = fopen("romfs:/text/darkness.txt", "rb");
        if (f) { char ln[96], nm[64]; int v; while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "%63s %d", nm, &v) == 2 && !strcmp(nm, R.id)) { lvl = v; break; } fclose(f); }
    }
    return lvl;
}
// NonSlider walls (romfs:/text/nonslider.txt)
bool game_nonslider(float x0, float y0, float x1, float y1) {
    static char cur[40]; static float (*bx)[4]; static int nb, cap;
    if (strcmp(cur, R.id)) {
        snprintf(cur, sizeof cur, "%s", R.id); nb = 0;
        FILE *f = fopen("romfs:/text/nonslider.txt", "rb");
        if (f) { char ln[128], nm[64]; float a, b, c, d;
            while (fgets(ln, sizeof ln, f)) if (sscanf(ln, "%63s %f %f %f %f", nm, &a, &b, &c, &d) == 5 && !strcmp(nm, R.id)) {
                if (nb >= cap) { cap = cap ? cap * 2 : 32; bx = realloc(bx, cap * sizeof *bx); }
                bx[nb][0] = a; bx[nb][1] = b; bx[nb][2] = c; bx[nb][3] = d; nb++; }
            fclose(f); }
    }
    for (int i = 0; i < nb; i++) if (x0 < bx[i][2] && x1 > bx[i][0] && y0 < bx[i][3] && y1 > bx[i][1]) return true;
    return false;
}
// broadcasts the native HUD / HeroController reacts to
void game_global_event(const char *ev) {
    if (!strcmp(ev, "ADD BLUE HEALTH")) game_add_blue(1);
}
int game_absorb_blue(int amount) {
    hiveT = 0;
    if (blueHealth <= 0) return amount;
    int b = amount < blueHealth ? amount : blueHealth;
    blueHealth -= b; return amount - b;
}
void game_add_soul(int n) { P.soul += n; if (P.soul > 99) P.soul = 99; }
int game_soul(void) { return P.soul; }
void game_hud_out(bool out) { hudOut = out; }
void game_set_bench_respawn(void) { hasBench = true; strncpy(benchRoom, R.id, 31); benchX = P.x; benchY = P.y; }
// the endings: the cinematics aren't in the data, so the ending's title and the game's credits roll, then the title screen
static float endingT; static char endingName[32];
// Godhome pantheons (BossSequence): romfs/text/pantheons.txt lists each tier's scenes; the boss scenes chain until the last
static int panTier, panIdx = -1, panN; static char panScenes[64][32], panEnd[32];
bool gg_pantheon_active(void) { return panTier > 0; }
bool gg_pantheon_first_scene(void) { return panTier <= 0 || panIdx <= 0; }
void gg_pantheon_clear(void) { panTier = 0; panIdx = -1; panN = 0; }
void gg_pantheon_start(int tier) {
    gg_pantheon_clear();
    FILE *f = fopen("romfs:/text/pantheons.txt", "rb"); if (!f) return;
    char ln[2048];
    while (fgets(ln, sizeof ln, f)) {
        int t = atoi(ln); if (t != tier) continue;
        char *tok = strtok(ln, " \r\n"); int k = 0;
        while ((tok = strtok(NULL, " \r\n"))) {
            if (k == 0) {} else if (k == 1) snprintf(panEnd, sizeof panEnd, "%s", strcmp(tok, "-") ? tok : "");
            else if (panN < 64) snprintf(panScenes[panN++], 32, "%s", tok);
            k++;
        }
        break;
    }
    fclose(f);
    if (panN) panTier = tier;
}
void gg_pantheon_test(int t, int i) { gg_pantheon_start(t); panIdx = i; }
static const char *pantheon_first(void) { if (!panTier) return NULL; panIdx = 0; return panScenes[0]; }
void game_load_level(const char *name) {
    if (name && !strcmp(name, "Cinematic_Stag_travel")) { stagTravelT = 3.5f; fade = 1; fadeTarget = 1; sfx_play("Stag_ambient_loop", 0.6f); }
    else if (name && (!strncmp(name, "Cinematic_Ending", 16) || !strcmp(name, "End_Game_Completion") || !strcmp(name, "End_Credits"))) {
        endingT = 0.001f; snprintf(endingName, sizeof endingName, "%s", name); fade = 1; fadeTarget = 1;
        { char k[24] = "completedEndingA"; if (!strncmp(name, "Cinematic_Ending_", 17) && name[17] >= 'A' && name[17] <= 'E') k[15] = name[17]; pd_set_bool(k, true); }
        game_save_now();
        audio_play_stream(0, "title", true, 2.0f);
    }
}
static void ending_draw(void) {
    C2D_DrawRectSolid(0, 0, 0.99f, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 255));
    char L = !strncmp(endingName, "Cinematic_Ending_", 17) ? endingName[17] : 'A';
    const char *title = L == 'B' ? "Sealed Siblings" : L == 'C' ? "Dream No More" : L == 'D' ? "Embrace the Void" : L == 'E' ? "Delicate Flower" : "The Hollow Knight";
    if (endingT < 8) { float a = endingT < 1 ? endingT : endingT > 7 ? 8 - endingT : 1;
        font_draw_centered(&A.fontBig, title, TOP_W / 2, 100, 1, C2D_Color32f(1, 1, 1, a)); return; }
    // credits scroll (the "Credits List" sheet)
    static char lines[160][96]; static int nl = -1;
    if (nl < 0) { nl = 0;
        snprintf(lines[nl++], 96, "Hollow Knight"); snprintf(lines[nl++], 96, "a game by Team Cherry"); lines[nl++][0] = 0;
        char cp[96]; lang_path("Credits List", cp, sizeof cp); FILE *cf = fopen(cp, "rb"); char ln[256];
        if (cf) { while (nl < 160 && fgets(ln, sizeof ln, cf)) { char *t = strchr(ln, '\t'); if (!t) continue; t++; t[strcspn(t, "\r\n")] = 0;
                  for (char *q = t; *q; q++) if (q[0] == '\\' && q[1] == 'n') { q[0] = ' '; memmove(q + 1, q + 2, strlen(q + 2) + 1); }
                  snprintf(lines[nl++], 96, "%s", t); } fclose(cf); } }
    float y = TOP_H - (endingT - 8) * 22;
    for (int i = 0; i < nl; i++, y += 22) if (y > -20 && y < TOP_H) font_draw_centered(&A.fontText, lines[i], TOP_W / 2, y, 1, C2D_Color32(230, 230, 230, 255));
}
void game_begin_transition(const char *scene, const char *gate) {
    if (!scene || !scene[0] || transitioning) return;
    if (!strncmp(scene, "Cinematic_Ending", 16) || !strcmp(scene, "End_Game_Completion") || !strcmp(scene, "End_Credits")) { game_load_level(scene); return; }
    // cutscene-only scenes (no level in them): go straight to where the cutscene leads
    static const char *cut[][3] = { {"GG_Entrance_Cutscene", "GG_Atrium", "door_dreamEnter"}, {"GG_Boss_Door_Entrance", "GG_Atrium", ""},
                                    {"GG_Unlock", "GG_Atrium", ""} };
    for (int i = 0; i < 3; i++) if (!strcmp(scene, cut[i][0])) { scene = cut[i][1]; gate = cut[i][2]; break; }
    if (panTier && !strcmp(scene, "GG_Atrium") && panIdx < 0 && pantheon_first()) { scene = panScenes[0]; gate = "door_dreamEnter"; }
    else if (panTier && panIdx >= 0 && panIdx < panN && !strcmp(R.id, panScenes[panIdx])) {
        // the boss scene (or the spa) is over: on to the next one, or the pantheon is complete
        if (panIdx + 1 < panN) { panIdx++; scene = panScenes[panIdx]; gate = "door_dreamEnter"; }
        else {
            char k[32]; snprintf(k, sizeof k, "pantheonComplete%d", panTier); pd_set_bool(k, true);
            if (panTier == 5) { gg_pantheon_clear(); game_load_level(pd_bool("xunFlowerGiven") ? "Cinematic_Ending_E" : "Cinematic_Ending_D"); return; }
            if (panEnd[0]) { scene = panEnd; gate = ""; }
            gg_pantheon_clear(); game_save_now();
        }
    }
    transitioning = true; fadeTarget = 1;
    snprintf(pendingRoom, sizeof pendingRoom, "%s", scene); snprintf(pendingGate, sizeof pendingGate, "%s", gate ? gate : "");
}
void game_camera_fade(bool out, bool instant) { fadeTarget = out ? 1 : 0; if (instant) fade = fadeTarget; }
bool game_is_paused(void) { return paused; }
bool game_leaving_room(void) { return transitioning; }     // an exit fired: the fade out to the next room is running
void game_freeze(float t) { freezeT = t; }
static bool dreamDeath, keepGeo;
static bool in_dream_zone(void) { return !strcmp(game_map_zone(), "DREAM_WORLD") || !strcmp(game_map_zone(), "GODS_GLORY") || !strcmp(game_map_zone(), "WHITE_PALACE"); }
void game_hero_dead(void) {
    P.dead = true; P.deadTimer = 0;
    play_knight("Death", true);
    // dying in a dream (dream bosses, White Palace, Godhome's boss scenes) wakes the Knight where he entered it:
    // no Shade, no lost geo, charms keep their state
    dreamDeath = in_dream_zone() && pd_str("dreamReturnScene")[0];
    gg_pantheon_clear();      // dying in a pantheon ends it
    if (!dreamDeath) { charm_on_death(); hasShade = true; strncpy(shadeRoom, R.id, 31); shadeX = P.x; shadeY = P.y + COL_OFF_Y; shadeGeo = P.geo; }
    sfx_play("hero_death_v2", 1.0f);
    shakeT = 0.25f;
}
void game_hero_hazard_death(void) { P.hazardTimer = 0.6f; shakeT = 0.25f; }

/* ---------------- nail ---------------- */
static void do_nail_hits(void) {
    float x0, y0, x1, y1;
    hero_nail_box(&x0, &y0, &x1, &y1);
    bool hitSomething = false, gainSoul = false;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &E[i];
        if (!en->alive || en->dying || en->lastSwing == P.swing) continue;
        if (!box_overlap(x0, y0, x1, y1, en->x - en->hw, en->y - en->hh, en->x + en->hw, en->y + en->hh)) continue;
        en->lastSwing = P.swing;
        if (en->type == 4) { boss_hit(en); hitSomething = gainSoul = true; continue; }
        en->hp -= NAIL_DAMAGE;
        en->hitFlash = 0.12f;
        hitSomething = gainSoul = true;
        float dir = (P.attackDir == 0) ? P.facing : (en->x >= P.x ? 1 : -1);
        if (en->hp <= 0) {
            en->dying = true; en->deathTimer = 0;
            en->vx = dir * 6; en->vy = 6;
            anim_play_idx(&en->anim, en->aDAir, true);
            sfx_play("enemy_death_sword", 1.0f);
            spawn_geo(en->x, en->y, en->geo);
            shakeT = 0.12f;
            if (!strncmp(R.ents[en->ent].name, "Hornet", 6)) {     // the cloak corpse drops its Shiny Item (Mothwing Cloak)
                pd_set_bool("hornet1Defeated", true); uo_broadcast("HORNET LEAVE");
                if (!uo_room_active()) { P.hasDash = true; set_message("Mothwing Cloak - press R to dash"); }
            }
        } else {
            sfx_play("enemy_damage", 1.0f);
            en->stun = 0.2f;
            if (en->type == 3 || en->type == 5) { }
            else if (en->type == 1) { en->vx = dir * 10; en->vy = P.attackDir == 1 ? 8 : (P.attackDir == 2 ? -8 : 2); }
            else en->vx = dir * 6;
        }
    }
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        if (!ES[i].alive || ES[i].lastSwing == P.swing) continue;
        if (e->kind != EK_BREAKABLE_WALL && e->kind != EK_GEO_ROCK && e->kind != EK_CHEST && e->kind != EK_COLLAPSE_FLOOR) continue;
        if (e->kind == EK_COLLAPSE_FLOOR && strncmp(e->name, "Break Floor", 11)) continue;
        if (!box_overlap(x0, y0, x1, y1, e->x - e->w / 2, e->y - e->h / 2, e->x + e->w / 2, e->y + e->h / 2)) continue;
        ES[i].lastSwing = P.swing;
        hitSomething = true;
        ES[i].flash = 0.1f;
        if (e->kind == EK_GEO_ROCK) {
            sfx_play("geo_rock_hit_1", 1.0f);
            spawn_geo(e->x, e->y + 0.3f, 1);
            if (--ES[i].hp <= 0) { ES[i].alive = false; spawn_geo(e->x, e->y, 1); }
        } else if (e->kind == EK_CHEST) {
            sfx_play("chest_open", 1.0f);
            ES[i].alive = false; ES[i].state = 1;
            spawn_geo(e->x, e->y + 0.5f, e->geo > 0 ? e->geo / 5 + 4 : 8);
        } else {
            sfx_play(ES[i].hp > 1 ? "breakable_wall_hit_1" : "breakable_wall_death", 1.0f);
            if (--ES[i].hp <= 0) { ES[i].alive = false; shakeT = 0.15f; }
        }
    }
    // Greenpath vine platforms: cutting the vine drops the platform onto its landing spot
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        if (e->kind != EK_VINE || ES[i].state != 0) continue;
        float v0, w0, v1, w1;
        if (sscanf(e->extra, "%f %f %f %f", &v0, &w0, &v1, &w1) != 4) continue;
        if (box_overlap(x0, y0, x1, y1, v0 - 0.3f, w0, v1 + 0.3f, w1)) { ES[i].state = 1; ES[i].vy = 0; sfx_play("breakable_wall_hit_1", 0.8f); hitSomething = true; }
    }
    if (SH.active && !SH.dying) { int before = SH.hp; shade_hit(); if (SH.hp != before) hitSomething = true; }
    {   // converted enemies: HealthManager.Hit through the slash, NailSlash recoil unless a NonBouncer was down-slashed
        int fl = uo_nail_hits(x0, y0, x1, y1, P.swing, P.slashAngle, NAIL_DAMAGE);
        if (fl & 8) hero_slash_hit_shroom();       // a bounce shroom: before the plain recoil, which it overrides
        if ((fl & 1) && !(P.slashAngle == 270 && (fl & 2))) hitSomething = true;
    }
    if (hitSomething) hero_slash_hit_enemy();      // NailSlash: recoil off what the slash touched
    if (gainSoul) { P.soul += charm_soul_per_hit(SOUL_PER_HIT); if (P.soul > SOUL_MAX) P.soul = SOUL_MAX; }
    if (!hitSomething && P.attackDir == 2) {
        // pogo off spikes
        for (int i = 0; i < R.nents; i++) {
            RoomEnt *e = &R.ents[i];
            if (e->kind != EK_SPIKES) continue;
            if (box_overlap(x0, y0, x1, y1, e->x - e->w / 2, e->y - e->h / 2, e->x + e->w / 2, e->y + e->h / 2)) { hero_slash_hit_bouncer(); break; }
        }
    }
}

/* ---------------- InputHandler actions for FSMs / UI ---------------- */
// The New 3DS family (New 2DS XL included) has ZL and ZR; the Old 3DS and 2DS don't. There, L is Super Dash (the bottom
// screen shows the map all the time, so Quick Map can go) and Quick Cast is a tap of A, as Focus/Cast already is.
static bool has_zl_zr(void) {
    static int z = -1;
    if (z < 0) {
#ifdef __HOST__
        z = getenv("HK_OLD3DS") ? 0 : 1;
#else
        bool n3 = false; APT_CheckNew3DS(&n3); z = n3;
#endif
    }
    return z;
}
static void map_game_input(u32 down, u32 held, circlePosition cp) {
    GameInput p = gin, g; memset(&g, 0, sizeof g);
    g.upH = (held & KEY_DUP) || cp.dy > 70; g.downH = (held & KEY_DDOWN) || cp.dy < -70;
    g.leftH = (held & KEY_DLEFT) || cp.dx < -70; g.rightH = (held & KEY_DRIGHT) || cp.dx > 70;
    g.upP = g.upH && !p.upH; g.downP = g.downH && !p.downH; g.leftP = g.leftH && !p.leftH; g.rightP = g.rightH && !p.rightH;
    g.jumpP = down & KEY_B; g.jumpH = held & KEY_B;
    g.attackP = down & KEY_Y; g.attackH = held & KEY_Y;
    g.castP = down & KEY_A; g.castH = held & KEY_A;
    g.dashP = down & KEY_R; g.dashH = held & KEY_R;
    g.quickCastP = down & KEY_ZR; g.quickCastH = held & KEY_ZR; g.quickCastR = !(held & KEY_ZR) && p.quickCastH;
    g.submitP = down & KEY_A; g.cancelP = down & KEY_B;
    g.dreamNailP = down & KEY_X; g.dreamNailH = held & KEY_X;
    u32 sd = has_zl_zr() ? KEY_ZL : KEY_L;
    bool qm = has_zl_zr() || inv_open();         // without ZL, L still turns the inventory's panes
    g.quickMapP = qm && (down & KEY_L); g.quickMapH = qm && (held & KEY_L); g.inventoryP = down & KEY_SELECT;
    g.superDashP = down & sd; g.superDashH = held & sd;
    gin = g;
}

/* ---------------- player frame ---------------- */
#define FOCUS_START_MP 33
static HeroInput map_input(u32 down, u32 held, u32 up, circlePosition cp) {
    HeroInput in; memset(&in, 0, sizeof in);
    float mx = 0, my = 0;
    if (held & KEY_DLEFT) mx = -1;
    if (held & KEY_DRIGHT) mx = 1;
    if (held & KEY_DUP) my = 1;
    if (held & KEY_DDOWN) my = -1;
    if (abs(cp.dx) > 24) mx = cp.dx / 156.0f;
    if (abs(cp.dy) > 24) my = cp.dy / 156.0f;
    if (mx > 1) mx = 1; if (mx < -1) mx = -1; if (my > 1) my = 1; if (my < -1) my = -1;
    in.mx = mx; in.my = my;
    in.leftH = mx < -0.3f; in.rightH = mx > 0.3f; in.upH = my > 0.5f; in.downH = my < -0.5f;
    in.downP = (down & KEY_DDOWN) != 0;
    in.jumpP = down & KEY_B; in.jumpH = held & KEY_B; in.jumpR = up & KEY_B;
    in.atkP = down & KEY_Y; in.atkH = held & KEY_Y;
    in.dashP = down & (KEY_R | KEY_ZR); in.dashH = held & (KEY_R | KEY_ZR);
    in.focusH = held & KEY_A;
    return in;
}

static void player_frame(float dt, u32 down, u32 held, u32 up, circlePosition cp) {
    HeroInput in = map_input(down, held, up, cp);
    if (P.dead) {
        P.deadTimer += dt;
        anim_update(&P.anim, dt);
        if (P.deadTimer > 2.5f && !transitioning && dreamDeath) {
            transitioning = true; fadeTarget = 1; dreamDeath = false; keepGeo = true;
            strncpy(pendingRoom, pd_str("dreamReturnScene"), 31);
            const char *g = pd_str("bossReturnEntryGate"); strncpy(pendingGate, g[0] ? g : "door_dreamReturn", 31);
            loadAtBench = false;
        } else if (P.deadTimer > 2.5f && !transitioning) {
            // respawn at the bench (or the room entrance) with full health; the Shade keeps the geo
            transitioning = true; fadeTarget = 1;
            strncpy(pendingRoom, hasBench ? benchRoom : R.id, 31);
            if (hasBench) pendingGate[0] = 0; else strncpy(pendingGate, curGate, 31);
            loadAtBench = hasBench;
        }
        return;
    }
    if (P.hazardTimer > 0) {
        P.hazardTimer -= dt; anim_update(&P.anim, dt);
        if (P.hazardTimer <= 0) { hero_hazard_respawn(hazardX, hazardY, P.facing > 0); KX = P.x; KY = P.y; snap_camera(); }
        return;
    }
    if (P.sitting) {
        if (anim_is(&P.anim, "Sit") && P.anim.done) play_knight("Sit Idle", true);
        anim_update(&P.anim, dt);
        if (in.jumpP || in.atkP || in.downP || in.leftH || in.rightH) {
            P.sitting = false; play_knight("Get Off", true); hero_regain_control();
        } else return;
    }
    // Focus (the Knight's "Spell Control" FSM; simplified until the FSM runtime runs it): hold A on the ground
    bool canFocus = !uo_has_knight() && in.focusH && !P.controlRelinquished && !dlg_busy() && P.c.onGround && (P.state == AS_IDLE || P.state == AS_RUNNING || P.focusing) && !P.c.attacking &&
                    !P.c.dashing && !P.c.recoiling && P.soul >= FOCUS_COST && P.health < P.maxHealth && P.trans == TS_WAITING_TO_TRANSITION;
    if (canFocus) {
        if (!P.focusing) { P.focusing = true; P.focusTimer = 0; hero_relinquish_control(); play_knight("Focus", true); sfx_stop(focusSfx); focusSfx = sfx_play("focus_health_charging", 0.8f); }
        P.focusTimer += dt;
        if (P.focusTimer >= charm_focus_time(FOCUS_TIME)) {
            P.soul -= FOCUS_COST; P.health += charm_focus_heal(); if (P.health > P.maxHealth) P.health = P.maxHealth;
            sfx_stop(focusSfx); focusSfx = -1;
            sfx_play("focus_health_heal", 1.0f);
            play_knight("Focus Get Once", true);
            P.focusTimer = 0;
        }
        anim_update(&P.anim, dt);
        if (anim_is(&P.anim, "Focus Get Once") && P.anim.done) { play_knight("Focus", true); if (focusSfx < 0) focusSfx = sfx_play("focus_health_charging", 0.8f); }
        return;
    } else if (P.focusing && !uo_has_knight()) { P.focusing = false; play_knight("Focus End", true); hero_regain_control(); sfx_stop(focusSfx); focusSfx = -1; }
    // (with the Knight's own Spell Control running, P.focusing is its SetCState("focusing"): ending it here gave the Knight
    // control back one frame into every focus, so he could walk, jump or cast with the focus still running)
    if (charm_hiveblood() && P.health < P.maxHealth && !P.dead) {      // Hive Health Regen: a mask grows back after 10 s unhurt
        hiveT += dt; if (hiveT >= 10) { hiveT = 0; P.health++; sfx_play("focus_health_heal", 0.6f); }
    } else hiveT = 0;
    hero_update(dt, &in);
}

/* ---------------- False Knight (simplified from the game's FSM) ---------------- */
enum { FK_WAIT, FK_DROP, FK_IDLE, FK_ATK_ANTIC, FK_ATK, FK_ATK_RECOVER, FK_JUMP_ANTIC, FK_JUMP, FK_LAND, FK_JATK_ANTIC, FK_JATK, FK_JATK_HIT,
       FK_STUN_ROLL, FK_STUN_OPEN, FK_STUN_RECOVER, FK_RAGE, FK_DEATH };

static void fk_play(Enemy *en, const char *clip) {
    char nm[72]; snprintf(nm, sizeof nm, "%s:%s", en->lib, clip); nm[31] = 0;
    int a = spr_anim(&R.enemyset, nm);
    if (a >= 0) anim_play_idx(&en->anim, a, true);
}

static void fk_set(Enemy *en, int st, const char *clip) { en->bstate = st; en->bt = 0; en->slamDone = false; if (clip) fk_play(en, clip); }

static void fk_shockwave(Enemy *en, float x0, float x1) {
    float fy = en->y - en->hh;
    shakeT = 0.35f;
    sfx_play("false_knight_strike_ground", 1.0f);
    if (!P.dead && box_overlap(P.x - HURT_HW, P.y + HURT_OY - HURT_HH, P.x + HURT_HW, P.y + HURT_OY + HURT_HH, fminf(x0, x1), fy - 0.2f, fmaxf(x0, x1), fy + 1.6f))
        hurt_player(en->x, false);
}

static void update_boss(Enemy *en, float dt) {
    float dx = P.x - en->x;
    en->bt += dt;
    bool ground = on_ground_box(en->x, en->y, en->hw, en->hh);
    if (!en->bossActive) {
        if (fabsf(dx) < 12 && P.y < en->y) {
            en->bossActive = true;
            strncpy(areaMusic, R.music, 31);
            audio_play_stream(0, "boss1", true, 0.5f);
            fk_set(en, FK_DROP, "Jump");
            en->vy = 0;
        } else return;
    }
    // physics
    if (en->bstate != FK_WAIT) {
        en->vy -= GRAVITY * dt; if (en->vy < -MAX_FALL_VELOCITY * 1.5f) en->vy = -MAX_FALL_VELOCITY * 1.5f;
        float cx = en->x, cy = en->y;
        if (en->bstate == FK_DROP && solid_box(cx - en->hw, cy - en->hh, cx + en->hw, cy + en->hh)) { en->y += en->vy * dt; }   // drops in through the ceiling
        else {
            int fl = move_box(&cx, &cy, en->hw, en->hh, en->vx * dt, en->vy * dt);
            en->x = cx; en->y = cy;
            if (fl & 1) en->vx = 0;
            if (fl & 2) en->vy = 0;
        }
    }
    switch (en->bstate) {
        case FK_DROP:
            if (ground && en->bt > 0.1f) { fk_set(en, FK_LAND, "Land"); shakeT = 0.5f; sfx_play("false_knight_land_1st_time", 1.0f); en->vx = 0; }
            break;
        case FK_IDLE:
            en->vx = 0;
            if (en->anim.anim < 0 || !anim_is(&en->anim, "False Knight:Idle")) fk_play(en, "Idle");
            en->facing = dx > 0 ? 1 : -1;
            if (en->bt > 0.7f) {
                int r = rand() % 3;
                if (fabsf(dx) < 5.5f && r != 2) { fk_set(en, FK_ATK_ANTIC, "Attack Antic"); sfx_play("False_Knight_Attack_New_01", 1.0f); }
                else if (r == 0) fk_set(en, FK_JATK_ANTIC, "Jump Antic");
                else fk_set(en, FK_JUMP_ANTIC, "Jump Antic");
            }
            break;
        case FK_ATK_ANTIC:
            if (en->bt > 0.6f) { fk_set(en, FK_ATK, "Attack"); sfx_play("false_knight_swing", 1.0f); }
            break;
        case FK_ATK:
            if (!en->slamDone && en->bt > 0.12f) { en->slamDone = true; fk_shockwave(en, en->x, en->x + en->facing * 5.0f); }
            if (en->bt > 0.35f) fk_set(en, FK_ATK_RECOVER, "Attack Recover");
            break;
        case FK_ATK_RECOVER:
            if (en->bt > 0.55f) fk_set(en, FK_IDLE, "Idle");
            break;
        case FK_JUMP_ANTIC: case FK_JATK_ANTIC:
            if (en->bt > 0.35f) {
                float tx = dx; if (tx > 9) tx = 9; if (tx < -9) tx = -9;
                en->vy = 22; en->vx = tx / 0.9f;
                sfx_play("false_knight_jump", 1.0f);
                if (en->bstate == FK_JATK_ANTIC) fk_set(en, FK_JATK, "Jump Attack Up"); else fk_set(en, FK_JUMP, "Jump");
            }
            break;
        case FK_JUMP:
            if (ground && en->bt > 0.2f) { fk_set(en, FK_LAND, "Land"); shakeT = 0.3f; sfx_play("false_knight_land", 1.0f); en->vx = 0; }
            break;
        case FK_JATK:
            if (ground && en->bt > 0.2f) { fk_set(en, FK_JATK_HIT, "Jump Attack Hit 1"); en->vx = 0; fk_shockwave(en, en->x - 4.5f, en->x + 4.5f); }
            break;
        case FK_JATK_HIT:
            if (en->bt > 0.6f) fk_set(en, FK_IDLE, "Idle");
            break;
        case FK_LAND:
            if (en->bt > 0.45f) fk_set(en, FK_IDLE, "Idle");
            break;
        case FK_STUN_ROLL:
            en->vx *= 0.95f;
            if (en->bt > 0.7f) { fk_set(en, FK_STUN_OPEN, "Stun Open"); en->vx = 0; sfx_play("Fknight_daze", 1.0f); }
            break;
        case FK_STUN_OPEN:
            if (en->bt > 0.35f && !anim_is(&en->anim, "False Knight:Stun Opened") && !anim_is(&en->anim, "False Knight:Stun Hit")) fk_play(en, "Stun Opened");
            if (en->phase >= 3 && en->headDmg >= 20) {
                fk_set(en, FK_DEATH, "Death Fall");
                sfx_play("false_knight_damage_armour_final", 1.0f);
                shakeT = 0.6f;
                spawn_geo(en->x, en->y + 1, 40);
                fkDefeated = true;
                for (int k = 0; k < R.nents && k < MAX_ENTS; k++) if (R.ents[k].kind == EK_COLLAPSE_FLOOR && !strcmp(R.ents[k].name, "Boss Floor")) ES[k].state = 1;
                audio_play_stream(0, areaMusic, true, 3.0f);
                set_message("False Knight defeated");
            } else if (en->bt > 3.5f) fk_set(en, FK_STUN_RECOVER, "Stun Recover");
            break;
        case FK_STUN_RECOVER:
            if (en->bt > 0.8f) { fk_set(en, FK_RAGE, "Rage"); shakeT = 0.4f; }
            break;
        case FK_RAGE:
            if (en->bt > 1.0f) { en->armor = 65; fk_set(en, FK_IDLE, "Idle"); }
            break;
        case FK_DEATH:
            en->vx = 0;
            if (en->bt > 0.6f && !anim_is(&en->anim, "False Knight:Death Land")) fk_play(en, "Death Land");
            if (en->bt > 6) en->alive = false;
            return;
    }
    // contact damage
    if (en->bstate != FK_STUN_OPEN && !P.dead && box_overlap(P.x - HURT_HW, P.y + HURT_OY - HURT_HH, P.x + HURT_HW, P.y + HURT_OY + HURT_HH,
                               en->x - en->hw, en->y - en->hh, en->x + en->hw, en->y + en->hh))
        hurt_player(en->x, false);
}

static void boss_hit(Enemy *en) {
    if (en->bstate == FK_STUN_OPEN) {
        en->headDmg += NAIL_DAMAGE; en->hitFlash = 0.12f;
        fk_play(en, "Stun Hit");
        sfx_play("false_knight_head_damage_2", 1.0f);
        return;
    }
    if (en->bstate == FK_DEATH || en->bstate == FK_STUN_ROLL || en->bstate == FK_STUN_RECOVER || !en->bossActive) return;
    en->armor -= NAIL_DAMAGE; en->hitFlash = 0.1f;
    sfx_play("false_knight_damage_armour", 0.9f);
    if (en->armor <= 0) {
        en->phase++; en->headDmg = 0;
        fk_set(en, FK_STUN_ROLL, "Stun Roll");
        en->vx = (P.x < en->x ? 1 : -1) * 6; en->vy = 6;
        shakeT = 0.3f;
    }
}

/* ---------------- enemies ---------------- */
static void update_enemies(float dt) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &E[i];
        if (!en->alive) continue;
        anim_update(&en->anim, dt);
        if (en->hitFlash > 0) en->hitFlash -= dt;
        if (en->type == 4) { update_boss(en, dt); continue; }
        float cx = en->x, cy = en->y;
        if (en->dying) {
            en->deathTimer += dt;
            en->vy -= GRAVITY * dt; if (en->vy < -MAX_FALL_VELOCITY) en->vy = -MAX_FALL_VELOCITY;
            en->vx *= 0.97f;
            int fl = move_box(&cx, &cy, en->hw, en->hh, en->vx * dt, en->vy * dt);
            en->x = cx; en->y = cy;
            if (fl & 2) { en->vx = 0; if (!en->onGround) { en->onGround = true; anim_play_idx(&en->anim, en->aDLand >= 0 ? en->aDLand : en->aDAir, true); } }
            if (en->deathTimer > 8) en->alive = false;
            continue;
        }
        if (en->stun > 0) en->stun -= dt;
        float dx = P.x - en->x, dy = (P.y + COL_OFF_Y) - en->y;
        if (en->type == 3 || en->type == 5) {     // 5: hovers in place (Duranda)
            if (en->anim.anim < 0) anim_play_idx(&en->anim, en->aIdle >= 0 ? en->aIdle : en->aWalk, true);
            en->facing = dx > 0 ? 1 : -1;
        } else if (en->type == 0 || en->type == 2) {
            // ground enemies: patrol, turn at walls/ledges; type 2 (husks) charge when the Knight is close
            bool charging = en->type == 2 && fabsf(dx) < 7 && fabsf(dy) < 3;
            float speed = charging ? 4.5f : en->speed;
            if (en->turnCool > 0) en->turnCool -= dt;
            if (charging && en->turnCool <= 0) en->facing = dx > 0 ? 1 : -1;
            if (en->stun > 0) { en->vx *= 0.9f; }
            else en->vx = en->facing * speed;
            en->vy -= GRAVITY * dt; if (en->vy < -MAX_FALL_VELOCITY) en->vy = -MAX_FALL_VELOCITY;
            if (en->grounded && en->vy < 0) en->vy = -0.5f;         // stay pressed to the floor, no gravity build-up
            unstick_box(&cx, &cy, en->hw, en->hh, 0.5f);
            // walk up small steps and slopes like the Knight does (the collision grid is 1/4 unit)
            int fl = move_box_ex(&cx, &cy, en->hw, en->hh, en->vx * dt, en->vy * dt, en->grounded ? 0.4f : 0.0f);
            bool ground = on_ground_box(cx, cy, en->hw, en->hh);
            if (!ground && en->grounded && en->vy <= 0) {            // follow slopes and small drops downward
                float sx = cx, sy = cy;
                int f2 = move_box(&sx, &sy, en->hw, en->hh, 0, -0.45f);
                if (f2 & 2) { cx = sx; cy = sy; ground = true; fl |= 2; }
            }
            en->x = cx; en->y = cy;
            if (fl & 2) en->vy = 0;
            en->grounded = ground;
            // a real ledge = no floor within half a unit just ahead of the feet
            float lx0 = cx + en->facing * (en->hw + 0.05f), lx1 = cx + en->facing * (en->hw + 0.3f);
            bool ledge = ground && !solid_box(fminf(lx0, lx1), cy - en->hh - 0.5f, fmaxf(lx0, lx1), cy - en->hh - 0.01f);
#ifdef __HOST__
            if (getenv("HK_TURNTRACE") && en->stun <= 0 && en->turnCool <= 0 && ((fl & 1) || ledge))
                fprintf(stderr, "turn E%d %s at %.2f,%.2f wall=%d ledge=%d ground=%d\n", i, en->lib, cx, cy, fl & 1, ledge, ground);
#endif
            if (en->stun <= 0 && en->turnCool <= 0 && ((fl & 1) || ledge)) {
                en->facing = -en->facing; en->turnCool = 0.3f;
                if (charging) { en->vx = 0; en->turnCool = 0.8f; }
            }
            int want = charging && en->aChase >= 0 ? en->aChase : en->aWalk;
            if (en->anim.anim != want) anim_play_idx(&en->anim, want, true);
        } else {
            // flyers: hover until the Knight gets close, then chase
            if (!en->alert && fabsf(dx) < 8 && fabsf(dy) < 6) {
                en->alert = true; en->startle = 0.4f;
                if (en->aAttack >= 0) anim_play_idx(&en->anim, en->aAttack, true);
                sfx_play("buzzer_startle_01", 0.9f);
            }
            if (en->startle > 0) { en->startle -= dt; en->vx *= 0.9f; en->vy *= 0.9f; }
            else if (en->alert && en->stun <= 0) {
                float d = sqrtf(dx * dx + dy * dy) + 1e-3f;
                float acc = 14.0f, maxs = 6.5f;
                en->vx += dx / d * acc * dt; en->vy += dy / d * acc * dt;
                float sp = sqrtf(en->vx * en->vx + en->vy * en->vy);
                if (sp > maxs) { en->vx *= maxs / sp; en->vy *= maxs / sp; }
                int want = en->aChase >= 0 ? en->aChase : en->aIdle;
                if (en->anim.anim != want) anim_play_idx(&en->anim, want, true);
                en->facing = dx > 0 ? 1 : -1;
            } else if (en->stun > 0) { en->vx *= 0.92f; en->vy *= 0.92f; }
            else {
                en->vy = sinf(svcGetSystemTick() / 268111856.0f * 2.0f + i) * 0.6f; en->vx = 0;
            }
            unstick_box(&cx, &cy, en->hw, en->hh, 0.5f);
            int fl = move_box(&cx, &cy, en->hw, en->hh, en->vx * dt, en->vy * dt);
            en->x = cx; en->y = cy;
            if (fl & 1) en->vx = -en->vx * 0.5f;
            if (fl & 6) en->vy = -en->vy * 0.5f;
        }
#ifdef __HOST__
        if (getenv("HK_ETRACE")) {
            static int lastFacing[MAX_ENEMIES], flips[MAX_ENEMIES], frames;
            if (en->facing != lastFacing[i]) { flips[i]++; lastFacing[i] = en->facing; }
            if (i == 0) frames++;
            if (frames % 120 == 0)
                fprintf(stderr, "E%d %s type=%d pos=%.2f,%.2f box=%.2fx%.2f vx=%.2f vy=%.2f flips=%d embedded=%d ground=%d\n", i, en->lib, en->type, en->x, en->y,
                        en->hw * 2, en->hh * 2, en->vx, en->vy, flips[i], solid_box(en->x - en->hw, en->y - en->hh, en->x + en->hw, en->y + en->hh),
                        on_ground_box(en->x, en->y, en->hw, en->hh));
        }
#endif
        // contact damage
        if (!P.dead && box_overlap(P.x - HURT_HW, P.y + HURT_OY - HURT_HH, P.x + HURT_HW, P.y + HURT_OY + HURT_HH,
                                   en->x - en->hw, en->y - en->hh, en->x + en->hw, en->y + en->hh))
            hurt_player(en->x, false);
    }
}

static void update_shade(float dt) {
    if (!SH.active) return;
    anim_update(&SH.anim, dt);
    SH.t += dt; if (SH.flash > 0) SH.flash -= dt;
    if (SH.dying) { if (SH.anim.done || SH.t > 2) SH.active = false; return; }
    float dx = P.x - SH.x, dy = (P.y + COL_OFF_Y) - SH.y, d = sqrtf(dx * dx + dy * dy) + 1e-3f;
    if (!SH.alert && d < 7) { SH.alert = true; anim_play(&SH.anim, &A.enemies, "Shade Startle", true); }
    if (SH.alert) {
        SH.facing = dx > 0 ? 1 : -1;
        if (SH.atk > 0) {
            SH.atk -= dt;
            if (SH.atk < 0.35f && !anim_is(&SH.anim, "Shade Slash")) { anim_play(&SH.anim, &A.enemies, "Shade Slash", true); sfx_play("hollow_shade_sword", 0.9f); SH.vx = SH.facing * 12; }
            SH.vx *= 0.9f; SH.vy *= 0.9f;
        } else {
            SH.vx += dx / d * 8 * dt; SH.vy += dy / d * 8 * dt;
            float sp = sqrtf(SH.vx * SH.vx + SH.vy * SH.vy); if (sp > 3) { SH.vx *= 3 / sp; SH.vy *= 3 / sp; }
            if (!anim_is(&SH.anim, "Shade Fly") && SH.anim.done) anim_play(&SH.anim, &A.enemies, "Shade Fly", true);
            if (d < 3 && (rand() % 90) == 0) { SH.atk = 0.8f; anim_play(&SH.anim, &A.enemies, "Shade Slash Antic", true); SH.vx = SH.vy = 0; }
        }
        float cx = SH.x, cy = SH.y;
        move_box(&cx, &cy, 0.4f, 0.6f, SH.vx * dt, SH.vy * dt);
        SH.x = cx; SH.y = cy;
        if (!P.dead && fabsf(dx) < 0.7f && fabsf(dy) < 1.0f) hurt_player(SH.x, false);
    }
}

static void shade_hit(void) {
    if (!SH.active || SH.dying) return;
    float x0, y0, x1, y1; hero_nail_box(&x0, &y0, &x1, &y1);
    if (SH.lastSwing == P.swing || !box_overlap(x0, y0, x1, y1, SH.x - 0.5f, SH.y - 0.7f, SH.x + 0.5f, SH.y + 0.7f)) return;
    SH.lastSwing = P.swing;
    SH.hp -= NAIL_DAMAGE; SH.flash = 0.12f; SH.vx = P.facing * 6; SH.alert = true;
    sfx_play("enemy_damage", 1.0f);
    if (SH.hp <= 0) {
        SH.dying = true; SH.t = 0;
        anim_play(&SH.anim, &A.enemies, "Shade Death", true);
        sfx_play("enemy_death_sword", 1.0f);
        P.geo += shadeGeo; hasShade = false; shadeGeo = 0;
        set_message("Geo recovered");
    }
}

/* ---------------- geo & entities ---------------- */
static void update_geo(float dt) {
    for (int i = 0; i < MAX_GEO; i++) {
        Geo *g = &G[i];
        if (!g->alive) continue;
        g->t += dt;
        g->vy -= GRAVITY * 0.6f * dt;
        float cx = g->x, cy = g->y;
        int fl = move_box(&cx, &cy, 0.15f, 0.15f, g->vx * dt, g->vy * dt);
        g->x = cx; g->y = cy;
        if (fl & 1) g->vx = -g->vx * 0.5f;
        if (fl & 2) { if (g->vy < -3) sfx_play_pitch("geo_hit_ground_1", 0.4f, 1.0f); g->vy = -g->vy * 0.35f; g->vx *= 0.6f; if (fabsf(g->vy) < 1) g->vy = 0; }
        if (fl & 4) g->vy = 0;
        if (g->t > 0.3f && fabsf(g->x - P.x) < 0.7f && fabsf(g->y - (P.y + COL_OFF_Y)) < 1.0f) {
            g->alive = false; P.geo++;
            sfx_play_pitch("geo_small_collect_1", 0.7f, 0.95f + (rand() % 10) / 100.0f);
        }
        if (g->t > 60) g->alive = false;
    }
}

static void update_entities(float dt) {
    nearDoor = false;
    float px0 = P.x - HALF_W, py0 = P.y + COL_OFF_Y - HALF_H, px1 = P.x + HALF_W, py1 = P.y + COL_OFF_Y + HALF_H;
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        EntState *s = &ES[i];
        if (s->flash > 0) s->flash -= dt;
        float ex0 = e->x - e->w / 2, ey0 = e->y - e->h / 2 + s->dy, ex1 = e->x + e->w / 2, ey1 = e->y + e->h / 2 + s->dy;
        bool touch = box_overlap(px0, py0, px1, py1, ex0, ey0, ex1, ey1);
        switch (e->kind) {
            case EK_SPIKES:
                if (touch && !P.dead) hurt_player(e->x, true);
                break;
            case EK_HAZARD_RESPAWN:
                if (touch) {   // HK: entering the trigger sets the respawn point to its linked marker
                    float hx, hy;
                    if (sscanf(e->extra, "R %f %f", &hx, &hy) == 2) { hazardX = hx; hazardY = hy; }
                    else if (P.onGround) { hazardX = P.x; hazardY = P.y; }
                }
                break;
            case EK_STALACTITE:
                if (s->state == 0 && fabsf(P.x - e->x) < 1.5f && P.y < e->y && e->y - P.y < 12 &&
                    !solid_box(e->x - 0.05f, P.y, e->x + 0.05f, e->y - e->h / 2 - 0.05f)) { s->state = 1; sfx_play("cave_in_rock_hit", 0.7f); }
                if (s->state == 1) {
                    s->vy -= GRAVITY * dt; s->dy += s->vy * dt;
                    if (touch) hurt_player(e->x, false);
                    if (solid_box(ex0 + 0.1f, ey0 - 0.05f, ex1 - 0.1f, ey0)) { s->state = 2; sfx_play("cave_in_rock_hit_2", 0.8f); }
                    if (s->dy < -40) s->state = 2;
                }
                break;
            case EK_COLLAPSE_FLOOR:
                if (!strcmp(e->name, "Boss Floor") && s->alive && s->state == 1) {
                    s->t += dt;
                    if (s->t > 2.5f) { s->alive = false; sfx_play("breakable_wall_death", 1.0f); shakeT = 0.6f; }
                }
                if (strncmp(e->name, "Collapser", 9) == 0 && s->alive) {
                    bool standing = P.onGround && box_overlap(px0, py0 - 0.1f, px1, py0, ex0, ey0, ex1, ey1 + 0.05f);
                    if (standing && s->state == 0) { s->state = 1; s->t = 0; sfx_play("breakable_wall_hit_1", 0.8f); }
                    if (s->state == 1) { s->t += dt; if (s->t > 0.8f) { s->alive = false; sfx_play("breakable_wall_death", 1.0f); shakeT = 0.2f; } }
                }
                break;
            case EK_BENCH:
                if (touch && P.onGround && !P.sitting && (keysDownCache & KEY_DUP) && !P.c.attacking && P.state != AS_NO_INPUT) {
                    hero_relinquish_control();
                    P.sitting = true; game_charm_update(); game_max_health(); hero_set_position(e->x, P.y);
                    pd_clear_prefix("pbs:");    // resting resets semi-persistent enemies
                    play_knight("Sit", true);
                    sfx_play("bench_rest", 1.0f);
                    hasBench = true; strncpy(benchRoom, R.id, 31); benchX = e->x; benchY = P.y;
                    if (map_update()) set_message("Map updated"); else set_message("Game saved");
                    save_game(R.id, "");
                }
                break;
            case EK_VINE:
                if (s->state == 1) {
                    float drop = (float)atof(e->target);
                    s->vy -= GRAVITY * dt; s->dy += s->vy * dt;
                    if (s->dy <= -drop) { s->dy = -drop; s->vy = 0; s->state = 2; sfx_play("hero_land_hard", 0.7f); shakeT = 0.15f; }
                }
                break;
            case EK_GATE:
                { extern bool gate_enabled(const RoomEnt *e); if (!gate_enabled(e)) { s->state = 0; break; } }
                { float a, b, c, d; gate_box(e, &a, &b, &c, &d);
                  // an edge exit also covers everything beyond it: the Knight can't stand outside the room
                  // (e.g. stopped short by a closed pillar while walking in) without the exit taking him back
                  if (strncmp(e->name, "door", 4)) {
                      if (c <= 1.0f) a = -1000; else if (a >= R.w - 1.0f) c = R.w + 1000.0f;
                      else if (b >= R.h - 1.0f) d = R.h + 1000.0f;
                      else if (d <= 1.0f) {
                          b = -1000;
                          if (P.y < -3.0f) {      // fallen out of the room: the nearest bottom exit takes him
                              float best = 1e9f; const RoomEnt *bg = NULL;
                              for (int k = 0; k < R.nents; k++) { const RoomEnt *q = &R.ents[k]; float qa, qb, qc, qd;
                                  if (q->kind != EK_GATE || !q->target[0] || !strncmp(q->name, "door", 4)) continue;
                                  gate_box(q, &qa, &qb, &qc, &qd); if (qd > 1.0f) continue;
                                  float dx = fabsf(P.x - (qa + qc) / 2); if (dx < best) { best = dx; bg = q; } }
                              if (bg == e) { a = -1e6f; c = 1e6f; }
                          }
                      }
                  }
                  touch = box_overlap(px0, py0, px1, py1, a, b, c, d); }
#ifdef __HOST__
                if (getenv("HK_GATEDBG") && !strcmp(e->name, getenv("HK_GATEDBG"))) fprintf(stderr, "[gatedbg] %s touch=%d state=%d P.y=%.2f tr=%d\n", e->name, touch, s->state, P.y, P.c.transitioning);
#endif
                if (!touch) s->state = 0;
                else if (P.c.transitioning && !strcmp(e->name, curGate)) s->state = 1;   // the arrival exit ignores the Knight while he enters
                // TransitionPoint.OnTriggerStay2D: once the Knight has finished entering and plays again, standing in
                // an exit's trigger still takes it (an exit blocked just inside, e.g. a closed shortcut pillar, sends him back)
                if (touch && s->state == 1 && P.enterPhase == 0 && !P.c.transitioning && P.acceptingInput && !P.controlRelinquished
                    && P.trans == TS_WAITING_TO_TRANSITION && strncmp(e->name, "door", 4) && (e->x <= 1.0f || e->x >= R.w - 1.0f)) s->state = 0;
                if (touch && s->state == 1 && P.y < -3.0f && strncmp(e->name, "door", 4)) s->state = 0;   // fell back out of a bottom entry
                if (!strncmp(e->name, "door", 4)) {           // doors: press Up to go in
                    if (uo_door_fsm(e->name)) break;           // the door's own Door Control FSM handles it
                    nearDoor = touch && e->target[0] && P.onGround;
                    if (!(nearDoor && (keysDownCache & KEY_DUP))) break;
                    s->state = 0;
                }
                if (touch && s->state == 0 && !e->target[0] && panTier && !strcmp(R.id, "GG_Spa") && strncmp(e->name, "door", 4) && !transitioning && !P.dead) {
                    s->state = 1; game_begin_transition("GG_Atrium", ""); break;     // the spa's way out leads to the next boss
                }
                if (touch && s->state == 0 && e->target[0] && !transitioning && (!P.c.transitioning || P.y < -3.0f) && !P.dead) {
                    char *c = strchr(e->target, ':');
                    if (c) {
                        memset(pendingRoom, 0, 32); memset(pendingGate, 0, 32);
                        strncpy(pendingRoom, e->target, c - e->target);
#ifdef __HOST__
                        fprintf(stderr, "[gate] %s fired at P=(%.2f,%.2f)\n", e->name, P.x, P.y);
#endif
                        strncpy(pendingGate, c + 1, 31);
                        transitioning = true; fadeTarget = 1;
                        // HeroController.LeaveScene: no input (no casting, talking or reading a tablet during the fade out),
                        // the Knight runs or drops out by himself; the next room's entry gives input back (FinishedEnteringScene)
                        // the game also tells the Knight's scripts he's leaving (HeroCtrl-LeavingScene): Spell Control cancels a
                        // cast in progress (control, animation and gravity back). Not sent, a fireball cast at an exit carried on in
                        // the next room with control given back: the recoil state held him in the air, only able to move sideways.
                        // A dive (Desolate Dive) is left running: it carries on down through the next room as before.
                        { UObj *kh = uo_hero();
                          if (kh && !P.c.spellQuake) for (int k = 0; k < kh->nfsm; k++)
                              if (kh->fsm[k]->name && !strcmp(kh->fsm[k]->name, "Spell Control") && strcmp(fsm_state_name(kh->fsm[k]), "Inactive"))
                                  fsm_event(kh->fsm[k], "LEAVING SCENE"); }
                        hero_leave_scene(gate_pos_of(e->name));
                    }
                }
                break;
            default: break;
        }
    }
}

/* ---------------- camera ---------------- */
static void update_camera(float dt) {
    cam_set_hero(KX, KY);
    cam_update(dt);
    camx = cam_x(); camy = cam_y();
}
static void snap_camera(void) {
    cam_set_hero(P.x, P.y); cam_position_to_hero();
    camx = cam_x(); camy = cam_y();
}

/* ---------------- main update ---------------- */
int game_update(float dt, u32 down, u32 held, u32 up, circlePosition cp) {
    dbgDt[dbgDtI++ % 60] = dt;
#ifdef __HOST__
    { extern void game_solid_probe(void); game_solid_probe(); }
#endif
    hero_footsteps_end_frame();     // the run loop stops while paused, in menus, sitting, focusing...
    if (!paused) playTime += dt;
    ach_update(dt);
#ifdef __HOST__
    // HK_ROOMCYCLE="RoomA,RoomB,...": enter each room in turn every 240 frames and print the heap (leak hunting)
    { static int cyc = 0, ci = 0; const char *rc = getenv("HK_ROOMCYCLE");
      if (rc && ++cyc % 240 == 0) {
          extern size_t host_heap_used(void);
          char t[2048]; snprintf(t, sizeof t, "%s", rc); int k = 0; char *sv, *x = strtok_r(t, ",", &sv);
          int n = 0; for (char *y = x; y; y = strtok_r(NULL, ",", &sv)) n++;
          snprintf(t, sizeof t, "%s", rc); x = strtok_r(t, ",", &sv);
          for (; x && k < ci % n; k++) x = strtok_r(NULL, ",", &sv);
          printf("[cycle] leaving %s heap=%zuKB\n", R.id, host_heap_used() / 1024);
          if (x) { enter_room(x, ""); test_place(R.w * 0.5f, R.h * 0.5f); }
          ci++;
      } }
#endif
    // test runs on the 3DS (emulator): sdmc:/3ds/hk3ds/cycle.txt = "<frames per room>" then one room per line. Each room
    // is entered through its first exit like a real transition; the log gets its frame rate and where the Knight ended up
    { static int st = 0, n = 0, i = 0, fr = 0, per = 120; static char rooms[600][64]; static float tsum, tmax;
      if (st == 0) {
          st = 1; FILE *f = fopen("sdmc:/3ds/hk3ds/cycle.txt", "rb");
          if (f) { char ln[64]; if (fgets(ln, sizeof ln, f)) per = atoi(ln) > 10 ? atoi(ln) : 120;
                   while (n < 600 && fgets(ln, sizeof ln, f)) { ln[strcspn(ln, "\r\n")] = 0; if (ln[0]) snprintf(rooms[n++], 64, "%s", ln); }
                   fclose(f); if (n) { st = 2; g_testGod = true; hk_log("cycle: %d rooms, %d frames each", n, per); } }
      }
      if (st == 2 && !transitioning) {
          tsum += dt; if (dt > tmax) tmax = dt;
          if (++fr >= per || i == 0) {
              if (i > 0) hk_log("cycle %s: %.1f fps (worst frame %.0f ms) Knight %.1f,%.1f%s", R.id, fr / (tsum > 0 ? tsum : 1), tmax * 1000, P.x, P.y,
                                (P.x < -2 || P.x > R.w + 2 || P.y < -2) ? " OUTSIDE" : "");
              if (i >= n) { hk_log("cycle done"); st = 3; }
              else {
                  char rm[32] = "", g[32] = "";     // "Room gate": enter through that exit, like a real transition
                  sscanf(rooms[i], "%31s %31s", rm, g);
                  enter_room(rm, g);
                  i++;
              }
              fr = 0; tsum = 0; tmax = 0;
          }
      } }
    if (paused) {     // the game's pause menu: Continue / Options / Quit to Menu
        pauseT += dt; pausePtrT += dt;
        if (pausePage == 0) {
            if (down & KEY_DUP) { pauseSel = (pauseSel + 2) % 3; pausePtrT = 0; sfx_play("button", 0.6f); }
            if (down & KEY_DDOWN) { pauseSel = (pauseSel + 1) % 3; pausePtrT = 0; sfx_play("button", 0.6f); }
            if (down & KEY_A) {
                sfx_play("begin_button", 0.8f);
                if (pauseSel == 0) paused = false;
                else if (pauseSel == 1) { pausePage = 1; pauseSel = 0; pausePtrT = 0; }
                else { paused = false; if (!P.dead) game_save_now(); return 1; }     // Quit to Menu saves, like the game
            }
            if (down & (KEY_START | KEY_B)) { paused = false; sfx_play("button", 0.6f); }
        } else {
            if (down & (KEY_DUP | KEY_DDOWN)) { pauseSel = (pauseSel + ((down & KEY_DUP) ? 3 : 1)) % 4; pausePtrT = 0; sfx_play("button", 0.6f); }
            if (down & (KEY_DLEFT | KEY_DRIGHT | (pauseSel >= 2 ? KEY_A : 0))) {
                if (pauseSel == 2) { lang_set((g_lang + 1) % LANG_COUNT); ach_reload(); }      // Language
                else if (pauseSel == 3) g_hints = !g_hints;                                     // Button Hints
                else {
                    float d = (down & KEY_DLEFT) ? -0.1f : 0.1f; float *v = pauseSel == 0 ? &g_musicVolume : &g_sfxVolume;
                    *v += d; if (*v < 0) *v = 0; if (*v > 1) *v = 1;
                }
                sfx_play("button", 0.5f);
            }
            if (down & KEY_B) { pausePage = 0; pauseSel = 1; pausePtrT = 0; sfx_play("button", 0.6f); settings_save(); }
            if (down & KEY_START) { paused = false; pausePage = 0; settings_save(); }
        }
        return 0;
    }
    if (endingT > 0) {      // the ending: any button after the title skips; the credits end at the title screen
        endingT += dt;
        if ((endingT > 8 + 3 && (down & (KEY_A | KEY_START))) || endingT > 8 + 170) { endingT = 0; return 1; }
        return 0;
    }
    GameInput ginPrev = gin; (void)ginPrev;
    map_game_input(down, held, cp);
    if ((down & KEY_START) && !pd_bool("disablePause") && !dlg_busy() && !menus_busy() && !msgs_busy() && !shop_busy()) { paused = true; pauseSel = 0; pausePage = 0; pauseT = 0; pausePtrT = 0; sfx_play("button", 0.6f); return 0; }
    if (getenv("HK_INVDBG") && gin.inventoryP) fprintf(stderr, "INV dp%d dlg%d men%d msg%d shop%d tr%d st%.1f dead%d any%d\n", pd_bool("disablePause"), dlg_busy(), menus_busy(), msgs_busy(), shop_busy(), transitioning, stagTravelT, P.dead, charm_any_owned());
    if (inv_open()) { inv_update(dt); return 0; }      // the inventory pane holds the game (the game keeps running; here it pauses)
    if (gin.inventoryP && !pd_bool("disablePause") && !dlg_busy() && !menus_busy() && !msgs_busy() && !shop_busy() && !transitioning &&
        stagTravelT <= 0 && !P.dead) { inv_toggle(); return 0; }
    if (stagTravelT > 0) {     // StagTravel: minimumLoadDuration 3.5s, then the station's door_stagExit
        stagTravelT -= dt; fade = 1;
        if (stagTravelT <= 0 && !transitioning) {
            const char *ns = pd_str("nextScene");
            if (ns[0]) { transitioning = true; fadeTarget = 1; snprintf(pendingRoom, sizeof pendingRoom, "%s", ns); snprintf(pendingGate, sizeof pendingGate, "door_stagExit"); }
            else fadeTarget = 0;
        } else if (stagTravelT > 0) return 0;
    }
    // fade / transitions
    if (fade < fadeTarget) { fade += dt * 3.5f; if (fade > fadeTarget) fade = fadeTarget; }
    if (fade > fadeTarget) { fade -= dt * 2.0f; if (fade < fadeTarget) fade = fadeTarget; }
    if (transitioning && fade >= 1) {
        char r[32], g[32]; strcpy(r, pendingRoom); strcpy(g, pendingGate);
        bool wasDead = P.dead;
        if (P.dead) {
            int mh = P.maxHealth, soul = P.soul, geo = P.geo; bool hd = P.hasDash, hw = P.hasWalljump, dj = P.hasDoubleJump;
            hero_reset(); P.maxHealth = mh; P.health = mh; game_charm_update(); game_max_health(); P.soul = soul; P.geo = keepGeo ? geo : 0; P.hasDash = hd; P.hasWalljump = hw; P.hasDoubleJump = dj;
            keepGeo = false;
            play_knight("Idle", true);
        }
        enter_room(r, g);
        if (wasDead && loadAtBench && !strcmp(R.id, benchRoom)) sit_at_bench();
        loadAtBench = false;
        transitioning = false; fadeTarget = 0;
    }
    if (shakeT > 0) shakeT -= dt;
    if (msgT > 0) msgT -= dt;
    area_titles_update(dt, P.x - HALF_W, P.y + COL_OFF_Y - HALF_H, P.x + HALF_W, P.y + COL_OFF_Y + HALF_H);
    if ((g_testAuto || getenv("HK_AUTOFIGHT")) && !P.dead) {
        // test bot: walk to the nearest live enemy (or one whose name contains HK_AUTOFIGHT) and keep hitting it
        static u32 prevK; static int tick; tick++;
#ifdef __HOST__
        static int after = -1; if (after < 0) after = getenv("HK_AUTOFIGHT_AFTER") ? atoi(getenv("HK_AUTOFIGHT_AFTER")) : 0;
        if (tick < after) goto bot_done;
#endif
        const char *want = getenv("HK_AUTOFIGHT"); if (!want) want = "1"; bool any = !strcmp(want, "1");
        UObj *best = NULL; float bd = 1e9f, bx = 0, by = 0;
        const char *extra = NULL;
#ifdef __HOST__
        extra = getenv("HK_BOTTARGETS");     // also go and hit these objects while their FSM idles (the final boss's chains)
#endif
        for (int i = 0; i < uo_count(); i++) { UObj *o = uo_obj(i); if (!o || o->destroyed || o->bank == 1 || !uo_active(o)) continue;
            bool ex = extra && !strncmp(o->name, extra, strlen(extra)) && o->nfsm && o->fsm[0]->name && !strcmp(o->fsm[0]->name, "Control") && !strcmp(fsm_state_name(o->fsm[0]), "Idle");
            if (!ex && (!uo_comp(o, C_HEALTH) || hm_is_dead(o))) continue;
            if (!any && !strstr(o->name, want)) continue;
            float x = uo_wx(o), y = uo_wy(o), x0, y0, x1, y1;
            bool body = false;
            for (int k = 0; k < o->ncol; k++) if (o->col[k].en && !o->col[k].trig && uo_col_aabb(o, &o->col[k], &x0, &y0, &x1, &y1)) { x = (x0 + x1) / 2; y = (y0 + y1) / 2; body = true; break; }
            if (!body)      // only a hurtbox (a trigger: the Radiance's hexagon): aim at it, not at the object's origin
                for (int k = 0; k < o->ncol; k++) if (o->col[k].en && uo_col_aabb(o, &o->col[k], &x0, &y0, &x1, &y1)) { x = (x0 + x1) / 2; y = (y0 + y1) / 2; break; }
            if (g_testWalk && (fabsf(x - P.x) > 7 || fabsf(y - (P.y + COL_OFF_Y)) > 3.5f || (x - P.x) * g_testWalk < -3)) continue;   // walk: only what's in the way
            float d = fabsf(x - P.x) + fabsf(y - P.y) * 0.5f + (hm_invincible(o) ? 30 : 0); if (d < bd) { bd = d; best = o; bx = x; by = y; } }
        u32 k = 0;
        if (!best && tick % 300 == 1) fprintf(stderr, "[auto] no target (objs %d)\n", uo_count());
        if (best) {
            float dx = bx - P.x, dy = by - (P.y + COL_OFF_Y);
            if (fabsf(dx) > 1.6f) k |= dx > 0 ? KEY_DRIGHT : KEY_DLEFT;
            else if ((dx > 0) != (P.facing > 0)) k |= dx > 0 ? KEY_DRIGHT : KEY_DLEFT;
            if (dy > 2.0f && (tick % 40) < 14) k |= KEY_B;
            if (dy > 1.5f) k |= KEY_DUP; else if (dy < -1.5f && !P.onGround) k |= KEY_DDOWN;
            if (fabsf(dx) < 3.0f && (tick % 12) < 3) k |= KEY_Y;
        }
        if (!best && g_testWalk) {     // walk: keep going that way; jump when blocked, at a gap, and up walls (wall jumps)
            static int stuck, jumpT, lastX100;
            k = g_testWalk > 0 ? KEY_DRIGHT : KEY_DLEFT;
            int x100 = (int)(P.x * 100);
            if (abs(x100 - lastX100) < 2) stuck++; else stuck = 0;
            lastX100 = x100;
            float h; bool gapAhead = P.onGround && !w_raycast(P.x + g_testWalk * 0.9f, P.y + COL_OFF_Y, 0, -1, 4.0f, &h);
            if (jumpT > 0) { k |= KEY_B; jumpT--; }
            else if (P.onGround && (stuck > 4 || gapAhead)) jumpT = 22;
            else if (!P.onGround && (P.wallSlidingL || P.wallSlidingR) && tick % 16 == 0) jumpT = 14;
            if ((tick % 90) == 45 && P.onGround && !jumpT) k |= KEY_Y;      // the odd swing, as players do
        }
        if (dlg_busy()) k = (tick % 20) < 2 ? KEY_A : 0;      // talk through a boss's opening dialogue
        else if (!best && !g_testWalk) goto bot_done;                          // nothing to fight: the test script's buttons (walk into an arena...)
        down = k & ~prevK; up = prevK & ~k; held = k; prevK = k;
        gin = ginPrev; map_game_input(down, held, cp);       // FSMs and the dialogue read the bot's buttons too
        static int lastLog; if (best && tick - lastLog > 120) { lastLog = tick; fprintf(stderr, "[auto] target %s at %.1f,%.1f knight %.1f,%.1f\n", best->name, bx, by, P.x, P.y); }
    bot_done:;
    }
    keysDownCache = down;
    if (!transitioning) {
        // Unity order: FixedUpdate steps (50 Hz) first, then Update
        fixedAcc += dt; if (fixedAcc > 0.1f) fixedAcc = 0.1f;
        while (fixedAcc >= HERO_FIXED_DT) {
            fixedAcc -= HERO_FIXED_DT;
            if (!P.dead) { hero_fixed(); if (hero_slash_active()) do_nail_hits(); }
            if (freezeT <= 0) {
#ifdef __HOST__
                struct timespec a0, a1; clock_gettime(CLOCK_MONOTONIC, &a0);
                uo_fixed(HERO_FIXED_DT); w_edge_walls(false);
                clock_gettime(CLOCK_MONOTONIC, &a1); extern double g_uoProf; g_uoProf += (a1.tv_sec - a0.tv_sec) * 1e6 + (a1.tv_nsec - a0.tv_nsec) / 1e3;
#else
                uo_fixed(HERO_FIXED_DT); w_edge_walls(false);
#endif
                {   // ride moving platforms (lifts, trams, moving walls carry the Knight standing on them)
                    float pdx, pdy;
                    if (P.onGround && uo_platform_delta(P.x - HALF_W, P.x + HALF_W, P.y + COL_OFF_Y - HALF_H, &pdx, &pdy)) hero_set_position(P.x + pdx, P.y + pdy);
                }
            }
        }
        player_frame(dt, down, held, up, cp);
    }
    {   // the Knight's own scripts that may switch his gravity off: all idle?
        UObj *h = uo_hero(); bool idle = true;
        if (h) for (int k = 0; k < h->nfsm; k++) { Fsm *f = h->fsm[k];
            if (f->name && (!strcmp(f->name, "Spell Control") || !strcmp(f->name, "Superdash") || !strcmp(f->name, "Surface Water") || !strcmp(f->name, "Roar Lock")))
                { const char *sn = fsm_state_name(f); if (sn && strcmp(sn, "Inactive") && strcmp(sn, "Detect")) idle = false; } }
        extern void hero_float_watchdog(float, bool); hero_float_watchdog(dt, idle);
        {   // a cast whose script still holds the Knight (velocity every frame, gravity off) after something else already gave him
            // control back: he hangs in the air and only moves sideways. Cancel it the way leaving a room does (Spell Control's
            // LEAVING SCENE: control, animation and gravity back) and say so in the log
            static const char *holding[] = { "Fireball Antic", "Fireball 1", "Fireball 2", "Fireball Recoil", "Wallside?", "Level Check",
                "Level Check 2", "Level Check 3", "Quake Antic", "Q1 Effect", "Q2 Effect", "Quake1 Land", "Q2 Land", "Q2 Pillar", "Quake Finish",
                "Scream Antic1", "Scream Antic2", "SG Antic", "Scream Burst 1", "Scream Burst 2", "Scream Burst 3", "Scream End", "Scream End 2",
                "End Roar", "End Roar 2", "Spell End" };
            static float stuckT; Fsm *sc = NULL;
            if (h) for (int k = 0; k < h->nfsm; k++) if (h->fsm[k]->name && !strcmp(h->fsm[k]->name, "Spell Control")) sc = h->fsm[k];
            const char *sn = sc ? fsm_state_name(sc) : NULL; bool hold = false;
            if (sn) for (unsigned k = 0; k < sizeof holding / sizeof *holding; k++) if (!strcmp(sn, holding[k])) hold = true;
            stuckT = hold && P.acceptingInput && !P.controlRelinquished && !transitioning && !P.dead ? stuckT + dt : 0;
            if (stuckT > 1.0f) {
                hk_log("watchdog: spell state %s still running with the Knight in control (g %.2f): cancelled", sn, P.gravityScale);
#ifdef __HOST__
                fprintf(stderr, "[watchdog-spell %d] %s at %.2f,%.2f\n", uo_frame, sn, P.x, P.y);
#endif
                fsm_event(sc, "LEAVING SCENE"); stuckT = 0;
            }
        }
#ifdef __HOST__
        { const char *ss = NULL; if (h) for (int k = 0; k < h->nfsm; k++) if (h->fsm[k]->name && !strcmp(h->fsm[k]->name, "Spell Control")) ss = fsm_state_name(h->fsm[k]);
          extern void hero_suspect_check(float, const char *, bool);
          hero_suspect_check(dt, ss, (pd_bool("disablePause") || dlg_busy() || menus_busy() || msgs_busy() || shop_busy()) && !paused);
          static bool told; bool pb = (pd_bool("disablePause") || dlg_busy() || menus_busy() || msgs_busy() || shop_busy()) && !paused && P.acceptingInput && !P.controlRelinquished;
          if (pb && !told && getenv("HK_PAUSEWHY")) { told = true; fprintf(stderr, "[pausewhy] pd %d dlg %d menus %d msgs %d shop %d\n", pd_bool("disablePause"), dlg_busy(), menus_busy(), msgs_busy(), shop_busy()); }
          if (!pb) told = false; }
#endif
    }
    if (g_testHeroLog) { static int hl; if (++hl % 30 == 0 || (down & (KEY_A | KEY_B | KEY_START)))
        hk_log("hero P=%.2f,%.2f v=%.2f,%.2f g=%.2f/%.2f ground=%d st=%d input=%d relinq=%d pause=%d keys=%lx", P.x, P.y, P.vx, P.vy,
               P.gravityScale, P.prevGravityScale, P.onGround, (int)P.state, P.acceptingInput, P.controlRelinquished, pd_bool("disablePause"), (unsigned long)held); }
    { float al = fixedAcc / HERO_FIXED_DT; if (al > 1) al = 1; KX = P.px + (P.x - P.px) * al; KY = P.py + (P.y - P.py) * al; }
    if (freezeT > 0) freezeT -= dt;
    else {
        update_enemies(dt);
        masks_update(dt, P.x - HALF_W, P.y + COL_OFF_Y - HALF_H, P.x + HALF_W, P.y + COL_OFF_Y + HALF_H);
        particles_update(dt, cam_x(), cam_y());
#ifdef __HOST__
        { struct timespec a0, a1; clock_gettime(CLOCK_MONOTONIC, &a0);
          uo_update(dt); uo_late_update(dt); w_edge_walls(false); uo_update_loops();
          clock_gettime(CLOCK_MONOTONIC, &a1); extern double g_uoProf; g_uoProf += (a1.tv_sec - a0.tv_sec) * 1e6 + (a1.tv_nsec - a0.tv_nsec) / 1e3;
          static int pf, pn; if (!pn) pn = getenv("HK_PROF") && atoi(getenv("HK_PROF")) > 1 ? atoi(getenv("HK_PROF")) : 600; if (getenv("HK_PROF") && ++pf % pn == 0) { fprintf(stderr, "[prof f%d] uo avg %.1f us/frame (%d objs)\n", uo_frame, g_uoProf / pn, uo_count()); g_uoProf = 0; } }
#else
        uo_update(dt);
        uo_late_update(dt);
        w_edge_walls(false);
        uo_update_loops();
#endif
        update_shade(dt);
        update_geo(dt);
        update_entities(dt);
    }
    dlg_update(dt);
    menus_update(dt);
    msgs_update(dt);
    shop_update(dt);
    if (hudOut) { hudAlpha -= dt / 0.3f; if (hudAlpha < 0) hudAlpha = 0; } else { hudAlpha += dt / 0.3f; if (hudAlpha > 1) hudAlpha = 1; }
#ifdef __HOST__
    if (getenv("HK_CHECK") && roomLoaded && !transitioning && !P.dead) {
        static int frame; frame++;
        if (solid_box(P.x - HALF_W + 0.01f, P.y + COL_OFF_Y - HALF_H + 0.01f, P.x + HALF_W - 0.01f, P.y + COL_OFF_Y + HALF_H - 0.01f))
            fprintf(stderr, "CHECK knight_embedded room=%s f=%d P=%.2f,%.2f v=%.2f,%.2f ground=%d\n", R.id, frame, P.x, P.y, P.vx, P.vy, P.onGround);
        if (on_ground_box(P.x, P.y + COL_OFF_Y, HALF_W, HALF_H) && P.vy <= 0.01f) {   // support narrower than 1/8 of the Knight = snagged on a wall step
            float by = P.y + COL_OFF_Y - HALF_H;
            int n = 0; for (int k = 0; k < 8; k++) { float x = P.x - HALF_W + (k + 0.5f) * (2 * HALF_W / 8); if (solid_box(x - 0.005f, by - 0.06f, x + 0.005f, by)) n++; }
            if (n <= 1) fprintf(stderr, "CHECK knight_microledge room=%s f=%d P=%.2f,%.2f support=%d/8\n", R.id, frame, P.x, P.y, n);
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {
            Enemy *en = &E[i];
            if (!en->alive || en->dying || en->type == 4) continue;
            if (solid_box(en->x - en->hw + 0.01f, en->y - en->hh + 0.01f, en->x + en->hw - 0.01f, en->y + en->hh - 0.01f))
                fprintf(stderr, "CHECK enemy_embedded room=%s f=%d E%d %s at %.2f,%.2f\n", R.id, frame, i, en->lib, en->x, en->y);
        }
    }
#endif
    update_camera(dt);
#ifdef __HOST__
    { static int fc; if (getenv("HK_TRACE") && (++fc % (getenv("HK_TRACEN") ? atoi(getenv("HK_TRACEN")) : 30)) == 0) fprintf(stderr, "t=%d P=(%.2f,%.2f) v=(%.2f,%.2f) ground=%d hp=%d soul=%d geo=%d anim=%s cam=(%.1f,%.1f) E0=(%d %.1f,%.1f f%d a%d)\n", fc, P.x, P.y, P.vx, P.vy, P.onGround, P.health, P.soul, P.geo, P.anim.anim >= 0 ? A.knight.anims[P.anim.anim].name : "-", camx, camy, E[4].alive, E[4].x, E[4].y, anim_cur_frame(&E[4].anim), E[4].anim.anim);
      for (int k = 0; k < MAX_ENEMIES; k++) if (getenv("HK_TRACE") && E[k].alive && E[k].type == 4 && (fc % 30) == 0) fprintf(stderr, "   boss st=%d act=%d pos=(%.1f,%.1f) armor=%d phase=%d anim=%d frame=%d\n", E[k].bstate, E[k].bossActive, E[k].x, E[k].y, E[k].armor, E[k].phase, E[k].anim.anim, anim_cur_frame(&E[k].anim)); }
#endif
    return 0;
}

/* ---------------- drawing ---------------- */
static void draw_ent_image(int i, float alpha) {
    RoomEnt *e = &R.ents[i];
    if (e->image < 0) return;
    // entity images store their absolute top-left in baked px; draw relative to world origin
    float ox = world_to_sx(0), oy = world_to_sy(0) - ES[i].dy * PPU;
    float sc = PPU / (R.S ? R.S : PPU);
    if (ES[i].flash > 0) spr_draw_tint(&R.entset, e->image, ox, oy, sc, sc, C2D_Color32(255, 255, 255, 255), 0.6f);
    else spr_draw(&R.entset, e->image, ox, oy, sc, sc, alpha);
}

static void dbg_rect(float x0, float y0, float x1, float y1, u32 c) {
    float sx0 = world_to_sx(x0), sx1 = world_to_sx(x1), sy0 = world_to_sy(y1), sy1 = world_to_sy(y0);
    C2D_DrawRectSolid(sx0, sy0, 0.9f, sx1 - sx0, 1, c); C2D_DrawRectSolid(sx0, sy1 - 1, 0.9f, sx1 - sx0, 1, c);
    C2D_DrawRectSolid(sx0, sy0, 0.9f, 1, sy1 - sy0, c); C2D_DrawRectSolid(sx1 - 1, sy0, 0.9f, 1, sy1 - sy0, c);
}
static void debug_draw_boxes(void) {
    // collision cells near the camera
    float rs = (float)R.res;
    int cx0 = (int)((camx - 14) * rs), cx1 = (int)((camx + 14) * rs), cy0 = (int)((camy - 9) * rs), cy1 = (int)((camy + 9) * rs);
    for (int y = cy0; y <= cy1; y++)      // one rectangle per horizontal run of solid cells (keeps the draw count low)
        for (int x = cx0; x <= cx1; x++) {
            if (!room_solid_cell(&R, x, y)) continue;
            int x2 = x; while (x2 + 1 <= cx1 && room_solid_cell(&R, x2 + 1, y)) x2++;
            C2D_DrawRectSolid(world_to_sx(x / rs), world_to_sy((y + 1) / rs), 0.85f, (x2 - x + 1) * PPU / rs, PPU / rs, C2D_Color32(255, 0, 0, 70));
            x = x2;
        }
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        u32 c = ent_is_solid(i) ? C2D_Color32(255, 128, 0, 255) : e->kind == EK_SPIKES ? C2D_Color32(255, 0, 0, 255) : C2D_Color32(0, 200, 255, 200);
        if (e->kind == EK_ENEMY || e->kind == EK_HAZARD_RESPAWN || e->kind == EK_HAZARD_RESPAWN_MARKER || e->kind == EK_RESPAWN_MARKER) continue;
        if (!ES[i].alive && (e->kind == EK_BREAKABLE_WALL || e->kind == EK_GEO_ROCK)) continue;
        dbg_rect(e->x - e->w / 2, e->y - e->h / 2 + ES[i].dy, e->x + e->w / 2, e->y + e->h / 2 + ES[i].dy, c);
    }
    uo_draw_debug(dbg_rect);
    for (int i = 0; i < MAX_ENEMIES; i++) if (E[i].alive)
        dbg_rect(E[i].x - E[i].hw, E[i].y - E[i].hh, E[i].x + E[i].hw, E[i].y + E[i].hh, C2D_Color32(0, 255, 0, 255));
    dbg_rect(P.x - HALF_W, P.y + COL_OFF_Y - HALF_H, P.x + HALF_W, P.y + COL_OFF_Y + HALF_H, C2D_Color32(255, 255, 0, 255));
    if (hero_slash_active()) { float a, b, c2, d; hero_nail_box(&a, &b, &c2, &d); dbg_rect(a, b, c2, d, C2D_Color32(255, 255, 255, 255)); }
}

/* ---------------- the Knight's HeroLight ---------------- */
// Knight_0.prefab 'HeroLight': a soft additive glow (haze2, scale 3) at his feet, tinted by the scene's heroLightColor.
// It sits at his depth among the scenery, so it lights what's behind him and never the foreground silhouettes.
// (tools/build_herolight.py: romfs/gfx/herolight.a8, romfs/text/herolight.txt)
static C3D_Tex hlTex; static Tex3DS_SubTexture hlSub; static C2D_Image hlImg; static int hlState;   // 0 untried, 1 ok, -1 missing
static float hlW = 17.5f, hlH = 15.5f, hlOx = 0, hlOy = -0.6f, hlCol[4] = { 1, 1, 1, 0.6f }; static char hlRoom[40];
static void herolight_room(const char *room) {
    if (!strcmp(hlRoom, room)) return;
    snprintf(hlRoom, sizeof hlRoom, "%s", room);
    hlCol[0] = hlCol[1] = hlCol[2] = 1; hlCol[3] = 0.6f; uo_ambient[0] = uo_ambient[1] = uo_ambient[2] = 1;
    size_t sz; char *t = read_file("romfs:/text/herolight.txt", &sz); if (!t) return;
    size_t L = strlen(room);
    for (char *p = t; p && *p; ) {
        char *nl = strchr(p, '\n');
        if (!strncmp(p, "size ", 5)) sscanf(p + 5, "%f %f %f %f", &hlW, &hlH, &hlOx, &hlOy);
        else if (!strncmp(p, room, L) && p[L] == ' ')
            sscanf(p + L + 1, "%f %f %f %f %f %f %f", &hlCol[0], &hlCol[1], &hlCol[2], &hlCol[3], &uo_ambient[0], &uo_ambient[1], &uo_ambient[2]);
        p = nl ? nl + 1 : NULL;
    }
    free(t);
}
static void herolight_draw(void) {
    if (!roomLoaded) return;
    if (hlState == 0) {
        hlState = -1;
        size_t sz; void *buf = read_file("romfs:/gfx/herolight.a8", &sz);
        if (buf && sz >= 128 * 128 && C3D_TexInit(&hlTex, 128, 128, GPU_A8)) {
            memcpy(hlTex.data, buf, 128 * 128); GSPGPU_FlushDataCache(hlTex.data, hlTex.size);
            C3D_TexSetFilter(&hlTex, GPU_LINEAR, GPU_LINEAR); C3D_TexSetWrap(&hlTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            hlSub = (Tex3DS_SubTexture){ 128, 128, 0.0f, 1.0f, 1.0f, 0.0f }; hlImg.tex = &hlTex; hlImg.subtex = &hlSub;
            hlState = 1;
        }
        free(buf);
    }
    if (hlState != 1 || P.dead) return;
    herolight_room(R.id);
    float w = hlW * PPU, h = hlH * PPU;
    float cx = world_to_sx(KX + hlOx), cy = world_to_sy(KY + hlOy);
    C2D_ImageTint tint; u32 c = C2D_Color32f(hlCol[0] > 1 ? 1 : hlCol[0], hlCol[1] > 1 ? 1 : hlCol[1], hlCol[2] > 1 ? 1 : hlCol[2], hlCol[3]);
    for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = 1; }
    gfx_tint_mode(false);         // an alpha-only texture takes its colour from the tint
    gfx_additive(true);
    C2D_DrawImageAt(hlImg, cx - w / 2, cy - h / 2, 0.5f, &tint, w / 128.0f, h / 128.0f);
    gfx_additive(false);
}

#ifdef __HOST__
// HK_SECFILL: screens of fill per part of the frame (GPU cost)
static void secfill(int sec) {
    extern double g_fill; static double last, acc[12]; static int fr;
    if (sec == 0) { if (getenv("HK_SECFILL") && ++fr % 120 == 0) { static const char *nm[] = { "-", "bg rect", "back layers+objs", "plane objs+knight", "front layers", "over layers+particles+uo_last", "darkness", "hud+rest" };
            fprintf(stderr, "[secfill]"); for (int i = 1; i < 8; i++) fprintf(stderr, " %s %.2f |", nm[i], acc[i] / 120 / (800.0 * 240)); fprintf(stderr, "\n"); memset(acc, 0, sizeof acc); }
        last = g_fill; return; }
    acc[sec] += g_fill - last; last = g_fill;
}
#define SECFILL(n) secfill(n)
#else
#define SECFILL(n)
#endif
u32 game_clear_color(void) { return endingT > 0 ? C2D_Color32(0, 0, 0, 255) : C2D_Color32(8, 8, 12, 255); }
void game_draw_top(void) {
    if (endingT > 0) { ending_draw(); return; }
    SECFILL(0);
    uo_draw_begin();
    float sx = 0, sy = 0;
    if (shakeT > 0) { sx = (rand() % 5 - 2) * shakeT * 8; sy = (rand() % 5 - 2) * shakeT * 8; }
    float cx = camx + sx / PPU, cy = camy + sy / PPU;
    float savex = camx, savey = camy; camx = cx; camy = cy;
    // the backdrop colour comes from the screen clear (game_clear_color): a full-screen rectangle here cost the GPU a
    // whole screen of fill every frame
    SECFILL(1);
    room_plane_hook = herolight_draw;
    if (roomLoaded) herolight_room(R.id);          // the scene's light colours, before anything that uses them draws
    if (roomLoaded) room_draw_layers(&R, camx, camy, false);
    SECFILL(2);
    // entities with baked images
    for (int i = 0; i < R.nents && i < MAX_ENTS; i++) {
        RoomEnt *e = &R.ents[i];
        if (e->image < 0) continue;
        if ((e->kind == EK_BREAKABLE_WALL || e->kind == EK_GEO_ROCK || e->kind == EK_COLLAPSE_FLOOR) && !ES[i].alive) continue;
        if (e->kind == EK_CHEST && ES[i].state == 1) {
            int a = spr_anim(&A.enemies, "Chest Opened");
            if (a >= 0) spr_draw(&A.enemies, A.enemies.animFrames[A.enemies.anims[a].first], world_to_sx(e->x), world_to_sy(e->y - e->h / 2), 1, 1, 1);
            continue;
        }
        draw_ent_image(i, 1);
    }
    // geo
    int ga = spr_anim(&A.enemies, "Geo Small Idle");
    for (int i = 0; i < MAX_GEO; i++) if (G[i].alive && ga >= 0) {
        const Anim *an = &A.enemies.anims[ga];
        int f = A.enemies.animFrames[an->first + ((int)(G[i].t * an->fps)) % an->count];
        spr_draw(&A.enemies, f, world_to_sx(G[i].x), world_to_sy(G[i].y), 1, 1, 1);
    }
    // enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *en = &E[i];
        if (!en->alive) continue;
        int f = anim_cur_frame(&en->anim);
        float scx = en->facing > 0 ? -1 : 1;   // HK sprites face left
        float ex = world_to_sx(en->x + en->pox * (en->facing > 0 ? -1 : 1)), ey = world_to_sy(en->y + en->poy);
        if (en->hitFlash > 0) spr_draw_tint(&R.enemyset, f, ex, ey, scx, 1, C2D_Color32(255, 255, 255, 255), 0.8f);
        else spr_draw(&R.enemyset, f, ex, ey, scx, 1, 1);
    }
    if (!g_noFront) masks_draw_range(camx, camy, 0.98f, 1.0f);    // masks just behind the gameplay plane
    particles_draw_range(camx, camy, 0.98f, 1.0f);
    uo_draw(camx, camy, 1);
    if (SH.active) {
        int f = anim_cur_frame(&SH.anim);
        float scx = SH.facing > 0 ? -1 : 1;
        if (SH.flash > 0) spr_draw_tint(&A.enemies, f, world_to_sx(SH.x), world_to_sy(SH.y), scx, 1, C2D_Color32(255, 255, 255, 255), 0.7f);
        else spr_draw(&A.enemies, f, world_to_sx(SH.x), world_to_sy(SH.y), scx, 1, 0.9f);
    }
    // the Knight
    {
        int f = anim_cur_frame(&P.anim);
        float kx = world_to_sx(KX), ky = world_to_sy(KY);
        float scx = P.facing > 0 ? -1 : 1;
        bool blink = P.c.invulnerable && !P.dead && ((int)(P.invulDuration * 12) & 1);
        if (P.focusing) spr_draw_tint(&A.knight, f, kx, ky, scx, 1, C2D_Color32(255, 255, 255, 255), 0.25f + 0.2f * sinf(P.focusTimer * 20));
        else spr_draw(&A.knight, f, kx, ky, scx, 1, blink ? 0.45f : 1);
        if (P.slashing && P.slashFx.anim >= 0 && !P.slashFx.done) {
            int sf = anim_cur_frame(&P.slashFx);
            float ox, oy, ssx, ssy; hero_slash_xf(&ox, &oy, &ssx, &ssy);    // the slash object's place and scale
            spr_draw(&A.knight, sf, world_to_sx(KX + ox), world_to_sy(KY + oy), ssx, ssy, 1);
        }
    }
    particles_draw_range(camx, camy, 1.0f, 1.02f);
    if (!g_noFront) masks_draw_range(camx, camy, 1.0f, 1.02f);     // and just in front of it
    SECFILL(3);
    if (roomLoaded && !g_noFront) room_draw_layers(&R, camx, camy, true);   // the rest go in among the layers
    SECFILL(4);
    particles_draw_over(camx, camy, 1, 13);                                  // dust in the sorting layers above the scenery
    if (roomLoaded && !g_noFront) room_draw_over(&R, camx, camy);            // glows and haze Unity draws over everything
    particles_draw_over(camx, camy, 13, 1 << 30);
    uo_draw_last(camx, camy);
    SECFILL(5);
    {   // darkness (SceneManager.darknessLevel): the vignette around the Knight; the lantern widens it
        extern bool uo_has_vignette(void);
        int dl = roomLoaded && !uo_has_vignette() ? game_darkness_level() : 0;
        if (dl > 0) {
            float r0 = dl >= 2 && !pd_bool("hasLantern") ? 1.2f : 4.5f, r1 = dl >= 2 && !pd_bool("hasLantern") ? 3.2f : 9.0f;
            float cx = world_to_sx(KX), cy = world_to_sy(KY + COL_OFF_Y), a0 = r0 * PPU, a1 = r1 * PPU, far = 900;
            u32 clear = C2D_Color32(0, 0, 0, 0), dark = C2D_Color32(0, 0, 0, dl >= 2 ? 250 : 215);
            const int N = 28;
            for (int i = 0; i < N; i++) {
                float t0 = i * 2 * (float)M_PI / N, t1 = (i + 1) * 2 * (float)M_PI / N;
                float c0 = cosf(t0), s0 = sinf(t0), c1 = cosf(t1), s1 = sinf(t1);
                C2D_DrawTriangle(cx + c0 * a0, cy + s0 * a0, clear, cx + c1 * a0, cy + s1 * a0, clear, cx + c0 * a1, cy + s0 * a1, dark, 0.5f);
                C2D_DrawTriangle(cx + c1 * a0, cy + s1 * a0, clear, cx + c1 * a1, cy + s1 * a1, dark, cx + c0 * a1, cy + s0 * a1, dark, 0.5f);
                C2D_DrawTriangle(cx + c0 * a1, cy + s0 * a1, dark, cx + c1 * a1, cy + s1 * a1, dark, cx + c0 * far, cy + s0 * far, dark, 0.5f);
                C2D_DrawTriangle(cx + c1 * a1, cy + s1 * a1, dark, cx + c1 * far, cy + s1 * far, dark, cx + c0 * far, cy + s0 * far, dark, 0.5f);
            }
        }
    }
    SECFILL(6);
    dlg_draw_world(camx, camy);
#ifdef __HOST__
    if (getenv("HK_BOXES")) dbgBoxes = true;
#endif
    if (dbgBoxes) {
        debug_draw_boxes();
        float sum = 0, mx = 0; for (int i = 0; i < 60; i++) { sum += dbgDt[i]; if (dbgDt[i] > mx) mx = dbgDt[i]; }
        char t[64]; snprintf(t, sizeof t, "fps %.1f  worst %.0fms", sum > 0 ? 60.0f / sum : 0, mx * 1000);
        font_draw(&A.fontText, t, 250, 4, 0.9f, C2D_Color32(255, 255, 0, 255));
    }
    camx = savex; camy = savey;

    // HUD
    if (hudAlpha > 0.01f) {
        int fr = spr_anim(&A.hud, "HUD Frame");
        float hx = 44, hy = 40;
        if (fr >= 0) spr_draw(&A.hud, A.hud.animFrames[A.hud.anims[fr].first + A.hud.anims[fr].count - 1], hx, hy, 1, 1, 1);
        if (P.soul > 0) {   // the soul orb fills from the bottom like the HUD's white liquid (rows of a disc up to the level)
            const float cx = 28.5f, cy = 44.0f, r = 11.5f; float level = cy + r - 2 * r * (P.soul >= 99 ? 1 : P.soul / 99.0f);
            u32 col = C2D_Color32(235, 235, 240, (int)(hudAlpha * (P.soul >= 33 ? 245 : 200)));
            for (float y = cy + r - 1; y >= level - 0.5f && y >= cy - r; y -= 1) {
                float dy = y + 0.5f - cy, hw = r * r - dy * dy; if (hw <= 0) continue; hw = sqrtf(hw);
                C2D_DrawRectSolid(cx - hw, y, 0.5f, hw * 2, 1, col);
            }
        }
        int hi = spr_anim(&A.hud, "Health Idle"), he = spr_anim(&A.hud, "Health Empty");
        for (int i = 0; i < P.maxHealth; i++) {
            int a = i < P.health ? hi : he;
            if (a < 0) continue;
            int f = A.hud.animFrames[A.hud.anims[a].first];
            spr_draw(&A.hud, f, 82 + i * 15, 26, 1, 1, 1);
        }
        if (blueHealth > 0 && hi >= 0) {      // lifeblood masks (blue health_display)
            int f = A.hud.animFrames[A.hud.anims[hi].first];
            for (int i = 0; i < blueHealth; i++)
                spr_draw_tint(&A.hud, f, 82 + (P.maxHealth + i) * 15, 26, 1, 1, C2D_Color32(80, 170, 255, 255), 0.75f);
        }
        int ci = spr_anim(&A.hud, "Coin Idle");
        if (ci >= 0) spr_draw(&A.hud, A.hud.animFrames[A.hud.anims[ci].first], 84, 48, 1, 1, 1);
        char buf[16]; snprintf(buf, sizeof buf, "%d", P.geo);
        font_draw(&A.fontText, buf, 94, 40, 1, C2D_Color32(240, 240, 240, 255));
    }
    uo_draw_hud();
    dlg_draw_hud();
    menus_draw();
    msgs_draw();
    shop_draw();
    if (msgT > 0) font_draw_centered(&A.fontText, msg, TOP_W / 2, 200, 1, C2D_Color32f(1, 1, 1, msgT > 1 ? 1 : msgT));
    if (nearDoor) font_draw_centered(&A.fontText, TR("Up: Enter"), world_to_sx(P.x), world_to_sy(P.y + 1.2f), 1, C2D_Color32(255, 255, 255, 230));
    if (paused) {     // fades in over 0.2 s like the game's; items in Trajan with the menu's fleur pointers
        float pa = pauseT / 0.2f; if (pa > 1) pa = 1;
        C2D_DrawRectSolid(0, 0, 0.9f, TOP_W, TOP_H, C2D_Color32f(0, 0, 0, 0.72f * pa));
        u32 wc = C2D_Color32f(1, 1, 1, pa);
        if (pausePage == 0) {
            const char *pi[3] = { lang_get_or_key("PAUSE_CONTINUE", "MainMenu"), lang_get_or_key("PAUSE_OPTIONS", "MainMenu"), lang_get_or_key("PAUSE_MAIN", "MainMenu") };
            for (int i = 0; i < 3; i++) {
                float y = 88 + i * 24;
                font_draw_centered(&A.fontMenu, pi[i], TOP_W / 2, y, 1, wc);
                if (i == pauseSel) menu_draw_pointers(TOP_W / 2, y + A.fontMenu.lineh / 2, font_width(&A.fontMenu, pi[i]) / 2, pausePtrT);
            }
        } else {
            char a[64], b[64], c[64], d[64];
            snprintf(a, sizeof a, "%s  %d", lang_get_or_key("AUDIO_MUSIC_VOL", "MainMenu"), (int)roundf(g_musicVolume * 10));
            snprintf(b, sizeof b, "%s  %d", lang_get_or_key("AUDIO_SOUND_VOL", "MainMenu"), (int)roundf(g_sfxVolume * 10));
            snprintf(c, sizeof c, "%s  %s", lang_get_or_key("GAME_LANGUAGE", "MainMenu"), lang_name(g_lang));
            snprintf(d, sizeof d, "%s  %s", TR("Button Hints"), lang_get_or_key(g_hints ? "MOH_ON" : "MOH_OFF", "MainMenu"));
            const char *oi[4] = { a, b, c, d };
            font_draw_centered(&A.fontMenu, lang_get_or_key("PAUSE_OPTIONS", "MainMenu"), TOP_W / 2, 52, 1, wc);
            for (int i = 0; i < 4; i++) {
                float y = 92 + i * 26;
                font_draw_centered(&A.fontMenu, oi[i], TOP_W / 2, y, 1, wc);
                if (i == pauseSel) menu_draw_pointers(TOP_W / 2, y + A.fontMenu.lineh / 2, font_width(&A.fontMenu, oi[i]) / 2, pausePtrT);
            }
            if (g_hints) font_draw_centered(&A.fontText, TR("Left/Right: change    B: back"), TOP_W / 2, 200, 0.9f, C2D_Color32f(0.7f, 0.7f, 0.75f, pa));
        }
    }
    inv_draw_top();
    if (fade > 0) C2D_DrawRectSolid(0, 0, 1, TOP_W, TOP_H, C2D_Color32f(0, 0, 0, fade));
    ach_draw_popup();
    SECFILL(7);
}

void game_draw_bottom(void) {
    if (inv_open()) { inv_draw_bottom(); return; }
    C2D_DrawRectSolid(0, 0, 0.1f, BOT_W, BOT_H, C2D_Color32(12, 12, 18, 255));
    if (dbgBoxes) {
        float sum = 0, mx = 0; for (int i = 0; i < 60; i++) { sum += dbgDt[i]; if (dbgDt[i] > mx) mx = dbgDt[i]; }
        char t[64]; snprintf(t, sizeof t, "fps %.1f worst %.0fms objs %d", sum > 0 ? 60.0f / sum : 0, mx * 1000, uo_count());
        font_draw(&A.fontText, t, 4, 222, 0.9f, C2D_Color32(255, 255, 0, 255));
    }
    if (roomLoaded) {
        { const char *zn = lang_get(R.title, "Map Zones"); char zb[48]; snprintf(zb, sizeof zb, "%s", zn ? zn : (map_area_name()[0] ? map_area_name() : R.title));
          for (char *p = zb; *p; p++) if (p[0] == '\\' && p[1] == 'n') { p[0] = ' '; memmove(p + 1, p + 2, strlen(p + 2) + 1); }
          font_draw_centered(&A.fontMenu, zb, BOT_W / 2, 6, 1, C2D_Color32(230, 230, 230, 255)); }
        map_draw_area(8, 30, BOT_W - 16, g_hints ? 170 : 200, 1);     // (taller without the button hints under it)
    }
    if (0 && roomLoaded && R.hasMap) {
        float sc = fminf(300.0f / R.w, 150.0f / R.h);
        float mx = (BOT_W - R.w * sc) / 2, my = 40;
        C2D_DrawImageAt(R.mapImg, mx, my, 0.5f, NULL, sc, sc);
        // player marker (map y is bottom-up)
        float px = mx + P.x * sc, py = my + (R.h - P.y) * sc;
        if (charm_compass()) C2D_DrawRectSolid(px - 2, py - 3, 0.6f, 4, 5, C2D_Color32(255, 255, 255, 255));   // Wayward Compass
        { const char *zn = lang_get(R.title, "Map Zones"); char zb[48]; snprintf(zb, sizeof zb, "%s", zn ? zn : R.title);
          for (char *p = zb; *p; p++) if (p[0] == '\\' && p[1] == 'n') { p[0] = ' '; memmove(p + 1, p + 2, strlen(p + 2) + 1); }
          font_draw_centered(&A.fontMenu, zb, BOT_W / 2, 10, 1, C2D_Color32(230, 230, 230, 255)); }
    }
    if (g_hints) {   // control hints, shrunk to fit the screen when the language's are longer (Options: Button Hints)
        const char *h[2] = { TR("B Jump  Y Nail  A Focus/Cast  R Dash  X Dream Nail"),
                             has_zl_zr() ? TR("ZR Quick Cast  ZL Super Dash  SELECT Charms  START Pause")
                                         : TR("L Super Dash  SELECT Charms  START Pause") };
        for (int i = 0; i < 2; i++) {
            float w = font_width(&A.fontText, h[i]), sc = w * 0.8f > BOT_W - 16 ? (BOT_W - 16) / w : 0.8f;
            font_draw(&A.fontText, h[i], 8, 206 + i * 16, sc, C2D_Color32(150, 150, 160, 255));
        }
    }
    if (fade > 0) C2D_DrawRectSolid(0, 0, 1, BOT_W, BOT_H, C2D_Color32f(0, 0, 0, fade));
}

/* ---------------- reachability tester (host only) ----------------
 * HK_REACH="Room1 Room2 ..." runs a search over the Knight's standing positions from each entrance of each room,
 * using the real update_player() physics: walks of several lengths and jumps of every height with different air
 * steering. Prints which exits can be reached from which entrances. Enemies are removed; breakable walls count as
 * broken (the nail opens them); HK_REACH_DASH=1 also allows the Mothwing Cloak dash. */
#ifdef __HOST__
static u32 simPrev;
static float simAcc;
static void sim_step(u32 held) {
    u32 down = held & ~simPrev, up = simPrev & ~held; simPrev = held;
    circlePosition cp = { 0, 0 };
    simAcc += 1 / 60.0f;
    while (simAcc >= HERO_FIXED_DT) { simAcc -= HERO_FIXED_DT; hero_fixed(); if (hero_slash_active()) do_nail_hits(); }
    HeroInput in = map_input(down, held, up, cp);
    hero_update(1 / 60.0f, &in);
}
static bool sim_on_spikes(void) {
    float x0 = P.x - HURT_HW, x1 = P.x + HURT_HW, y0 = P.y + HURT_OY - HURT_HH, y1 = P.y + HURT_OY + HURT_HH;
    for (int i = 0; i < R.nents; i++) {
        RoomEnt *e = &R.ents[i];
        if (e->kind == EK_SPIKES && box_overlap(x0, y0, x1, y1, e->x - e->w / 2, e->y - e->h / 2, e->x + e->w / 2, e->y + e->h / 2)) return true;
    }
    return P.y < -10;
}
static int sim_gate_touch(void) {
    float px0 = P.x - HALF_W, py0 = P.y + COL_OFF_Y - HALF_H, px1 = P.x + HALF_W, py1 = P.y + COL_OFF_Y + HALF_H;
    for (int i = 0; i < R.nents; i++) {
        RoomEnt *e = &R.ents[i];
        if (e->kind != EK_GATE || !e->target[0] || !gate_enabled(e)) continue;
        if (!strncmp(e->name, "door", 4) && !P.onGround) continue;
        float a, b, c, d; gate_box(e, &a, &b, &c, &d);
        if (box_overlap(px0, py0, px1, py1, a, b, c, d)) return i;
    }
    return -1;
}
static bool sim_bad(void) { return P.dead || P.hazardTimer > 0 || P.c.hazardDeath || P.c.recoiling || P.c.recoilFrozen || sim_on_spikes(); }
static bool sim_grounded(void) { return on_ground_box(P.x, P.y + COL_OFF_Y, HALF_W, HALF_H) && P.vy <= 0.01f; }

#define RMAXS 12000
#define RHASH 32768
static Player rStates[RMAXS]; static int rN;
static int rHashKey[RHASH]; static bool rHashUsed[RHASH];
static void r_reset(void) { rN = 0; memset(rHashUsed, 0, sizeof rHashUsed); }
static bool r_add(void) {
    int kx = (int)floorf(P.x * 2), ky = (int)floorf((P.y + COL_OFF_Y - HALF_H) * 4 + 0.5f);
    int key = (kx + 4096) * 8192 + (ky + 4096);
    unsigned h = ((unsigned)key * 2654435761u) & (RHASH - 1);
    while (rHashUsed[h]) { if (rHashKey[h] == key) return false; h = (h + 1) & (RHASH - 1); }
    if (rN >= RMAXS) return false;
    rHashUsed[h] = true; rHashKey[h] = key; rStates[rN++] = P;
    return true;
}
// run one macro from the current P; returns true if it ended standing (new state candidate). reached[] marks gates.
static bool sim_macro(int kind, int dir, int a, int b, bool *reached) {
    u32 D = dir > 0 ? KEY_DRIGHT : dir < 0 ? KEY_DLEFT : 0;
    simPrev = 0;
    bool left = false;
    int nb = 0, ny = 0, na = 0; bool simPrevB = false;
    for (int f = 0; f < (kind == 3 ? 600 : 240); f++) {
        u32 held = 0;
        if (kind == 0) { if (f < a) held = D; }                                   // walk a frames
        else if (kind == 1) {                                                     // jump: hold B a frames
            if (f < a) held |= KEY_B;
            if (b == 0) held |= D;                                                // steer the whole time
            else if (b == 1 && P.vy > 0) held |= D;                               // steer while rising only
            else if (b == 2 && P.vy <= 0) held |= D;                              // steer while falling only
            else if (b >= 3 && f >= (b - 2) * 4) held |= D;                     // straight up, then drift after 4,8,..,24 frames
        } else if (kind == 3) {                                                   // pogo: jump, steer, down-slash repeatedly
            if (f < a) held |= KEY_B;
            int sm = b / 8, dj = b % 8;                                              // steer mode, dash frame
            if (sm == 0 || (sm == 1 && f >= 10) || (sm == 2 && P.vy <= 0) || (sm == 3 && P.vy > 0)) held |= D;
            if (dj && f == dj * 4) held |= KEY_R;                                   // dash part-way through the jump
            held |= KEY_DDOWN;
            if (!P.onGround && P.vy < 0) {        // slash only when an enemy is under the down-slash box
                float x0 = P.x - 1.4f, x1 = P.x + 1.4f, y0 = P.y - 3.5f, y1 = P.y - 0.7f;
                bool tgt = false;
                for (int k = 0; k < MAX_ENEMIES && !tgt; k++)
                    if (E[k].alive && box_overlap(x0, y0, x1, y1, E[k].x - E[k].hw, E[k].y - E[k].hh, E[k].x + E[k].hw, E[k].y + E[k].hh)) tgt = true;
                for (int k = 0; k < R.nents && !tgt; k++)       // thorns and spikes can be pogoed too
                    if (R.ents[k].kind == EK_SPIKES && box_overlap(x0, y0, x1, y1, R.ents[k].x - R.ents[k].w / 2, R.ents[k].y - R.ents[k].h / 2, R.ents[k].x + R.ents[k].w / 2, R.ents[k].y + R.ents[k].h / 2)) tgt = true;
                if (tgt && !(simPrev & KEY_Y)) held |= KEY_Y;
            }
        } else if (kind == 2) {                                                   // dash (optionally after a jump)
            if (a && f < 11) held |= KEY_B;
            if (f == (a ? 6 : 0)) held |= KEY_R;
            held |= D;
        }
        sim_step(held);
        if (kind == 3 && getenv("HK_REACH_TRACE2") && f % 3 == 0) {
            float dmin = 99; for (int k = 0; k < MAX_ENEMIES; k++) if (E[k].alive) { float d = fabsf(E[k].x - P.x) + fabsf(E[k].y - (P.y - 2)); if (d < dmin) dmin = d; }
            fprintf(stderr, "   b=%d f%d held=%x P=(%.2f,%.2f) v=(%.2f,%.2f) dmin=%.2f\n", b, f, held, P.x, P.y, P.vx, P.vy, dmin); }
        if (kind == 3 && (held & KEY_Y)) ny++;
        if (kind == 3 && P.c.downAttacking) na++;
        if (kind == 3 && P.c.bouncing && !(simPrevB)) { nb++; }
        simPrevB = P.c.bouncing;
        if (sim_bad()) { if (kind == 3 && ny && getenv("HK_REACH_TRACE")) fprintf(stderr, "pogo b=%d ny=%d na=%d bounces=%d BAD at f%d P=(%.2f,%.2f) hz=%.2f\n", b, ny, na, nb, f, P.x, P.y, P.hazardTimer); return false; }
        int g = sim_gate_touch(); if (g >= 0) reached[g] = true;
        bool gr = sim_grounded();
        if (!gr) left = true;
        if ((kind == 0 && f >= a && gr && fabsf(P.vx) < 0.01f && f > a + 2) || (kind != 0 && left && gr && f > 3)) {
            // let any landing lock / attack finish so the stored state can act right away
            for (int k = 0; k < 90 && (P.state == AS_HARD_LANDING || P.c.attacking || P.c.dashing || fabsf(P.vx) > 0.01f); k++) {
                sim_step(0); if (sim_bad()) return false;
                int g2 = sim_gate_touch(); if (g2 >= 0) reached[g2] = true;
            }
            return sim_grounded();
        }
        if (P.y < -20 || P.y > R.h + 20) return false;
    }
    if (kind == 3 && getenv("HK_REACH_TRACE")) fprintf(stderr, "pogo-end ny=%d na=%d nb=%d P=(%.2f,%.2f)\n", ny, na, nb, P.x, P.y);
    return sim_grounded();
}

static void reach_room(const char *room) {
    enter_room(room, "");
    printf("ROOM %s\n", room);
    int gidx[64], ng = 0;
    for (int i = 0; i < R.nents && ng < 64; i++) if (R.ents[i].kind == EK_GATE) gidx[ng++] = i;
    bool dash = getenv("HK_REACH_DASH") != NULL;
    char gname[64][32], gtarget[64][32];
    for (int k = 0; k < ng; k++) { strncpy(gname[k], R.ents[gidx[k]].name, 31); gname[k][31] = 0; strncpy(gtarget[k], R.ents[gidx[k]].target, 31); gtarget[k][31] = 0; }
    printf("  GATES:"); for (int k = 0; k < ng; k++) printf(" %s->%s", gname[k], gtarget[k][0] ? gtarget[k] : "(none)"); printf("\n");
    bool tut = !strcmp(room, "Tutorial_01");
    if (tut && ng < 63) { gidx[ng] = -1; strcpy(gname[ng], "spawn"); gtarget[ng][0] = 0; ng++; }
    for (int gi = 0; gi < ng; gi++) {
        enter_room(room, gidx[gi] < 0 ? "" : gname[gi]);
        if (gidx[gi] < 0) { P.vy = -5; }
        bool pogo = getenv("HK_REACH_POGO") != NULL;
        for (int i = 0; i < MAX_ENEMIES; i++) { if (E[i].alive && E[i].type == 5) pogo = true; else E[i].alive = false; }
        for (int i = 0; i < R.nents && i < MAX_ENTS; i++)
            if (R.ents[i].kind == EK_BREAKABLE_WALL || (R.ents[i].kind == EK_COLLAPSE_FLOOR && (getenv("HK_REACH_DROPFLOORS") ||
                (!strncmp(R.ents[i].name, "Break Floor", 11) && R.ents[i].h > R.ents[i].w))))     // vertical "Break Floor" = breakable wall
                ES[i].alive = false;    // the nail breaks these
        for (int i = 0; i < R.nents && i < MAX_ENTS; i++)
            if (R.ents[i].kind == EK_VINE) { ES[i].state = 2; ES[i].dy = -(float)atof(R.ents[i].target); }   // vines cut down
        P.hasDash = dash; P.c.invulnerable = true; P.invulRoutine = false; simAcc = 0;
        bool reached[MAX_ENTS]; memset(reached, 0, sizeof reached);
        // settle: let the entry walk-in / launch play out
        simPrev = 0;
        for (int f = 0; f < 400 && !(f > 30 && sim_grounded() && P.trans == TS_WAITING_TO_TRANSITION && P.state != AS_NO_INPUT); f++) { sim_step(0); int g = sim_gate_touch(); if (g >= 0 && g != gidx[gi]) reached[g] = true; }
        r_reset();
        clock_t t0 = clock();
        if (!sim_grounded()) { printf("  ENTRY %s: never lands (%.2f,%.2f)\n", gname[gi], P.x, P.y); continue; }
        r_add();
        for (int s = 0; s < rN; s++) {
            static const int walks[] = { 1, 3, 8, 16, 32 };
            static const int holds[] = { 3, 8, 14, 40 };    // 40 = held through the whole rise (full height)
            static const int modes[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };   // steer always / rising / falling / after 4..24 frames
            if (rN > 300 && (double)(clock() - t0) / CLOCKS_PER_SEC > 60) break;   // big rooms: cap the search at ~60 s CPU per entrance
            for (int dir = -1; dir <= 1; dir++) {
                if (dir) for (int w = 0; w < 5; w++) { P = rStates[s]; if (sim_macro(0, dir, walks[w], 0, reached)) r_add(); }
                for (int h = 0; h < 4; h++) for (int mi = 0; mi < 9; mi++) {
                    int m = modes[mi];
                    if (!dir && mi) continue;
                    P = rStates[s]; if (sim_macro(1, dir, holds[h], m, reached)) r_add();
                }
                if (pogo && dir) for (int h = 0; h < 2; h++) for (int sm = 0; sm < 4; sm++) for (int dj = 0; dj < (dash ? 7 : 1); dj++) { P = rStates[s]; P.dashCooldownTimer = 0; P.airDashed = false; if (sim_macro(3, dir, h ? 40 : 8, sm * 8 + dj, reached)) r_add(); }
                if (dash && dir) for (int j = 0; j < 2; j++) { P = rStates[s]; P.dashCooldownTimer = 0; P.airDashed = false; if (sim_macro(2, dir, j, 0, reached)) r_add(); }
            }
        }
        if (getenv("HK_REACH_DUMP")) {
            char fn[128]; snprintf(fn, sizeof fn, "%s/%s_%s.txt", getenv("HK_REACH_DUMP"), room, gname[gi]);
            FILE *df = fopen(fn, "w"); if (df) { for (int k = 0; k < rN; k++) fprintf(df, "%.2f %.2f\n", rStates[k].x, rStates[k].y + COL_OFF_Y - HALF_H); fclose(df); }
        }
        printf("  ENTRY %-12s at (%.1f,%.1f) states=%d reaches:", gname[gi], rStates[0].x, rStates[0].y, rN);
        for (int k = 0; k < ng; k++) if (gidx[k] >= 0 && reached[gidx[k]] && k != gi) printf(" %s", gname[k]);
        printf("\n");
        fflush(stdout);
    }
}

// HK_CAMTOUR="Room Room ...": park the Knight on standable spots across each room, let the camera settle, and
// measure how much of the screen the foreground layers turn dark. Writes <dir>/<room>_<x>_<y>.png for bad views.
void game_camtour_main(void) {
    extern float (*host_fbtop(void))[400][3]; extern void host_force_draw(int on); extern void host_write_top_png(const char *path);
    char list[4096]; strncpy(list, getenv("HK_CAMTOUR"), sizeof list - 1); list[sizeof list - 1] = 0;
    const char *dir = getenv("HK_CAMTOUR_OUT") ? getenv("HK_CAMTOUR_OUT") : "shots";
    static float with[240][400], without[240][400];
    reset_player();
    char *sv; for (char *room = strtok_r(list, " ", &sv); room; room = strtok_r(NULL, " ", &sv)) {
        enter_room(room, "");
        if (!roomLoaded) continue;
        float cand[256][2]; int nc = 0;
        for (float x = 2; x < R.w - 2 && nc < 256; x += 5)
            for (float y = R.h - 2; y > 1 && nc < 256; y -= 0.25f) {
                float cy = y + COL_OFF_Y;
                if (!solid_box(x - HALF_W, cy - HALF_H, x + HALF_W, cy + HALF_H) && solid_box(x - HALF_W, cy - HALF_H - 0.15f, x + HALF_W, cy - HALF_H)) {
                    cand[nc][0] = x; cand[nc][1] = y; nc++; y -= 3;
                }
            }
        int worst = 0;
        {   int mx = getenv("HK_CAMTOUR_MAX") ? atoi(getenv("HK_CAMTOUR_MAX")) : 0;     // at most this many spots, spread out
            if (mx > 0 && nc > mx) { for (int k = 0; k < mx; k++) { int s = (int)((k + 0.5f) * nc / mx); cand[k][0] = cand[s][0]; cand[k][1] = cand[s][1]; } nc = mx; } }
        for (int k = 0; k < nc; k++) {
            test_place(cand[k][0], cand[k][1]);
            P.c.transitioning = false; fade = 0; fadeTarget = 0; transitioning = false;
            circlePosition cp = { 0, 0 };
            for (int f = 0; f < 45; f++) game_update(1.0f / 60, 0, 0, 0, cp);
            if (P.dead || transitioning) continue;
            float (*fb)[400][3] = host_fbtop();
            host_force_draw(1);
            g_noFront = true; game_draw_top();
            for (int y = 0; y < 240; y++) for (int x = 0; x < 400; x++) without[y][x] = (fb[y][x][0] + fb[y][x][1] + fb[y][x][2]) / 3;
            g_noFront = false; game_draw_top();
            for (int y = 0; y < 240; y++) for (int x = 0; x < 400; x++) with[y][x] = (fb[y][x][0] + fb[y][x][1] + fb[y][x][2]) / 3;
            int dark = 0, nearK = 0, nearN = 0;
            float kx = world_to_sx(P.x), ky = world_to_sy(P.y + COL_OFF_Y);
            for (int y = 0; y < 240; y++) for (int x = 0; x < 400; x++) {
                bool d = with[y][x] < 0.05f && without[y][x] > 0.12f;   // a foreground layer blacked out visible art
                if (d) dark++;
                if (fabsf(x - kx) < 40 && fabsf(y - ky) < 25) { nearN++; if (d) nearK++; }
            }
            float fs = dark / 96000.0f, fk = nearN ? nearK / (float)nearN : 0;
            // holes: screen left at the clear colour (no art at all there); seams: long dead-straight brightness steps
            // (a glow or a layer cut off by its tile or canvas edge). The HUD corner doesn't count.
            int clear = 0, hrun = 0, vrun = 0;
            for (int y = 0; y < 240; y++) for (int x = 0; x < 400; x++) {
                if (y < 64 && x < 150) continue;
                if (fabsf(fb[y][x][0] - 8 / 255.0f) < 0.004f && fabsf(fb[y][x][1] - 8 / 255.0f) < 0.004f && fabsf(fb[y][x][2] - 12 / 255.0f) < 0.004f) clear++;
            }
            for (int y = 1; y < 240; y++) { int run = 0, sg = 0;
                for (int x = 0; x < 400; x++) { float d = with[y][x] - with[y - 1][x]; int s = d > 0.03f ? 1 : d < -0.03f ? -1 : 0;
                    if (s && s == sg && !(y < 64 && x < 150)) run++; else run = s ? 1 : 0; sg = s; if (run > hrun) hrun = run; } }
            for (int x = 1; x < 400; x++) { int run = 0, sg = 0;
                for (int y = 0; y < 240; y++) { float d = with[y][x] - with[y][x - 1]; int s = d > 0.03f ? 1 : d < -0.03f ? -1 : 0;
                    if (s && s == sg && !(y < 64 && x < 150)) run++; else run = s ? 1 : 0; sg = s; if (run > vrun) vrun = run; } }
            float fc = clear / 96000.0f;
            printf("CAM %s %.1f %.1f screen=%.3f knight=%.3f cam=(%.1f,%.1f) clear=%.3f hrun=%d vrun=%d\n", room, cand[k][0], cand[k][1], fs, fk, camx, camy, fc, hrun, vrun);
            static int savedAll; static char lastRoom[40];
            if (strcmp(lastRoom, room)) { snprintf(lastRoom, sizeof lastRoom, "%s", room); savedAll = 0; }
            if (getenv("HK_CAMTOUR_ALL") && savedAll < atoi(getenv("HK_CAMTOUR_ALL")) && (k % 3 == 1 || nc < 3)) {
                char fn[160]; snprintf(fn, sizeof fn, "%s/all_%s_%.0f_%.0f.png", dir, room, cand[k][0], cand[k][1]);
                host_write_top_png(fn); savedAll++;
            }
            if ((fs > 0.15f || fk > 0.35f || fc > 0.03f || hrun > 90 || vrun > 90) && worst < 4) {
                char fn[160]; snprintf(fn, sizeof fn, "%s/%s_%.0f_%.0f.png", dir, room, cand[k][0], cand[k][1]);
                host_write_top_png(fn); worst++;
            }
            host_force_draw(0);
        }
        fflush(stdout);
    }
}

void game_reach_main(void) {
    char list[2048]; strncpy(list, getenv("HK_REACH"), sizeof list - 1); list[sizeof list - 1] = 0;
    reset_player();
    char *sv; for (char *t = strtok_r(list, " ", &sv); t; t = strtok_r(NULL, " ", &sv)) reach_room(t);
}
#endif

bool game_any_key_down(void) { return keysDownCache != 0; }
