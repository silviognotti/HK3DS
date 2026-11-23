int hk_zlib_uncompress(void *out, unsigned int *outLen, const void *src, unsigned int srcLen);
#include "room.h"
#include "masks.h"
#include "particles.h"
#include "uo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ext/tinf.h"

#define RD(T, p) ({ T _v; memcpy(&_v, p, sizeof(T)); p += sizeof(T); _v; })

/* ------------- tile cache (layer tiles are RLE-compressed in RAM, decoded into GPU textures on demand) ------------- */
// 128x128 tiles: a screen of a layer needs about half the memory it did with 256x256 tiles (less of each tile lies
// off screen), so an Old 3DS's cache holds a whole screen of King's Pass (18 layers) again
#define CACHE_TILE 128
#ifdef HK_HD
#define CACHE_N 640           // HD art: twice the texels across, so a screen needs about twice the tiles
#else
#define CACHE_N 400
#endif
// soft layers (fog, haze, glows: tools/bake_scene.py SOFT_NAMES) are 32-bit at half resolution: their own, smaller cache
#ifdef HK_HD
#define CACHE8_N 112
#else
#define CACHE8_N 64
#endif
typedef struct { C3D_Tex tex; const Room *room; int layer, idx; u32 used; bool ok; C2D_Image img; Tex3DS_SubTexture sub; } CacheEnt;
static CacheEnt cache[CACHE_N], cache8[CACHE8_N];
static int cache8N = CACHE8_N;
static u32 cacheClock, frameStart;
static int decodeBudget = 99; static u64 decodeTicks; static int decodedNow;   // tile unpacking per frame: count and time caps
static bool primeNext, primeNow;     // the first frame of a room (drawn under the black screen): unpack all it shows
static bool cullOff;                 // tiles drawn whole (test runs: sdmc:/3ds/hk3ds/test_nocull.txt, HK_NOCULL on the PC)
static bool tileClassic;             // test runs (sdmc:/3ds/hk3ds/test_tiles_classic.txt): neither of these, to compare

// per-room tile statistics for the SD log (written when the room is left): what a real console does with the art
static char stRoom[32]; static int stFrames, stMissFrames, stMissWorst, stMissNow, stMain, stWorker; static u64 stMainTicks;

/* ------------- unpacking on another CPU core (HD build) ------------- */
// The HD art needs twice the tiles of the standard art for a screen, and a real New 3DS unpacks them slower than the
// emulator does: with only the main thread's few milliseconds a frame, tiles came in late on a New 2DS XL, all over
// the screen on room entry and at the edges while scrolling. A worker thread on the New 3DS's third core (core 2; if
// that can't be had, on the main core at the lowest priority, where it gets the time the main thread spends waiting
// for the GPU and the screen) unpacks the tiles the main thread has no time for into buffers of its own. The main
// thread copies a finished one into a cache texture when it's next asked for: only the main thread writes textures.
#if defined(HK_HD) && !defined(__HOST__)
#define TW 1
int hk_zlib_uncompress_with(void *d, void *out, unsigned int *outLen, const void *src, unsigned int srcLen);
void *hk_inflater_new(void);
#define TW_SLOTS 16
#define TW_MAXTILE 128
enum { TW_FREE, TW_QUEUED, TW_WORKING, TW_READY };
typedef struct { volatile int state; const Room *room; int layer, idx; const u8 *src; u32 srcLen, outLen, seq, frame; u8 *buf; } TwSlot;
static TwSlot tw[TW_SLOTS];
static LightLock twLock; static LightEvent twEvent; static Thread twThread; static volatile bool twQuit;
static u32 twSeq, twFrame;

static void tw_main(void *arg) {
    (void)arg;
    void *inf = hk_inflater_new();
    while (!twQuit) {
        TwSlot *job = NULL;
        LightLock_Lock(&twLock);
        for (int i = 0; i < TW_SLOTS; i++) if (tw[i].state == TW_QUEUED && (!job || tw[i].seq < job->seq)) job = &tw[i];
        if (job) job->state = TW_WORKING;
        LightLock_Unlock(&twLock);
        if (!job) { LightEvent_Wait(&twEvent); continue; }
        unsigned int outLen = job->outLen;
        if (hk_zlib_uncompress_with(inf, job->buf, &outLen, job->src, job->srcLen) != 0) memset(job->buf, 0, job->outLen);
        LightLock_Lock(&twLock);
        job->state = TW_READY;
        LightLock_Unlock(&twLock);
    }
}

static void tw_start(void) {
    for (int i = 0; i < TW_SLOTS; i++) { tw[i].buf = malloc(TW_MAXTILE * TW_MAXTILE * 4); if (!tw[i].buf) { hk_log("tile worker: no memory"); return; } }
    LightLock_Init(&twLock); LightEvent_Init(&twEvent, RESET_ONESHOT);
    s32 prio = 0x30; svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    int core = 2;
    twThread = threadCreate(tw_main, NULL, 32 * 1024, prio, 2, false);
    if (!twThread) { core = 0; twThread = threadCreate(tw_main, NULL, 32 * 1024, 0x3F, -2, false); }
    hk_log("tile worker: %s", twThread ? (core == 2 ? "core 2" : "main core, lowest priority") : "none");
}

// before a room's data is freed or the cache changes: drop the queued jobs and wait for the one being unpacked
static void tw_drain(void) {
    if (!twThread) return;
    for (;;) {
        bool working = false;
        LightLock_Lock(&twLock);
        for (int i = 0; i < TW_SLOTS; i++) { if (tw[i].state == TW_WORKING) working = true; else tw[i].state = TW_FREE; }
        LightLock_Unlock(&twLock);
        if (!working) break;
        svcSleepThread(200000);
    }
}

static TwSlot *tw_find(const Room *r, int li, int idx) {
    for (int i = 0; i < TW_SLOTS; i++) { TwSlot *s = &tw[i]; if (s->state != TW_FREE && s->room == r && s->layer == li && s->idx == idx) return s; }
    return NULL;
}

static bool tw_queue(Room *r, int li, int idx, int bpp) {
    if (r->tile > TW_MAXTILE) return false;
    Layer *L = &r->layers[li];
    TwSlot *f = NULL;
    LightLock_Lock(&twLock);
    for (int i = 0; i < TW_SLOTS && !f; i++) if (tw[i].state == TW_FREE) f = &tw[i];
    // all busy: take over the oldest job nobody has asked for in a third of a second (the camera moved on)
    if (!f) for (int i = 0; i < TW_SLOTS; i++)
        if ((tw[i].state == TW_QUEUED || tw[i].state == TW_READY) && twFrame - tw[i].frame > 20 && (!f || tw[i].frame < f->frame)) f = &tw[i];
    if (f) {
        f->room = r; f->layer = li; f->idx = idx; f->src = r->blob + L->tileOff[idx]; f->srcLen = L->tileSize[idx];
        f->outLen = r->tile * r->tile * bpp; f->seq = ++twSeq; f->frame = twFrame; f->state = TW_QUEUED;
    }
    LightLock_Unlock(&twLock);
    if (f) LightEvent_Signal(&twEvent);
    return f != NULL;
}

static bool tw_room(void) {      // a free slot to queue into (the prefetch stops when there's none)
    for (int i = 0; i < TW_SLOTS; i++) if (tw[i].state == TW_FREE) return true;
    return false;
}
#endif

// the tile textures are allocated once at boot, while GPU memory is still in one piece: allocated on demand later they
// could fail when memory got fragmented, and those tiles (often the foreground shadows) stayed missing until a restart
static int cacheAlloc, cacheN = CACHE_N;
#ifdef __HOST__
int g_tileMiss;
#endif
void room_tilecache_init(void) {
#ifdef __HOST__
    if (getenv("HK_TILECACHE")) cacheN = atoi(getenv("HK_TILECACHE"));     // emulate an Old 3DS's 60
#endif
#ifndef __HOST__
    // Old 3DS: about half the GPU memory. Fewer cached tiles (they are re-unpacked more often instead): 136 of 16 bits
    // and 52 of 32 bits (the far background and soft art), 7.5 MB, what 60 tiles of 256x256 took. Those 60 big tiles
    // couldn't hold a screen of King's Pass (18 layers): about a dozen tiles a frame, the far background and the nearest
    // foreground, went missing; this holds a screen of King's Pass, Greenpath and the Crossroads (tools: HK_TILETRACE).
    // Sprite pages that need the memory later take 16-bit tiles back (room_tilecache_release)
    if (linearSpaceFree() < (40u << 20)) { cacheN = 136; cache8N = 52; }
    else { extern bool g_lowmem; if (g_lowmem) { cacheN = 152; cache8N = 56; } }     // a New 3DS running in 80/96 MB
#endif
    for (cacheAlloc = 0; cacheAlloc < cacheN; cacheAlloc++) {
        CacheEnt *c = &cache[cacheAlloc];
        if (!C3D_TexInit(&c->tex, CACHE_TILE, CACHE_TILE, GPU_RGBA4)) break;
        C3D_TexSetFilter(&c->tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&c->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        c->sub = (Tex3DS_SubTexture){ CACHE_TILE, CACHE_TILE, 0.0f, 1.0f, 1.0f, 0.0f };
        c->img.tex = &c->tex; c->img.subtex = &c->sub; c->ok = true; c->room = NULL; c->layer = -1;
    }
    hk_log("tile cache: %d of %d tiles allocated", cacheAlloc, cacheN);
    cacheN = cacheAlloc;
#ifdef __HOST__
    if (getenv("HK_TILECACHE8")) cache8N = atoi(getenv("HK_TILECACHE8"));
#endif
    int a8 = 0;
    for (; a8 < cache8N; a8++) {
        CacheEnt *c = &cache8[a8];
        if (!C3D_TexInit(&c->tex, CACHE_TILE, CACHE_TILE, GPU_RGBA8)) break;
        C3D_TexSetFilter(&c->tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&c->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        c->sub = (Tex3DS_SubTexture){ CACHE_TILE, CACHE_TILE, 0.0f, 1.0f, 1.0f, 0.0f };
        c->img.tex = &c->tex; c->img.subtex = &c->sub; c->ok = true; c->room = NULL; c->layer = -1;
    }
    hk_log("soft tile cache: %d of %d tiles", a8, cache8N);
    cache8N = a8;
#ifndef __HOST__
    { FILE *f = fopen("sdmc:/3ds/hk3ds/test_tiles_classic.txt", "rb"); if (f) { fclose(f); tileClassic = true; hk_log("tiles: classic (test)"); } }
    { FILE *f = fopen("sdmc:/3ds/hk3ds/test_nocull.txt", "rb"); if (f) { fclose(f); cullOff = true; hk_log("tiles: drawn whole (test)"); } }
#endif
#ifdef TW
    if (!tileClassic) tw_start();
#endif
}
// sprite pages that can't get GPU memory (after the cached sounds are dropped) take it from the layer tile cache: a missing
// enemy or effect is worse than a background tile being unpacked again more often. Keeps at least 80 tiles.
bool room_tilecache_release(void) {
    if (cacheN <= 80) return false;
#ifdef TW
    tw_drain();
#endif
    cacheN--;
    if (cache[cacheN].ok) { C3D_TexDelete(&cache[cacheN].tex); cache[cacheN].ok = false; }
    cache[cacheN].room = NULL; cache[cacheN].layer = -1;
    static int logged; if (logged++ < 3) hk_log("tile cache down to %d tiles for sprites", cacheN);
    return true;
}
void room_tilecache_reset(void) {
#ifdef TW
    tw_drain();
#endif
    if (stRoom[0] && stFrames > 0)
        hk_log("tiles %s: %d frames, %d with tiles missing (worst %d), unpacked %d here (%.2f ms each) + %d by the worker", stRoom,
               stFrames, stMissFrames, stMissWorst, stMain, stMain ? stMainTicks / 268111.856 / stMain : 0.0, stWorker);
    stRoom[0] = 0; stFrames = stMissFrames = stMissWorst = stMain = stWorker = 0; stMainTicks = 0;
    primeNext = !tileClassic;
    for (int i = 0; i < cacheN; i++) { cache[i].room = NULL; cache[i].layer = -1; }
    for (int i = 0; i < cache8N; i++) { cache8[i].room = NULL; cache8[i].layer = -1; }
}

static void rle_decode(const u8 *src, u32 size, u16 *dst, int n) {
    const u8 *end = src + size;
    int o = 0;
    while (src < end && o < n) {
        u16 h; memcpy(&h, src, 2); src += 2;
        if (h & 0x8000) {
            int c = h & 0x7FFF; u16 v; memcpy(&v, src, 2); src += 2;
            while (c-- && o < n) dst[o++] = v;
        } else {
            int c = h;
            if (o + c > n) c = n - o;
            memcpy(dst + o, src, c * 2); src += h * 2; o += c;
        }
    }
}

/* ------------- what's drawn of each tile ------------- */
// The art is drawn back to front, up to 18 layers deep, as 128x128 tiles: a frame painted the screen 10 to 17 times
// over, mostly with the transparent parts of tiles and with art the black terrain and foreground silhouettes drawn later
// cover. The HD build's 800-pixel screen doubles that, and on a real New 3DS the GPU then took ~25-30 ms a frame (30
// fps). Each tile is now drawn as up to a few pieces covering only its 16x16-texel blocks that have something visible
// and aren't hidden: each frame the layers are walked front to back once, adding the fully opaque blocks of each
// (and solid-colour tiles) to a coverage map of the screen, so a block wholly behind them is left out. Only
// transparent texels and texels that get painted over are skipped, so the picture is the same.
#define OCC_C 4                      // coverage cells of 4x4 screen units (citro2d's 400x240)
#define OCC_W (400 / OCC_C)
#define OCC_H (240 / OCC_C)
#define ALLB 0xFFFFFFFFFFFFFFFFull
static u32 occ[OCC_H][4];
#ifdef __HOST__
long g_ct[6];
#endif

static u64 grow8(u64 m) {          // add the 8 neighbours of each block (within the tile)
    const u64 C0 = 0x0101010101010101ull, C7 = 0x8080808080808080ull;
    u64 h = m | ((m << 1) & ~C0) | ((m >> 1) & ~C7);
    return h | (h << 8) | (h >> 8);
}
// a texel next to a block is blended into it by bilinear filtering: an empty block next to art can show a sliver of
// it, and an opaque block next to transparency isn't fully opaque at its edge. Outside the tile the texture is
// clamped (no neighbour's texels come in), so the tile's own edges need no margin.
static void learn_blocks(Room *r, Layer *L, int idx, const void *data, bool c32) {
    int T = r->tile, gt = T / 8;
    u64 o = 0, e = 0;
    if (T == 128) for (int by = 0; by < 8; by++) for (int bx = 0; bx < 8; bx++) {
        unsigned amin = 255, amax = 0;
        for (int ty = 0; ty < 2; ty++) for (int tx = 0; tx < 2; tx++) {
            int base = ((by * 2 + ty) * gt + (bx * 2 + tx)) * 64;
            if (c32) { const u8 *t = (const u8 *)data + base * 4;     // RGBA8 texels: bytes A B G R
                for (int k = 0; k < 64; k++) { unsigned a = t[k * 4]; if (a < amin) amin = a; if (a > amax) amax = a; } }
            else { const u16 *t = (const u16 *)data + base;            // RGBA4: alpha in the low nibble
                for (int k = 0; k < 64; k++) { unsigned a = (t[k] & 0xF) * 17; if (a < amin) amin = a; if (a > amax) amax = a; } }
        }
        if (amax > 0) e |= 1ull << (by * 8 + bx);
        if (amin == 255) o |= 1ull << (by * 8 + bx);
    } else { e = ALLB; o = 0; }     // other tile sizes: drawn whole, never counted as covering
    L->nem[idx] = grow8(e); L->opq[idx] = ~grow8(~o); L->known[idx] = 1;
}

static inline u32 occ_bits(int a, int b, int w) {    // bits [a, b) of word w (32 bits each)
    int lo = a - w * 32, hi = b - w * 32;
    if (lo < 0) lo = 0; if (hi > 32) hi = 32;
    if (hi <= lo) return 0;
    return (hi - lo == 32) ? 0xFFFFFFFFu : (((1u << (hi - lo)) - 1) << lo);
}
static inline int ifloor(float v) { int i = (int)v; return i - (v < (float)i); }
static inline int iceil(float v) { int i = (int)v; return i + (v > (float)i); }
static inline bool bits_all(const u32 *M, int a, int b) {     // cells [a, b) all set
    for (int w = a >> 5; w <= (b - 1) >> 5; w++) { u32 m = occ_bits(a, b, w); if ((M[w] & m) != m) return false; }
    return true;
}

static bool queueOnly;     // the prefetch with a worker thread: only queue tiles for it, never unpack them here
static CacheEnt *tile_get(Room *r, int li, int idx) {
    cacheClock++;
    // a tile already in the cache: its entry is remembered per tile (a search of the 640-entry cache for each of the
    // ~200 tiles on screen, twice a frame, cost the main thread a millisecond or two)
    CacheEnt *h = r->layers[li].cep ? r->layers[li].cep[idx] : NULL;
    if (h && h->room == r && h->layer == li && h->idx == idx) { h->used = cacheClock; return h; }
    CacheEnt *lru = NULL;
    bool c32 = r->layers[li].fmt == 0;                 // a 32-bit (soft) layer
    CacheEnt *pool = c32 ? cache8 : cache; int pn = c32 ? cache8N : cacheN;
    int bpp = c32 ? 4 : 2;
    // spread tile unpacking over frames: at most a few tiles and ~4 ms a frame (always one, so scrolling never stalls);
    // with the worker thread the main thread keeps ~1.5 ms of that and the worker does the rest. A room's first frame
    // unpacks everything it shows (the screen is still black)
#ifdef TW
    u64 cap = twThread ? 268111856ULL / 666 : 268111856ULL / 250;
#else
    u64 cap = 268111856ULL / 250;
#endif
    bool now = !r->zipped || primeNow || (!queueOnly && !(decodeBudget <= 0 || (decodedNow > 0 && decodeTicks > cap)));
    const u8 *ready = NULL;
#ifdef TW
    TwSlot *ts = NULL;
    if (r->zipped && twThread) {
        ts = tw_find(r, li, idx);
        if (ts && ts->state == TW_READY) ready = ts->buf;          // the worker has unpacked it: copy it in
        else if (ts) { ts->frame = twFrame; return NULL; }        // still queued or being unpacked
        else if (!now) { tw_queue(r, li, idx, bpp); return NULL; }
    }
#endif
    if (!ready && !now) return NULL;
    for (int i = 0; i < pn; i++) {
        CacheEnt *c = &pool[i];
        if (c->room == r && c->layer == li && c->idx == idx) { c->used = cacheClock; return c; }
        if (c->used > frameStart && c->room) continue;   // already drawn this frame: don't overwrite it
        if (!lru || c->used < lru->used) lru = c;
    }
    if (!lru) return NULL;
    Layer *L = &r->layers[li];
    if (lru->ok && lru->tex.width != r->tile) { C3D_TexDelete(&lru->tex); lru->ok = false; }   // a room with another tile size
    if (!lru->ok) {
        lru->ok = C3D_TexInit(&lru->tex, r->tile, r->tile, c32 ? GPU_RGBA8 : GPU_RGBA4);
        if (!lru->ok) { static int logged; if (logged++ < 5) hk_log("TILE TEX ALLOC FAIL (%d)", r->tile); }
        if (!lru->ok) return NULL;
        C3D_TexSetFilter(&lru->tex, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&lru->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        lru->sub = (Tex3DS_SubTexture){ r->tile, r->tile, 0.0f, 1.0f, 1.0f, 0.0f };   // tiles are stored top row first (tex3ds layout)
        lru->img.tex = &lru->tex; lru->img.subtex = &lru->sub;
    }
    int n = r->tile * r->tile;
    if (ready) {
        memcpy(lru->tex.data, ready, n * bpp);
#ifdef TW
        ts->state = TW_FREE;
#endif
        stWorker++;
    } else if (r->zipped) {
        unsigned int outLen = n * bpp;
        decodeBudget--; decodedNow++;
        u64 t0 = svcGetSystemTick();
        if (hk_zlib_uncompress(lru->tex.data, &outLen, r->blob + L->tileOff[idx], L->tileSize[idx]) != TINF_OK) memset(lru->tex.data, 0, n * bpp);
        u64 dt = svcGetSystemTick() - t0;
        decodeTicks += dt; stMain++; stMainTicks += dt;
    } else rle_decode(r->blob + L->tileOff[idx], L->tileSize[idx], (u16 *)lru->tex.data, n * bpp / 2);
    GSPGPU_FlushDataCache(lru->tex.data, n * bpp);
    if (L->known && !L->known[idx]) learn_blocks(r, L, idx, lru->tex.data, c32);
    lru->room = r; lru->layer = li; lru->idx = idx; lru->used = cacheClock;
    if (L->cep) L->cep[idx] = lru;
    return lru;
}

/* ------------- loading ------------- */
static void build_minimap(Room *r) {
    // 1 px per tile, RGBA4, power-of-two texture
    int tw = 8, th = 8;
    while (tw < r->w) tw *= 2;
    while (th < r->h) th *= 2;
    if (tw > 1024 || th > 1024) return;
    if (!C3D_TexInit(&r->mapTex, tw, th, GPU_RGBA4)) return;
    u16 *d = (u16 *)r->mapTex.data;
    memset(d, 0, tw * th * 2);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
        u16 v = 0;
        if (x < r->w && y < r->h) {
            bool s = room_solid_tile(r, x, y);
            bool edge = s && (!room_solid_tile(r, x + 1, y) || !room_solid_tile(r, x - 1, y) || !room_solid_tile(r, x, y + 1) || !room_solid_tile(r, x, y - 1));
            if (edge) v = 0xDDEF; else if (s) v = 0x3335; else v = 0x0000;
        }
        // texture memory row 0 is the top of the image (v = 1); map y is bottom-up
        int my = (y < r->h) ? r->h - 1 - y : y;
        int tx = x >> 3, ty = my >> 3, ix = x & 7, iy = my & 7;
        int m = (ix & 1) | ((iy & 1) << 1) | ((ix & 2) << 1) | ((iy & 2) << 2) | ((ix & 4) << 2) | ((iy & 4) << 3);
        d[(ty * (tw >> 3) + tx) * 64 + m] = v;
    }
    GSPGPU_FlushDataCache(d, tw * th * 2);
    C3D_TexSetFilter(&r->mapTex, GPU_NEAREST, GPU_NEAREST);
    r->mapSub = (Tex3DS_SubTexture){ r->w, r->h, 0.0f, 1.0f, r->w / (float)tw, 1.0f - r->h / (float)th };
    r->mapImg.tex = &r->mapTex; r->mapImg.subtex = &r->mapSub;
    r->hasMap = true;
}

static bool file_there(const char *p) { FILE *f = fopen(p, "rb"); if (f) fclose(f); return f != NULL; }

bool room_load(Room *r, const char *id) {
    memset(r, 0, sizeof(*r));
    strncpy(r->id, id, 31);
    char path[128];
    snprintf(path, sizeof path, "romfs:/rooms/%s.room", id);
    size_t sz;
    u8 *buf = read_file(path, &sz);
    if (!buf) { printf("no room %s\n", path); return false; }
    u8 *p = buf + 4;
    int version = RD(u16, p);
    r->w = RD(u16, p); r->h = RD(u16, p);
    r->nents = RD(u16, p);
    r->spawnx = RD(float, p); r->spawny = RD(float, p);
    memcpy(r->music, p, 32); p += 32;
    memcpy(r->ambience, p, 32); p += 32;
    memcpy(r->title, p, 32); p += 32;
    memcpy(r->subtitle, p, 32); p += 32;
    r->res = 1;
    if (version >= 3) { r->res = RD(u16, p); (void)RD(u16, p); }
    r->cw = r->w * r->res; r->ch = r->h * r->res;
    int rowb = (r->cw + 7) / 8;
    r->solid = malloc(rowb * r->ch);
    memcpy(r->solid, p, rowb * r->ch); p += rowb * r->ch;
    r->ents = calloc(r->nents ? r->nents : 1, sizeof(RoomEnt));
    for (int i = 0; i < r->nents; i++) {
        RoomEnt *e = &r->ents[i];
        e->kind = RD(u8, p); e->ai = RD(u8, p); (void)RD(u16, p);
        e->x = RD(float, p); e->y = RD(float, p); e->w = RD(float, p); e->h = RD(float, p);
        e->hp = RD(s16, p); e->geo = RD(s16, p);
        memcpy(e->name, p, 32); e->name[31] = 0; p += 32;
        memcpy(e->target, p, 32); e->target[31] = 0; p += 32;
        if (version >= 2) { memcpy(e->extra, p, 96); e->extra[95] = 0; p += 96; }
        e->image = -1;
    }
    free(buf);

    snprintf(path, sizeof path, "romfs:/rooms/%s.lyr", id);
    r->blob = read_file(path, &sz);
    if (!r->blob) hk_log("room %s: no art (%s)", id, file_there(path) ? "not enough memory" : "file missing");
    if (r->blob) {
        r->zipped = !memcmp(r->blob, "HKL2", 4);
        p = r->blob + 4;
        r->nlayers = RD(u16, p); r->tile = RD(u16, p); r->S = RD(u16, p);
        r->xd = r->S >> 8; r->S &= 0xFF; if (r->xd < 1) r->xd = 1;     // HD art: texels across per screen pixel in the high byte
        r->layers = calloc(r->nlayers, sizeof(Layer));
        for (int i = 0; i < r->nlayers; i++) {
            Layer *L = &r->layers[i];
            L->F = RD(float, p); L->x0 = RD(float, p); L->y0 = RD(float, p); L->res = RD(float, p);
            L->tx = RD(u16, p); L->ty = RD(u16, p);
            u16 nt = RD(u16, p);
            L->fmt = RD(u8, p); L->front = RD(u8, p);
            if (L->res <= 0) L->res = 1;
            L->tileOff = calloc(L->tx * L->ty, sizeof(u32));
            L->tileSize = calloc(L->tx * L->ty, sizeof(u32));
            L->opq = calloc(L->tx * L->ty, sizeof(u64)); L->nem = calloc(L->tx * L->ty, sizeof(u64));
            L->known = calloc(L->tx * L->ty, 1); L->vis = calloc(L->tx * L->ty, sizeof(u64));
            L->cep = calloc(L->tx * L->ty, sizeof(void *));
            for (int t = 0; t < nt; t++) {
                u16 ti = RD(u16, p), tj = RD(u16, p);
                u32 s = RD(u32, p);
                int idx = tj * L->tx + ti;
                L->tileOff[idx] = p - r->blob;
                L->tileSize[idx] = s;
                p += s ? s : 4;
            }
        }
    }
    // the baked entity and enemy images are only needed in rooms without objects (game.c loads them after the objects:
    // room_load_entity_art); here just their names, so entities know which image would be theirs
    snprintf(path, sizeof path, "romfs:/rooms/%s_ents.spr", id);
    spr_load_meta(&r->entset, path);
    snprintf(path, sizeof path, "romfs:/rooms/%s_enemies.spr", id);
    { FILE *t = fopen(path, "rb"); if (t) { fclose(t); spr_load_meta(&r->enemyset, path); } }
    for (int i = 0; i < r->nents; i++) r->ents[i].image = spr_named(&r->entset, r->ents[i].name);
    build_minimap(r);
    return true;
}

// the textures of the baked entity images (only those still needed) and of the native enemies
void room_load_entity_art(Room *r, bool images, bool enemies) {
    char path[96];
    // (the same files as the names loaded by room_load, so frame and animation numbers stay valid)
    spr_free(&r->entset); spr_free(&r->enemyset);
    if (images) { snprintf(path, sizeof path, "romfs:/rooms/%s_ents.spr", r->id); spr_load(&r->entset, path); }
    if (enemies) { snprintf(path, sizeof path, "romfs:/rooms/%s_enemies.spr", r->id); FILE *t = fopen(path, "rb"); if (t) { fclose(t); spr_load(&r->enemyset, path); } }
}

void room_free(Room *r) {
#ifdef TW
    tw_drain();         // no job may still be reading this room's data
#endif
    for (int i = 0; i < CACHE_N; i++) if (cache[i].room == r) { cache[i].room = NULL; cache[i].layer = -1; }
    for (int i = 0; i < r->nlayers; i++) { free(r->layers[i].tileOff); free(r->layers[i].tileSize); free(r->layers[i].opq); free(r->layers[i].nem); free(r->layers[i].known); free(r->layers[i].vis); free(r->layers[i].cep); }
    free(r->layers); free(r->blob); free(r->solid); free(r->ents);
    spr_free(&r->entset);
    spr_free(&r->enemyset);
    if (r->hasMap) C3D_TexDelete(&r->mapTex);
    memset(r, 0, sizeof(*r));
}

bool room_solid_cell(const Room *r, int x, int y) {
    if (y >= r->ch || y < 0) return false;    // open above and below (bottom exits like Dirtmouth's well)
    if (x < 0) x = 0;                         // beyond the side edges the edge column continues (doorways stay open
    if (x >= r->cw) x = r->cw - 1;            // for the walk in/out at transitions, like the original's off-screen floor)
    return (r->solid[y * ((r->cw + 7) / 8) + (x >> 3)] >> (x & 7)) & 1;
}

bool room_solid_tile(const Room *r, int x, int y) {
    int c = r->res / 2;
    return room_solid_cell(r, x * r->res + c, y * r->res + c);
}

// with time left in the frame, unpack the tiles the camera is heading into (where it will be in ~1/3 s), so scrolling
// finds them ready instead of unpacking them all on the frame they appear
static void prefetch_tiles(Room *r, float camx, float camy) {
    static float lx, ly, vx, vy; static const Room *lr;
    if (lr != r) { lr = r; lx = camx; ly = camy; vx = vy = 0; }
    vx = vx * 0.8f + (camx - lx) * 0.2f; vy = vy * 0.8f + (camy - ly) * 0.2f; lx = camx; ly = camy;
    if (fabsf(vx) < 0.005f && fabsf(vy) < 0.005f) return;
    float px = camx + vx * 20, py = camy + vy * 20;
    int T = r->tile;
    for (int li = r->nlayers - 1; li >= 0; li--) {          // nearest layers first: they scroll fastest
        Layer *L = &r->layers[li];
        float scale = PPU / (r->S * L->res), Sres = r->S * L->res, tsz = T * scale, tszx = tsz / r->xd;
        float ox = -px * L->F * Sres * scale + TOP_W / 2, oy = py * L->F * Sres * scale + TOP_H / 2;
        int i0 = (int)floorf((0 - ox) / tszx - L->x0 / T), i1 = (int)floorf((TOP_W - ox) / tszx - L->x0 / T);
        int j0 = (int)floorf((0 - oy) / tsz - L->y0 / T), j1 = (int)floorf((TOP_H - oy) / tsz - L->y0 / T);
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 >= L->tx) i1 = L->tx - 1; if (j1 >= L->ty) j1 = L->ty - 1;
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
            int idx = j * L->tx + i;
            if (!L->tileOff[idx] || L->tileSize[idx] == 0) continue;
#ifdef TW
            if (twThread) {          // the worker unpacks the tiles ahead; queue them while it has room
                if (!tw_room()) return;
                queueOnly = true; tile_get(r, li, idx); queueOnly = false;
                continue;
            }
#endif
            if (r->zipped && (decodeBudget <= 0 || decodeTicks > 268111856ULL / 300)) return;
            tile_get(r, li, idx);
        }
    }
}

// the tiles on screen, gameplay plane first: the walls and floor the Knight stands on (parallax 1), then the layers
// nearest to it, the far background last. Asked for before drawing, they take the tile cache and the frame's unpacking
// budget first; when the cache can't hold everything on screen (Old 3DS: 40 tiles) or unpacking runs out of time, a
// far background tile is what's missing for a moment, not the terrain (drawn back to front, the background used to
// fill the cache first and floor tiles flickered in and out)
// the layers in reverse drawing order (the foreground pass, then the back pass): each tile's blocks tested against
// what's in front of them, then its own opaque blocks added
static void compute_hidden(Room *r, float camx, float camy) {
    memset(occ, 0, sizeof occ);
#ifdef __HOST__
    { static int c = -1; if (c < 0) c = getenv("HK_NOCULL") != NULL; cullOff = c; }
#endif
    int T = r->tile;
    for (int pass = 1; pass >= 0; pass--) for (int li = r->nlayers - 1; li >= 0; li--) {
        Layer *L = &r->layers[li];
        if ((L->front & 4) || !L->vis) continue;              // 'over' layers are drawn last, whole
        bool isFront = (L->front & 1) && L->F > 1.001f;
        if (isFront != (pass == 1)) continue;
        bool occluder = !(L->front & 2);                       // not an additive glow layer (blocks with any transparency never cover)
        float Sres = r->S * L->res, scale = PPU / (r->S * L->res);
        float ox = -camx * L->F * Sres * scale + TOP_W / 2, oy = camy * L->F * Sres * scale + TOP_H / 2;
        float tsz = T * scale, scalex = scale / r->xd, tszx = tsz / r->xd;
        int i0 = (int)floorf((0 - ox) / tszx - L->x0 / T), i1 = (int)floorf((TOP_W - ox) / tszx - L->x0 / T);
        int j0 = (int)floorf((0 - oy) / tsz - L->y0 / T), j1 = (int)floorf((TOP_H - oy) / tsz - L->y0 / T);
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 >= L->tx) i1 = L->tx - 1; if (j1 >= L->ty) j1 = L->ty - 1;
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
            int idx = j * L->tx + i;
            if (!L->tileOff[idx]) continue;
            if (cullOff) { L->vis[idx] = ALLB; continue; }
            float sx = floorf((ox + (L->x0 + i * T) * scalex) * r->xd) / r->xd, sy = floorf(oy + (L->y0 + j * T) * scale);
            float sx2 = floorf((ox + (L->x0 + (i + 1) * T) * scalex) * r->xd) / r->xd, sy2 = floorf(oy + (L->y0 + (j + 1) * T) * scale);
            bool solid = L->tileSize[idx] == 0;
            u64 want, opq;
            if (solid) { const u8 *c = r->blob + L->tileOff[idx]; want = c[3] ? ALLB : 0; opq = c[3] == 255 ? ALLB : 0; }
            else if (L->known[idx]) { want = L->nem[idx]; opq = L->opq[idx]; }
            else { want = ALLB; opq = 0; }                     // not unpacked yet: drawn whole, covers nothing
            // the block edges in coverage cells (integer work from here: this runs for ~200 tiles a frame): a block
            // is covered if every cell it touches is (floor of its start, ceil of its end); it covers the cells wholly
            // inside it (ceil of its start, floor of its end)
            int fx[9], cx[9], fy[9], cy[9];
            float bw = (sx2 - sx) / 8, bh = (sy2 - sy) / 8;
            for (int k = 0; k <= 8; k++) {
                float ex = (k == 8 ? sx2 : sx + k * bw) * (1.0f / OCC_C), ey = (k == 8 ? sy2 : sy + k * bh) * (1.0f / OCC_C);
                fx[k] = ifloor(ex); cx[k] = iceil(ex); fy[k] = ifloor(ey); cy[k] = iceil(ey);
            }
            u64 vis = 0;
            for (int by = 0; by < 8; by++) {
                unsigned row = (unsigned)(want >> (by * 8)) & 0xFF;
                if (!row) continue;
                int y0 = fy[by] < 0 ? 0 : fy[by], y1 = cy[by + 1] > OCC_H ? OCC_H : cy[by + 1];
                if (y1 <= y0) continue;                              // off screen
                u32 M[4] = { ~0u, ~0u, ~0u, ~0u };                    // cells covered on every row the blocks touch
                for (int y = y0; y < y1; y++) { M[0] &= occ[y][0]; M[1] &= occ[y][1]; M[2] &= occ[y][2]; M[3] &= occ[y][3]; }
                bool none = !(M[0] | M[1] | M[2] | M[3]);
                for (int bx = 0; bx < 8; bx++) if (row >> bx & 1) {
                    int x0 = fx[bx] < 0 ? 0 : fx[bx], x1 = cx[bx + 1] > OCC_W ? OCC_W : cx[bx + 1];
                    if (x1 <= x0) continue;                          // off screen
                    if (none || !bits_all(M, x0, x1)) vis |= 1ull << (by * 8 + bx);
                }
            }
            L->vis[idx] = vis;
#ifdef __HOST__
            g_ct[0]++; if (!vis) g_ct[1]++; if (vis == ALLB) g_ct[2]++; g_ct[3] += __builtin_popcountll(vis); g_ct[4] += __builtin_popcountll(want);
#endif
            u64 m = occluder ? (opq & vis) : 0;
            for (int by = 0; by < 8 && m; by++) {
                unsigned row = (unsigned)(m >> (by * 8)) & 0xFF;
                if (!row) continue;
                int y0 = cy[by] < 0 ? 0 : cy[by], y1 = fy[by + 1] > OCC_H ? OCC_H : fy[by + 1];
                if (y1 <= y0) continue;
                u32 B[4] = { 0, 0, 0, 0 };
                for (int bx = 0; bx < 8; ) {
                    if (!(row >> bx & 1)) { bx++; continue; }
                    int e = bx; while (e < 8 && (row >> e & 1)) e++;
                    int x0 = cx[bx] < 0 ? 0 : cx[bx], x1 = fx[e] > OCC_W ? OCC_W : fx[e];
                    if (x1 > x0) for (int w = 0; w < 4; w++) B[w] |= occ_bits(x0, x1, w);
                    bx = e;
                }
                for (int y = y0; y < y1; y++) { occ[y][0] |= B[0]; occ[y][1] |= B[1]; occ[y][2] |= B[2]; occ[y][3] |= B[3]; }
            }
        }
    }
}

static void request_visible(Room *r, float camx, float camy) {
    int ord[64], n = r->nlayers < 64 ? r->nlayers : 64, T = r->tile;
    float key[64];
    for (int li = 0; li < n; li++) { float F = r->layers[li].F; key[li] = fabsf(logf(F > 0.01f ? F : 0.01f)); ord[li] = li; }
    for (int a = 1; a < n; a++) { int v = ord[a]; int b = a - 1; while (b >= 0 && key[ord[b]] > key[v]) { ord[b + 1] = ord[b]; b--; } ord[b + 1] = v; }
    for (int k = 0; k < n; k++) {
        int li = ord[k]; Layer *L = &r->layers[li];
        float scale = PPU / (r->S * L->res), Sres = r->S * L->res, tsz = T * scale, tszx = tsz / r->xd;
        float ox = -camx * L->F * Sres * scale + TOP_W / 2, oy = camy * L->F * Sres * scale + TOP_H / 2;
        int i0 = (int)floorf((0 - ox) / tszx - L->x0 / T), i1 = (int)floorf((TOP_W - ox) / tszx - L->x0 / T);
        int j0 = (int)floorf((0 - oy) / tsz - L->y0 / T), j1 = (int)floorf((TOP_H - oy) / tsz - L->y0 / T);
        if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 >= L->tx) i1 = L->tx - 1; if (j1 >= L->ty) j1 = L->ty - 1;
        for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
            int idx = j * L->tx + i;
            if (!L->tileOff[idx] || L->tileSize[idx] == 0 || (L->vis && !(L->front & 4) && !L->vis[idx])) continue;
            tile_get(r, li, idx);
        }
    }
}

#ifdef __HOST__
static void draw_layer_tiles_(Room *r, int li, float camx, float camy);
static void draw_layer_tiles(Room *r, int li, float camx, float camy) {    // HK_LAYERFILL: fill per layer
    extern double g_fill; static double acc[64]; static int fr, lastFrame = -1; extern long host_frame(void);
    double f0 = g_fill; draw_layer_tiles_(r, li, camx, camy); if (li < 64) acc[li] += g_fill - f0;
    if (getenv("HK_LAYERFILL") && host_frame() != lastFrame) { lastFrame = host_frame(); if (++fr % 120 == 0) {
        for (int k = 0; k < r->nlayers && k < 64; k++) { Layer *L = &r->layers[k]; fprintf(stderr, "[lfill] %s L%d F=%.3f res=%.2f front=%d fmt=%d: %.2f screens\n", r->id, k, L->F, L->res, L->front, L->fmt, acc[k] / 120 / (800.0 * 240)); }
        memset(acc, 0, sizeof acc); } }
}
static void draw_layer_tiles_(Room *r, int li, float camx, float camy) {
#else
static void draw_layer_tiles(Room *r, int li, float camx, float camy) {
#endif
    Layer *L = &r->layers[li]; int T = r->tile; bool isAdd = (L->front & 2) != 0;
    float Sres = r->S * L->res;
    float scale = PPU / (r->S * L->res) ;  // layer px -> screen px (S==PPU normally)
    // screen = (lx - camx*F*Sres) * scale + 200
    float ox = -camx * L->F * Sres * scale + TOP_W / 2;
    float oy = camy * L->F * Sres * scale + TOP_H / 2;
    float tsz = T * scale, scalex = scale / r->xd, tszx = tsz / r->xd;     // HD layers: xd texels across per screen pixel
    // visible tile range
    int i0 = (int)floorf((0 - ox) / tszx - L->x0 / T), i1 = (int)floorf((TOP_W - ox) / tszx - L->x0 / T);
    int j0 = (int)floorf((0 - oy) / tsz - L->y0 / T), j1 = (int)floorf((TOP_H - oy) / tsz - L->y0 / T);
    if (i0 < 0) i0 = 0;
    if (j0 < 0) j0 = 0;
    if (i1 >= L->tx) i1 = L->tx - 1;
    if (j1 >= L->ty) j1 = L->ty - 1;
    if (isAdd) gfx_additive(true);
    for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
        int idx = j * L->tx + i;
        if (!L->tileOff[idx]) continue;
        u64 vis = (L->front & 4) ? ALLB : L->vis[idx];           // the blocks to draw (compute_hidden)
        if (!vis) continue;                                      // nothing visible, or all of it behind opaque art
        float sx = floorf((ox + (L->x0 + i * T) * scalex) * r->xd) / r->xd;     // whole screen pixels (half ones on the wide screen)
        float sy = floorf(oy + (L->y0 + j * T) * scale);
        float sx2 = floorf((ox + (L->x0 + (i + 1) * T) * scalex) * r->xd) / r->xd;
        float sy2 = floorf(oy + (L->y0 + (j + 1) * T) * scale);
        bool solid = L->tileSize[idx] == 0;
        const u8 *c = solid ? r->blob + L->tileOff[idx] : NULL;
        CacheEnt *ce = NULL;
        if (!solid) {
            ce = tile_get(r, li, idx);
#ifdef __HOST__
            if (!ce) { extern int g_tileMiss; g_tileMiss++; }      // a tile on screen not drawn this frame (cache full or unpacking deferred)
#endif
            if (!ce) { stMissNow++; continue; }
        }
#ifdef __HOST__
        { extern int g_fillIsTile; g_fillIsTile = 1; }
#endif
        if (vis == ALLB) {
            if (solid) C2D_DrawRectSolid(sx, sy, 0.5f, sx2 - sx, sy2 - sy, C2D_Color32(c[0], c[1], c[2], c[3]));
            else C2D_DrawImageAt(ce->img, sx, sy, 0.5f, NULL, (sx2 - sx) / T, (sy2 - sy) / T);
        } else {
            // the visible blocks as a few rectangles: runs along each row of blocks, rows with the same runs merged
            float bw = (sx2 - sx) / 8, bh = (sy2 - sy) / 8;
            for (int by = 0; by < 8; ) {
                unsigned row = (unsigned)(vis >> (by * 8)) & 0xFF;
                if (!row) { by++; continue; }
                int ey = by + 1; while (ey < 8 && ((unsigned)(vis >> (ey * 8)) & 0xFF) == row) ey++;
                float y0 = sy + by * bh, y1 = ey == 8 ? sy2 : sy + ey * bh;
                for (int bx = 0; bx < 8; ) {
                    if (!(row >> bx & 1)) { bx++; continue; }
                    int ex = bx; while (ex < 8 && (row >> ex & 1)) ex++;
                    float x0 = sx + bx * bw, x1 = ex == 8 ? sx2 : sx + ex * bw;
                    if (solid) C2D_DrawRectSolid(x0, y0, 0.5f, x1 - x0, y1 - y0, C2D_Color32(c[0], c[1], c[2], c[3]));
                    else {
                        Tex3DS_SubTexture sub = { (u16)((ex - bx) * T / 8), (u16)((ey - by) * T / 8),
                                                  bx / 8.0f, 1.0f - by / 8.0f, ex / 8.0f, 1.0f - ey / 8.0f };
                        C2D_Image im = { ce->img.tex, &sub };
                        C2D_DrawImageAt(im, x0, y0, 0.5f, NULL, (x1 - x0) / sub.width, (y1 - y0) / sub.height);
                    }
                    bx = ex;
                }
                by = ey;
            }
        }
#ifdef __HOST__
        { extern int g_fillIsTile; g_fillIsTile = 0; }
#endif
    }
    if (isAdd) gfx_additive(false);
}

// layers of art Unity draws after everything else whatever its depth (a higher sorting layer: glows, haze, lamp light):
// over the foreground silhouettes too, which they tint. Far to near, each with its own parallax.
void room_draw_over(Room *r, float camx, float camy) {
    if (!r->blob) return;
    for (int li = 0; li < r->nlayers; li++) if (r->layers[li].front & 4) draw_layer_tiles(r, li, camx, camy);
}

void (*room_plane_hook)(void);     // the back pass calls this just before the gameplay plane's layers (hero light)

void room_draw_layers(Room *r, float camx, float camy, bool front) {
    bool hooked = front;
    if (!r->blob) return;
#ifdef __HOST__
    if (!front) { extern int g_tileMiss; static int fr, worst, tot; fr++; if (g_tileMiss > worst) worst = g_tileMiss; tot += g_tileMiss;
        if (getenv("HK_TILETRACE") && fr % 60 == 0) { fprintf(stderr, "[tiles] %s cache %d: misses last 60 frames %d (worst frame %d)\n", r->id, cacheN, tot, worst); worst = tot = 0; }
        g_tileMiss = 0; }
#endif
    if (!front) {     // back layers are drawn first each frame
        if (stFrames++) { if (stMissNow) stMissFrames++; if (stMissNow > stMissWorst) stMissWorst = stMissNow; }   // last frame's
        if (stFrames % 1200 == 0)     // a long stay in one room: the numbers so far every 20 s
            hk_log("tiles %s so far: %d frames, %d with tiles missing (worst %d), unpacked %d here (%.2f ms each) + %d by the worker", stRoom,
                   stFrames, stMissFrames, stMissWorst, stMain, stMain ? stMainTicks / 268111.856 / stMain : 0.0, stWorker);
        stMissNow = 0; snprintf(stRoom, sizeof stRoom, "%s", r->id);
        primeNow = primeNext; primeNext = false;
#ifdef TW
        twFrame++;
#endif
        frameStart = cacheClock; decodeBudget = primeNow ? 100000 : r->tile >= 256 ? 6 : 24; decodeTicks = 0; decodedNow = 0;
        compute_hidden(r, camx, camy);
        {   // test runs: every on-screen tile's culling state once, 300 frames into a room (sdmc:/3ds/hk3ds/test_tiledump.txt,
            // HK_TILEDUMP on the PC), to compare a console's decisions with the PC's
            static int want = -1; static char done[32];
#ifdef __HOST__
            if (want < 0) want = getenv("HK_TILEDUMP") != NULL;
#else
            if (want < 0) { FILE *f = fopen("sdmc:/3ds/hk3ds/test_tiledump.txt", "rb"); want = f != NULL; if (f) fclose(f); }
#endif
            if (want && stFrames == 300 && strcmp(done, r->id)) {
                snprintf(done, sizeof done, "%s", r->id);
                hk_log("tiledump %s cam %.3f %.3f", r->id, camx, camy);
                for (int li = 0; li < r->nlayers; li++) {
                    Layer *L = &r->layers[li]; int T = r->tile;
                    if (!L->vis) continue;
                    float scale = PPU / (r->S * L->res), Sres = r->S * L->res, tsz = T * scale, tszx = tsz / r->xd;
                    float ox = -camx * L->F * Sres * scale + TOP_W / 2, oy = camy * L->F * Sres * scale + TOP_H / 2;
                    int i0 = (int)floorf((0 - ox) / tszx - L->x0 / T), i1 = (int)floorf((TOP_W - ox) / tszx - L->x0 / T);
                    int j0 = (int)floorf((0 - oy) / tsz - L->y0 / T), j1 = (int)floorf((TOP_H - oy) / tsz - L->y0 / T);
                    if (i0 < 0) i0 = 0; if (j0 < 0) j0 = 0; if (i1 >= L->tx) i1 = L->tx - 1; if (j1 >= L->ty) j1 = L->ty - 1;
                    for (int j = j0; j <= j1; j++) for (int i = i0; i <= i1; i++) {
                        int idx = j * L->tx + i; if (!L->tileOff[idx]) continue;
                        hk_log("td L%d F%.3f f%d t%d,%d k%d s%d nem %016llx opq %016llx vis %016llx", li, L->F, L->front, i, j, L->known[idx],
                               L->tileSize[idx] == 0, (unsigned long long)(L->known[idx] ? L->nem[idx] : 0),
                               (unsigned long long)(L->known[idx] ? L->opq[idx] : 0), (unsigned long long)L->vis[idx]);
                    }
                }
            }
        }
#ifdef __HOST__
        { extern long g_ct[6]; static int f; if (getenv("HK_CULLTRACE") && ++f % 60 == 0) { int cov = 0; for (int y = 0; y < OCC_H; y++) for (int x = 0; x < OCC_W; x++) cov += occ[y][x >> 5] >> (x & 31) & 1;
            fprintf(stderr, "[cull] %s tiles %ld skipped %ld whole %ld | blocks drawn %ld of %ld with art | screen covered %.0f%%\n", r->id, g_ct[0], g_ct[1], g_ct[2], g_ct[3], g_ct[4], cov * 100.0 / (OCC_W * OCC_H)); }
          memset(g_ct, 0, sizeof g_ct); }
#endif
        request_visible(r, camx, camy);
    }
    int T = r->tile;
    float prevF = front ? 1.02f : 0.0f;
    for (int li = 0; li < r->nlayers; li++) {
        Layer *L = &r->layers[li];
        // the gameplay-plane 'front' layer (grass, small plants) is drawn behind the Knight so he never disappears;
        // only true foreground parallax layers (F > 1) cover him, like in HK
        if (L->front & 4) continue;      // an 'over' layer: drawn last (room_draw_over)
        // the hook (the Knight's light) goes in before the gameplay plane: the black terrain and the art laid over it
        // cover it, as in the game (lit over the tiles it made a grey patch on the black ground, cut off square by
        // the black filler sprites next to it)
        if (!hooked && L->F >= 0.999f) { hooked = true; if (room_plane_hook) room_plane_hook(); }
        bool isFront = (L->front & 1) && L->F > 1.001f, isAdd = (L->front & 2) != 0;   // bit 1: an additive (glow) layer
#ifdef __HOST__
        { static char lastId[32]; static int once = 0; if (strcmp(lastId, r->id)) { strcpy(lastId, r->id); once = 0; } if (getenv("HK_LAYERINFO") && once < r->nlayers) { once++; fprintf(stderr, "[layer %d] F=%.3f res=%.2f front=%d x0=%.0f y0=%.0f tiles=%dx%d\n", li, L->F, L->res, L->front, L->x0, L->y0, L->tx, L->ty); } }
#endif
        if (isFront != front) continue;
#ifdef __HOST__
        { static int skip = -2; if (skip == -2) skip = getenv("HK_SKIPLAYER") ? atoi(getenv("HK_SKIPLAYER")) : -1; if (skip == li) continue;
          const char *only = getenv("HK_ONLYLAYERS"); if (only) { char k[8]; snprintf(k, sizeof k, ",%d,", li); char b[128]; snprintf(b, sizeof b, ",%s,", only); if (!strstr(b, k)) continue; } }
#endif
        // live objects further back than this layer go first (background machinery, foreground silhouettes...)
        // A baked layer merges sprites from a range of depths, some just behind an object that sits among them: an
        // object within 2.5% of the layer's depth goes in front of it (the Grubfather at F 0.726 sat behind the
        // grub-home sheet at 0.739 and vanished, though most of that sheet is behind him in the game)
        { float lim = front ? L->F : (L->F * 0.975f < 0.98f ? L->F * 0.975f : 0.98f); if (lim > prevF) { uo_draw_range(camx, camy, prevF, lim); particles_draw_range(camx, camy, prevF, lim); masks_draw_range(camx, camy, prevF, lim); prevF = lim; } }
        draw_layer_tiles(r, li, camx, camy);
    }
    if (front) { uo_draw_range(camx, camy, prevF, 100.0f); particles_draw_range(camx, camy, prevF, 1e9f); masks_draw_range(camx, camy, prevF, 1e9f); }
    else if (prevF < 0.98f) { uo_draw_range(camx, camy, prevF, 0.98f); particles_draw_range(camx, camy, prevF, 0.98f); masks_draw_range(camx, camy, prevF, 0.98f); }
    if (!hooked && room_plane_hook) room_plane_hook();
    if (front) prefetch_tiles(r, camx, camy);
}
