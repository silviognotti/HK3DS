// Particle systems: see particles.h. Data from tools/particles.py: every curve is [n, lo..., hi...] (value at t =
// lerp(lo(t), hi(t), a per-particle random)), gradients the same with RGBA. Emitter matrices map the shape's space to
// world x/y (A, with translation), shape directions (D) and the emitter's local space (R) to world x/y.
#include "particles.h"
#include "gfx.h"
#include "json.h"
#include "uo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef __3DS__
#include <3ds.h>
#endif

#define CAM_D 38.1f
#define MAXC 8
typedef struct { int n; float lo[MAXC], hi[MAXC]; } Curve;
typedef struct { int n; bool randT; float lo[MAXC][4], hi[MAXC][4]; } Grad;

typedef struct {
    // data
    float A[8], D[6], R[6], pos[3], F, ssz;
    float dur, delay; bool loop, poa, world;
    Curve life, spd, size, rot, grav, rate, velx, vely, velz, frcx, frcy, frcz, lim, szt, rott, fot, stf;
    Grad col, colt;
    bool hasVel, velW, hasFrc, frcW, hasLim, hasColt, hasSzt, hasRott, hasUV, hasNoise;
    float limD, noiseS, noiseF, uvCyc; int uvN;
    int shape, sax; float sr, srt, sang, sarc, sbox[3], srd;
    int nb; float bt[4], bmin[4], bmax[4], bint[4]; int bcyc[4];
    bool stretch, add; float ls, vs, maxsz, tint[4]; int fr, maxn, sl;
    // runtime
    const SpriteSet *set;
    float t, acc, startT; int alive, bdone[4], bcycle;
    bool playing, emitting, follow, warm, used, near;
    struct UObj *owner;
    float ox, oy;          // where the emitter's origin was at the last update (local-space particles move with it)
    // on an object (a "ps" component): matrices in the object's frame, turned into world ones every frame
    bool owned; float LA[8], LD[6], LR[6], Lssz, Lz; int gen;
} Emitter;

typedef struct { float x, y, vx, vy, age, life, size, rot, rotv, grav, c[4], r[3]; short em; } Part;

#define MAX_EM 320
static Emitter ems[MAX_EM]; static int nems;     // nems: slots in use up to here (object emitters free theirs)
static Part *parts; static int nparts, cap;
static SpriteSet roomSet, zoneSet, objSet;     // objSet: the room's object emitters (<room>_pto.spr)
static JArena zoneArena; static JVal *zoneRoot;
static int zoneFirst = -1;            // emitters [zoneFirst..) are the area set (camera-relative)
static float budgetScale = 1;

static float frand01(void) { return (float)rand() / (float)RAND_MAX; }
static float frandr(float a, float b) { return a + (b - a) * frand01(); }

/* ---------------- data ---------------- */
static void curve_of(Curve *c, const JVal *v, float def) {
    c->n = 1; c->lo[0] = c->hi[0] = def;
    if (!jis(v, J_ARR) || v->n < 3) return;
    int n = jint(jat(v, 0), 1); if (n < 1) n = 1; if (n > MAXC) n = MAXC;
    if (v->n < 1 + 2 * n) return;
    c->n = n;
    for (int i = 0; i < n; i++) { c->lo[i] = (float)jnum(jat(v, 1 + i), def); c->hi[i] = (float)jnum(jat(v, 1 + n + i), def); }
}
static float cval(const Curve *c, float t, float r) {
    if (c->n <= 1) return c->lo[0] + (c->hi[0] - c->lo[0]) * r;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float x = t * (c->n - 1); int i = (int)x; if (i >= c->n - 1) i = c->n - 2; float s = x - i;
    float lo = c->lo[i] + (c->lo[i + 1] - c->lo[i]) * s, hi = c->hi[i] + (c->hi[i + 1] - c->hi[i]) * s;
    return lo + (hi - lo) * r;
}
static void grad_of(Grad *g, const JVal *v) {
    g->n = 1; g->randT = false; for (int k = 0; k < 4; k++) g->lo[0][k] = g->hi[0][k] = 1;
    if (!jis(v, J_ARR) || v->n < 10) return;
    int n = jint(jat(v, 0), 1); if (n < 1) n = 1; if (n > MAXC) n = MAXC;
    if (v->n < 2 + 8 * n) return;
    g->n = n; g->randT = jint(jat(v, 1), 0) != 0;
    for (int i = 0; i < n; i++) for (int k = 0; k < 4; k++) {
        g->lo[i][k] = (float)jnum(jat(v, 2 + i * 4 + k), 1); g->hi[i][k] = (float)jnum(jat(v, 2 + n * 4 + i * 4 + k), 1);
    }
}
static void gval(const Grad *g, float t, float r, float out[4]) {
    if (g->randT) { t = r; r = 0; }
    if (g->n <= 1) { for (int k = 0; k < 4; k++) out[k] = g->lo[0][k] + (g->hi[0][k] - g->lo[0][k]) * r; return; }
    if (t < 0) t = 0; if (t > 1) t = 1;
    float x = t * (g->n - 1); int i = (int)x; if (i >= g->n - 1) i = g->n - 2; float s = x - i;
    for (int k = 0; k < 4; k++) {
        float lo = g->lo[i][k] + (g->lo[i + 1][k] - g->lo[i][k]) * s, hi = g->hi[i][k] + (g->hi[i + 1][k] - g->hi[i][k]) * s;
        out[k] = lo + (hi - lo) * r;
    }
}
static void floats(float *dst, int n, const JVal *v, float def) { for (int i = 0; i < n; i++) dst[i] = (float)jnum(jat(v, i), def); }

static Emitter *emitter_from(const JVal *e, const SpriteSet *set) {
    int slot = -1;
    for (int i = 0; i < nems; i++) if (!ems[i].used) { slot = i; break; }   // a freed object emitter's slot
    if (slot < 0) { if (nems >= MAX_EM) return NULL; slot = nems++; }
    Emitter *m = &ems[slot]; int gen = m->gen; memset(m, 0, sizeof *m); m->gen = gen + 1;
    m->set = set; m->used = true;
    floats(m->A, 8, jget(e, "A"), 0); floats(m->D, 6, jget(e, "D"), 0); floats(m->R, 6, jget(e, "R"), 0); floats(m->pos, 3, jget(e, "pos"), 0);
    float d = CAM_D + m->pos[2]; m->F = CAM_D / (d < 1 ? 1 : d);
    m->ssz = (float)jnum(jget(e, "ssz"), 1);
    m->dur = (float)jnum(jget(e, "dur"), 5); if (m->dur < 0.05f) m->dur = 0.05f;
    m->delay = (float)jnum(jget(e, "delay"), 0); m->loop = jint(jget(e, "loop"), 1); m->poa = jint(jget(e, "poa"), 1);
    m->world = jint(jget(e, "sim"), 0) == 1; m->warm = jint(jget(e, "pre"), 0);
    curve_of(&m->life, jget(e, "life"), 5); curve_of(&m->spd, jget(e, "spd"), 5); curve_of(&m->size, jget(e, "size"), 1);
    curve_of(&m->rot, jget(e, "rot"), 0); curve_of(&m->grav, jget(e, "grav"), 0); curve_of(&m->rate, jget(e, "rate"), 0);
    grad_of(&m->col, jget(e, "col"));
    m->maxn = jint(jget(e, "max"), 100);
    const JVal *b = jget(e, "bursts");
    for (int i = 0; b && i < b->n && i < 4; i++) {
        const JVal *x = jat(b, i);
        m->bt[i] = (float)jnum(jat(x, 0), 0); m->bmin[i] = (float)jnum(jat(x, 1), 0); m->bmax[i] = (float)jnum(jat(x, 2), 0);
        m->bcyc[i] = jint(jat(x, 3), 1); m->bint[i] = (float)jnum(jat(x, 4), 0.01f); m->nb = i + 1;
    }
    const JVal *s = jget(e, "shape");
    m->shape = jint(jget(s, "t"), -1); m->sr = (float)jnum(jget(s, "r"), 1); m->srt = (float)jnum(jget(s, "rt"), 1);
    m->sang = (float)jnum(jget(s, "ang"), 25) * (float)M_PI / 180; m->sarc = (float)jnum(jget(s, "arc"), 360) * (float)M_PI / 180;
    m->srd = (float)jnum(jget(s, "rd"), 0); m->sax = jint(jget(s, "ax"), 2);
    floats(m->sbox, 3, jget(s, "box"), 1);
    const JVal *v;
    if ((v = jget(e, "vel"))) { m->hasVel = true; curve_of(&m->velx, jget(v, "x"), 0); curve_of(&m->vely, jget(v, "y"), 0); curve_of(&m->velz, jget(v, "z"), 0); m->velW = jint(jget(v, "w"), 0); }
    if ((v = jget(e, "frc"))) { m->hasFrc = true; curve_of(&m->frcx, jget(v, "x"), 0); curve_of(&m->frcy, jget(v, "y"), 0); curve_of(&m->frcz, jget(v, "z"), 0); m->frcW = jint(jget(v, "w"), 0); }
    if ((v = jget(e, "lim"))) { m->hasLim = true; curve_of(&m->lim, jget(v, "m"), 1); m->limD = (float)jnum(jget(v, "d"), 1); }
    if ((v = jget(e, "colt"))) { m->hasColt = true; grad_of(&m->colt, v); }
    if ((v = jget(e, "szt"))) { m->hasSzt = true; curve_of(&m->szt, v, 1); }
    if ((v = jget(e, "rott"))) { m->hasRott = true; curve_of(&m->rott, v, 0); }
    if ((v = jget(e, "noise"))) { m->hasNoise = true; m->noiseS = (float)jnum(jget(v, "s"), 0); m->noiseF = (float)jnum(jget(v, "fq"), 0.5f); }
    if ((v = jget(e, "uv"))) { m->hasUV = true; m->uvN = jint(jget(v, "n"), 1); curve_of(&m->fot, jget(v, "fot"), 0); curve_of(&m->stf, jget(v, "st"), 0); m->uvCyc = (float)jnum(jget(v, "cyc"), 1); }
    const JVal *r = jget(e, "ren");
    m->stretch = jint(jget(r, "m"), 0) == 1; m->ls = (float)jnum(jget(r, "ls"), 2); m->vs = (float)jnum(jget(r, "vs"), 0);
    m->add = jint(jget(r, "add"), 0); m->fr = jint(jget(r, "fr"), 0); m->sl = jint(jget(r, "sl"), 0); m->maxsz = (float)jnum(jget(r, "maxsz"), 0.5f);
    floats(m->tint, 4, jget(r, "tint"), 1);
    m->playing = m->emitting = m->poa; m->t = 0; m->startT = 0;
    return m;
}

/* ---------------- budget ---------------- */
static int budget(void) {
    static int b = 0;
    if (!b) {
        bool n3 = false;
#ifdef __3DS__
        APT_CheckNew3DS(&n3);
#else
        n3 = getenv("HK_EMU_O3DS") == NULL;
#endif
        b = n3 && !g_lowmem ? 420 : 160;
    }
    return b;
}
static void rebudget(void) {   // the steady count (rate x life, capped by its max) of every emitter in view, against the budget
    extern bool g_lowParticles;
    float want = 0;
    for (int i = 0; i < nems; i++) {
        Emitter *m = &ems[i]; if (!m->used || !m->playing || !m->near) continue;
        float rate = 0; for (int k = 0; k < m->rate.n; k++) rate += (m->rate.lo[k] + m->rate.hi[k]) * 0.5f; rate /= m->rate.n;
        float life = (m->life.lo[0] + m->life.hi[0]) * 0.5f;
        float n = rate * (m->loop ? life : fminf(life, m->dur)); if (n > m->maxn) n = (float)m->maxn;   // a one-shot burst emits rate x duration
        for (int k = 0; k < m->nb; k++) n += (m->bmin[k] + m->bmax[k]) * 0.5f;
        want += n;
    }
    budgetScale = want > budget() ? budget() / want : 1;
    if (g_lowParticles) budgetScale *= 0.45f;
}

/* ---------------- emitting ---------------- */
static void origin(Emitter *m, float camx, float camy, float *x, float *y) {
    if (m->follow) { *x = camx; *y = camy; return; }
    if (m->owner) { *x = uo_wx(m->owner); *y = uo_wy(m->owner); return; }
    if (m->owned) { *x = m->ox; *y = m->oy; return; }     // its object is gone: where it was
    *x = 0; *y = 0;
}
static void unit3(float v[3]) {
    float z = frandr(-1, 1), a = frandr(0, 2 * (float)M_PI), s = sqrtf(1 - z * z);
    v[0] = s * cosf(a); v[1] = s * sinf(a); v[2] = z;
}
static void spawn(Emitter *m, int ei, float ox, float oy, float preFrac) {
    if (nparts >= cap) {      // full: an object's effect (an explosion's gas, a hit's dust) takes the place of an area mote
        if (!m->owned) return;
        int j = nparts - 1; while (j >= 0 && (parts[j].em < 0 || ems[parts[j].em].owned)) j--;
        if (j < 0) return;
        ems[parts[j].em].alive--; parts[j] = parts[--nparts];
    }
    if (m->alive >= (int)(m->maxn * budgetScale + 0.999f)) return;
    float tn = m->t / m->dur; tn -= floorf(tn);
    float p[3] = { 0, 0, 0 }, d[3] = { 0, 0, 1 };
    switch (m->shape) {
        case 0: case 2: {           // sphere / hemisphere
            unit3(d); if (m->shape == 2 && d[2] < 0) d[2] = -d[2];
            float rr = m->sr * (1 - m->srt * frand01());
            p[0] = d[0] * rr; p[1] = d[1] * rr; p[2] = d[2] * rr; break; }
        case 4: {                   // cone: from a disc in XY, opening by its angle
            float a = frandr(0, m->sarc), rr = m->sr * sqrtf(1 - m->srt * frand01());
            float cx = cosf(a), sy = sinf(a);
            p[0] = cx * rr; p[1] = sy * rr;
            float sp = sinf(m->sang) * (m->sr > 0 ? rr / m->sr : 0);
            d[0] = cx * sp; d[1] = sy * sp; d[2] = cosf(m->sang); break; }
        case 5:                     // box (and mesh planes as boxes): along one axis
            for (int k = 0; k < 3; k++) p[k] = (frand01() - 0.5f) * m->sbox[k];
            d[0] = 0; d[1] = m->sax == 1 ? 1 : 0; d[2] = m->sax == 2 ? 1 : 0; break;
        case 10: case 11: {         // circle in XY, outwards
            float a = frandr(0, m->sarc), rr = m->shape == 11 ? m->sr : m->sr * sqrtf(1 - m->srt * frand01());
            d[0] = cosf(a); d[1] = sinf(a); d[2] = 0; p[0] = d[0] * rr; p[1] = d[1] * rr; break; }
        case 12:                    // edge along X, emitting +Y
            p[0] = frandr(-m->sr, m->sr); d[0] = 0; d[1] = 1; d[2] = 0; break;
        default: break;
    }
    if (m->srd > 0) { float r3[3]; unit3(r3); for (int k = 0; k < 3; k++) d[k] += (r3[k] - d[k]) * m->srd; }
    Part *q = &parts[nparts++];
    float wx = m->A[0] * p[0] + m->A[1] * p[1] + m->A[2] * p[2] + m->A[3];
    float wy = m->A[4] * p[0] + m->A[5] * p[1] + m->A[6] * p[2] + m->A[7];
    float dx = m->D[0] * d[0] + m->D[1] * d[1] + m->D[2] * d[2], dy = m->D[3] * d[0] + m->D[4] * d[1] + m->D[5] * d[2];
    float sp = cval(&m->spd, tn, frand01()) * m->ssz;
    q->x = wx + (m->world ? ox : 0); q->y = wy + (m->world ? oy : 0);    // local space: kept relative to the origin
    q->vx = dx * sp; q->vy = dy * sp;
    q->life = cval(&m->life, tn, frand01()); if (q->life < 0.05f) q->life = 0.05f;
    float preAge = preFrac * q->life * 0.95f; q->age = preAge;
    q->size = cval(&m->size, tn, frand01()) * m->ssz;
    q->rot = cval(&m->rot, tn, frand01());
    q->rotv = m->hasRott ? cval(&m->rott, 0, frand01()) : 0;
    q->grav = cval(&m->grav, tn, frand01()) * -9.81f;
    gval(&m->col, tn, frand01(), q->c);
    q->r[0] = frand01(); q->r[1] = frand01(); q->r[2] = frand01();
    q->em = (short)ei; m->alive++;
    if (preAge > 0) { q->x += q->vx * preAge; q->y += q->vy * preAge + 0.5f * q->grav * preAge * preAge; q->vy += q->grav * preAge; q->rot += q->rotv * preAge; }
}

static void warm_start(Emitter *m, int ei, float ox, float oy) {
    // prewarm (or coming into view): the steady population with ages spread over a lifetime
    float rate = cval(&m->rate, 0, 0.5f) * budgetScale, life = cval(&m->life, 0, 0.5f);
    int n = (int)(rate * life); if (n > (int)(m->maxn * budgetScale)) n = (int)(m->maxn * budgetScale);
    for (int i = 0; i < n; i++) spawn(m, ei, ox, oy, frand01());
    m->t = frand01() * m->dur;
}

static void reach(const Emitter *m, float *ex, float *ey) {    // how far the shape spreads from its centre (world units)
    if (m->shape == 5) {
        *ex = 0.5f * (fabsf(m->A[0]) * m->sbox[0] + fabsf(m->A[1]) * m->sbox[1] + fabsf(m->A[2]) * m->sbox[2]);
        *ey = 0.5f * (fabsf(m->A[4]) * m->sbox[0] + fabsf(m->A[5]) * m->sbox[1] + fabsf(m->A[6]) * m->sbox[2]);
    } else {
        *ex = (fabsf(m->A[0]) + fabsf(m->A[1]) + fabsf(m->A[2])) * m->sr;
        *ey = (fabsf(m->A[4]) + fabsf(m->A[5]) + fabsf(m->A[6])) * m->sr;
    }
}
static bool near_view(Emitter *m, float camx, float camy) {
    if (m->follow) return true;
    if (m->owned) {     // around its object (or where the object was)
        float hw = (TOP_W / 2) / (PPU * m->F) + 8, hh = (TOP_H / 2) / (PPU * m->F) + 8;
        return fabsf(m->ox - camx) < hw && fabsf(m->oy - camy) < hh;
    }
    // the view at this depth, plus the shape's spread and some travel (particles leave the shape)
    float hw = (TOP_W / 2) / (PPU * m->F), hh = (TOP_H / 2) / (PPU * m->F), ex, ey;
    reach(m, &ex, &ey);
    return fabsf(m->A[3] - camx) < hw + ex + 6 && fabsf(m->A[7] - camy) < hh + ey + 6;
}

/* ---------------- lifecycle ---------------- */
void particles_init(void) {
    cap = budget() + 40;
    parts = calloc(cap, sizeof(Part));
    spr_load(&zoneSet, "romfs:/rooms/_zones_ptc.spr");
    size_t sz; char *txt = read_file("romfs:/rooms/_zones.ptc", &sz);
    if (txt) { zoneRoot = json_parse(&zoneArena, txt, sz); free(txt); }
}

void particles_unload(void) {
    nems = 0; nparts = 0; zoneFirst = -1;
    if (roomSet.loaded) spr_free(&roomSet);
    if (objSet.loaded) spr_free(&objSet);
}

static int zone_number(const char *room) {      // romfs/text/ptczones.txt: "<room> <zone>" (-2: no area particles)
    static char *tab; static size_t tsz;
    if (!tab) { tab = read_file("romfs:/text/ptczones.txt", &tsz); if (!tab) return -1; }
    size_t L = strlen(room);
    for (char *p = tab; p && *p; ) {
        char *nl = strchr(p, '\n');
        if (!strncmp(p, room, L) && p[L] == ' ') return atoi(p + L + 1);
        p = nl ? nl + 1 : NULL;
    }
    return -1;
}

void particles_load(const char *room, const char *zoneName) {
    (void)zoneName;
    particles_unload();
    if (!parts) particles_init();
    extern bool g_lowParticles;
    if (g_lowParticles) return;
    if (getenv("HK_NOPARTICLES")) return;
    char path[96]; snprintf(path, sizeof path, "romfs:/rooms/%s.ptc", room);
    size_t sz; char *txt = read_file(path, &sz);
    static JArena roomArena;
    json_free(&roomArena);
    if (txt) {
        JVal *root = json_parse(&roomArena, txt, sz); free(txt);
        snprintf(path, sizeof path, "romfs:/rooms/%s_ptc.spr", room);
        if (root && spr_load(&roomSet, path)) {
            const JVal *a = jget(root, "emitters");
            for (int i = 0; a && i < a->n; i++) emitter_from(jat(a, i), &roomSet);
        }
    }
    snprintf(path, sizeof path, "romfs:/rooms/%s_pto.spr", room);
    { FILE *t = fopen(path, "rb"); if (t) { fclose(t); spr_load(&objSet, path); } }
    int z = zone_number(room);
    if (z != -2 && zoneRoot && zoneSet.loaded) {
        const JVal *zs = jget(zoneRoot, "zones");
        char key[16]; snprintf(key, sizeof key, "%d", z);
        const JVal *a = jget(zs, key); if (!a || !a->n) a = jget(zs, "-1");
        zoneFirst = nems;
        for (int i = 0; a && i < a->n; i++) { Emitter *m = emitter_from(jat(a, i), &zoneSet); if (m) m->follow = true; }
    }
    for (int i = 0; i < nems; i++) { ems[i].warm = ems[i].warm || ems[i].follow; ems[i].bcycle = -1; }
    hk_log("[ptc] %s zone %d: %d room emitters, %d area emitters", room, z, zoneFirst < 0 ? nems : zoneFirst, zoneFirst < 0 ? 0 : nems - zoneFirst);  // the area set is always in full swing
}

/* ---------------- emitters on objects ---------------- */
// the object's world frame (uo_world): position, rotation, scale (a flip mirrors directions too)
static void owned_frame(Emitter *m) {
    UObj *o = m->owner;
    float wx = uo_wx(o), wy = uo_wy(o), sx = uo_wsx(o), sy = uo_wsy(o), a = uo_wrz(o) * (float)M_PI / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    float U[4] = { ca * sx, -sa * sy, sa * sx, ca * sy };                       // R(a) * diag(sx, sy)
    float sgx = sx < 0 ? -1.0f : 1.0f, sgy = sy < 0 ? -1.0f : 1.0f;
    float N[4] = { ca * sgx, -sa * sgy, sa * sgx, ca * sgy };                   // rotation (and flip) only
    for (int c = 0; c < 4; c++) { m->A[c] = U[0] * m->LA[c] + U[1] * m->LA[4 + c]; m->A[4 + c] = U[2] * m->LA[c] + U[3] * m->LA[4 + c]; }
    for (int c = 0; c < 3; c++) {
        m->D[c] = N[0] * m->LD[c] + N[1] * m->LD[3 + c]; m->D[3 + c] = N[2] * m->LD[c] + N[3] * m->LD[3 + c];
        m->R[c] = N[0] * m->LR[c] + N[1] * m->LR[3 + c]; m->R[3 + c] = N[2] * m->LR[c] + N[3] * m->LR[3 + c];
    }
    m->ssz = m->Lssz * (fabsf(sx) + fabsf(sy)) * 0.5f;
    float z = m->Lz; for (UObj *q = o; q; q = q->parent) z += q->lz;
    float d = CAM_D + z; m->F = CAM_D / (d < 1 ? 1 : d);
    m->ox = wx; m->oy = wy;
    (void)wx; (void)wy;
}
static void kill_parts(int ei) {
    for (int j = 0; j < nparts; ) { if (parts[j].em == ei) { parts[j] = parts[--nparts]; continue; } j++; }
    ems[ei].alive = 0;
}
static void restart(Emitter *m) {
    m->playing = m->emitting = true; m->t = 0; m->startT = 0; m->acc = 0; m->bcycle = -1; memset(m->bdone, 0, sizeof m->bdone);
}
static Emitter *handle_em(int h) {
    if (h < 0) return NULL;
    int i = h & 1023, g = h >> 10;
    if (i >= nems || !ems[i].used || !ems[i].owned || (ems[i].gen & 0x1FFFFF) != g) return NULL;
    return &ems[i];
}
static int comp_handle(UComp *c) { return c->st ? *(int *)c->st : -1; }
// the component's emitter, made (stopped) if it has none yet
static Emitter *comp_emitter(UObj *o, UComp *c) {
    if (!c->st) { c->st = malloc(sizeof(int)); *(int *)c->st = -1; }
    Emitter *m = handle_em(comp_handle(c));
    if (m) return m;
    if (!parts || !objSet.loaded || getenv("HK_NOPARTICLES")) return NULL;
    m = emitter_from(c->f, &objSet);
    if (!m) return NULL;
    m->owned = true; m->owner = o; m->follow = false; m->world = jint(jget(c->f, "sim"), 0) == 1;
    memcpy(m->LA, m->A, sizeof m->LA); memcpy(m->LD, m->D, sizeof m->LD); memcpy(m->LR, m->R, sizeof m->LR);
    m->Lssz = m->ssz; m->Lz = (float)jnum(jget(c->f, "z"), 0);
    m->playing = m->emitting = false; m->bcycle = -1;
    owned_frame(m);
    *(int *)c->st = (int)(m - ems) | ((m->gen & 0x1FFFFF) << 10);
    return m;
}
void particles_comp_enable(UObj *o, UComp *c) {      // OnEnable: playOnAwake systems start (again)
    if (!jint(jget(c->f, "poa"), 1)) return;
    Emitter *m = comp_emitter(o, c); if (!m) return;
    restart(m); m->warm = jint(jget(c->f, "pre"), 0) != 0;
}
void particles_comp_disable(UObj *o, UComp *c) {     // the object went inactive: its particles go with it
    (void)o; Emitter *m = handle_em(comp_handle(c)); if (!m) return;
    m->playing = m->emitting = false; kill_parts((int)(m - ems));
}
static void each_ps(UObj *o, bool children, void (*fn)(UObj *, UComp *, float), float arg) {
    for (int k = 0; k < o->ncomp; k++) if (o->comp[k].type == C_PS) fn(o, &o->comp[k], arg);
    if (children) for (UObj *ch = o->child; ch; ch = ch->next) if (ch->activeSelf && !ch->destroyed) each_ps(ch, true, fn, arg);
}
static bool rbNow;     // an emitter just started: share the budget out again now (a burst is over in a tenth of a second)
static void ps_play(UObj *o, UComp *c, float on) {
    Emitter *m = on > 0 ? comp_emitter(o, c) : handle_em(comp_handle(c)); if (!m) return;
    if (on > 0) { if (!m->playing || !m->emitting) { restart(m); rbNow = true; } } else m->emitting = false;
}
static void ps_emit(UObj *o, UComp *c, float n) {
    Emitter *m = comp_emitter(o, c); if (!m) return;
    owned_frame(m);
    for (int i = 0; i < (int)n; i++) spawn(m, (int)(m - ems), m->ox, m->oy, 0);
}
static void ps_emission(UObj *o, UComp *c, float on) {
    Emitter *m = on > 0 ? comp_emitter(o, c) : handle_em(comp_handle(c)); if (!m) return;
    if (on > 0 && !m->playing) restart(m);
    m->emitting = on > 0;
}
static void ps_rate(UObj *o, UComp *c, float r) {
    Emitter *m = comp_emitter(o, c); if (!m) return;
    m->rate.n = 1; m->rate.lo[0] = m->rate.hi[0] = r;
}
void particles_obj_play(UObj *o, bool on) { if (o) each_ps(o, true, ps_play, on ? 1 : 0); }
void particles_obj_emit(UObj *o, int count) { if (o) each_ps(o, false, ps_emit, (float)count); }
void particles_obj_emission(UObj *o, bool on) { if (o) each_ps(o, false, ps_emission, on ? 1 : 0); }
void particles_obj_rate(UObj *o, float rate) { if (o) each_ps(o, false, ps_rate, rate); }

/* ---------------- update ---------------- */
void particles_update(float dt, float camx, float camy) {
    extern bool g_lowParticles;
    if (!parts || !nems) { nparts = 0; return; }
    if (dt > 0.1f) dt = 0.1f;
    static float rbT;
    bool changed = false;
    for (int i = 0; i < nems; i++) { bool nv = ems[i].used && near_view(&ems[i], camx, camy); changed |= nv != ems[i].near; ems[i].near = nv; }
    rbT -= dt; if (rbT <= 0 || changed || rbNow) { rebudget(); rbT = 0.5f; rbNow = false; }
    for (int i = 0; i < nems; i++) {
        Emitter *m = &ems[i]; if (!m->used) continue;
        if (m->owned) {
            if (!m->owner) { m->emitting = false; if (!m->alive) { m->used = false; continue; } }
            else owned_frame(m);
        }
        float ox, oy; origin(m, camx, camy, &ox, &oy);
        bool nv = m->near;
        if (m->playing && nv && m->warm) { warm_start(m, i, ox, oy); m->warm = false; }
        if (!nv) { if (m->playing && m->loop && !m->alive) m->warm = true; m->ox = ox; m->oy = oy; continue; }
        if (m->playing) {
            m->startT += dt;
            if (m->startT >= m->delay) {
                m->t += dt;
                if (!m->loop && m->t >= m->dur) { m->emitting = false; if (!m->alive) m->playing = false; }
                if (m->emitting) {
                    float tn = m->t / m->dur; tn -= floorf(tn);
                    m->acc += cval(&m->rate, tn, 0.5f) * budgetScale * dt;
                    int guard = 0;
                    while (m->acc >= 1 && guard++ < 64) { spawn(m, i, ox, oy, 0); m->acc -= 1; }
                    if (m->acc > 4) m->acc = 4;
                    int cyc = (int)floorf(m->t / m->dur);       // bursts: at their times within each loop of the duration
                    if (cyc != m->bcycle) { m->bcycle = cyc; memset(m->bdone, 0, sizeof m->bdone); }
                    float c = m->t - cyc * m->dur;
                    for (int k = 0; k < m->nb; k++) {
                        int reps = m->bcyc[k] < 1 ? 1000 : m->bcyc[k];
                        while (m->bdone[k] < reps) {
                            float bt = m->bt[k] + m->bdone[k] * (m->bint[k] > 0.01f ? m->bint[k] : 0.01f);
                            if (bt > c || bt >= m->dur) break;
                            int n = (int)(frandr(m->bmin[k], m->bmax[k]) * budgetScale + 0.5f);
                            for (int j = 0; j < n; j++) spawn(m, i, ox, oy, 0);
                            m->bdone[k]++;
                        }
                    }
                }
            }
        }
        m->ox = ox; m->oy = oy;
    }
    // particles
    static float T; T += dt;
    for (int j = 0; j < nparts; ) {
        Part *q = &parts[j]; Emitter *m = &ems[q->em];
        q->age += dt;
        if (q->age >= q->life) { m->alive--; parts[j] = parts[--nparts]; continue; }
        float tn = q->age / q->life;
        q->vy += q->grav * dt;
        if (m->hasFrc) {
            float fx = cval(&m->frcx, tn, q->r[0]), fy = cval(&m->frcy, tn, q->r[1]), fz = cval(&m->frcz, tn, q->r[2]);
            if (!m->frcW) { float wx = m->R[0] * fx + m->R[1] * fy + m->R[2] * fz, wy = m->R[3] * fx + m->R[4] * fy + m->R[5] * fz; fx = wx; fy = wy; }
            q->vx += fx * dt; q->vy += fy * dt;
        }
        if (m->hasLim) {
            float l = cval(&m->lim, tn, q->r[2]), s2 = q->vx * q->vx + q->vy * q->vy;
            if (s2 > l * l && s2 > 1e-6f) { float s = sqrtf(s2), ns = s + (l - s) * m->limD; q->vx *= ns / s; q->vy *= ns / s; }
        }
        float vx = q->vx, vy = q->vy;
        if (m->hasVel) {
            float ax = cval(&m->velx, tn, q->r[0]), ay = cval(&m->vely, tn, q->r[1]), az = cval(&m->velz, tn, q->r[2]);
            if (!m->velW) { float wx = m->R[0] * ax + m->R[1] * ay + m->R[2] * az, wy = m->R[3] * ax + m->R[4] * ay + m->R[5] * az; ax = wx; ay = wy; }
            vx += ax; vy += ay;
        }
        if (m->hasNoise) {
            float ph = q->r[0] * 6.2832f, w = m->noiseF * 6.2832f;
            vx += m->noiseS * sinf(T * w + ph); vy += m->noiseS * cosf(T * w * 0.83f + ph * 1.7f);
        }
        q->x += vx * dt; q->y += vy * dt;
        q->rot += q->rotv * dt;
        j++;
    }
    for (int i = 0; i < nems; i++) if (ems[i].playing && !ems[i].emitting && !ems[i].loop && !ems[i].alive) ems[i].playing = false;
#ifdef __HOST__
    { static int fr; if (getenv("HK_PTCTRACE") && ++fr % 60 == 0) { fprintf(stderr, "[ptc f] cam %.2f %.2f alive %d scale %.2f:", camx, camy, nparts, budgetScale);
        for (int i = 0; i < nems; i++) fprintf(stderr, " %d%s", ems[i].alive, ems[i].near ? "" : "x"); fprintf(stderr, "\n");
        if (getenv("HK_PTCTRACE")[0] == '2') for (int i = 0; i < nems; i++) if (ems[i].owned)
            fprintf(stderr, "   [ptc obj %d] %s at %.1f,%.1f F %.2f play %d emit %d alive %d max %d rate %.1f\n", i, ems[i].owner ? ems[i].owner->name : "(gone)", ems[i].ox, ems[i].oy, ems[i].F, ems[i].playing, ems[i].emitting, ems[i].alive, ems[i].maxn, ems[i].rate.lo[0]); } }
#endif
}

/* ---------------- drawing ---------------- */
static void draw_parts(float camx, float camy, float flo, float fhi, int slo, int shi);
void particles_draw_range(float camx, float camy, float flo, float fhi) { draw_parts(camx, camy, flo, fhi, 0, 1); }
// emitters in a higher sorting layer: Unity draws them after everything in the lower ones, foreground included
void particles_draw_over(float camx, float camy, int slo, int shi) { draw_parts(camx, camy, -1e9f, 1e9f, slo, shi); }

static void draw_parts(float camx, float camy, float flo, float fhi, int slo, int shi) {
    if (!nparts) return;
    for (int pass = 0; pass < 2; pass++) {      // alpha-blended first, then additive
        bool any = false;
        for (int j = 0; j < nparts; j++) {
            Part *q = &parts[j]; Emitter *m = &ems[q->em];
#ifdef __HOST__
            { static int nd2; if (pass == 0 && getenv("HK_PTCDRAW") && m->owner && strstr(m->owner->name, getenv("HK_PTCDRAW")) && nd2++ < 20) fprintf(stderr, "[ptcpre] sl %d [%d,%d) F %.2f [%.2f,%.2f) add %d world %d q %.1f,%.1f o %.1f,%.1f cam %.1f,%.1f size %.2f\n", m->sl, slo, shi, m->F, flo, fhi, m->add, m->world, q->x, q->y, m->ox, m->oy, camx, camy, q->size); }
#endif
            if (m->sl < slo || m->sl >= shi) continue;
            if (m->F < flo || m->F >= fhi || m->add != (pass == 1)) continue;
            float ox = 0, oy = 0;
            if (!m->world) { ox = m->ox; oy = m->oy; }
            float x = (q->x + ox - camx) * m->F * PPU + TOP_W / 2, y = (camy - q->y - oy) * m->F * PPU + TOP_H / 2;
            float tn = q->age / q->life;
            float size = q->size * (m->hasSzt ? cval(&m->szt, tn, q->r[2]) : 1) * m->F * PPU;     // px
            if (size < 0.5f) continue;
            float mx = m->maxsz * TOP_H; if (m->maxsz > 0 && size > mx) size = mx;
            if (x + size < 0 || x - size > TOP_W || y + size < 0 || y - size > TOP_H) continue;
            float c[4] = { q->c[0], q->c[1], q->c[2], q->c[3] };
            if (m->hasColt) { float g[4]; gval(&m->colt, tn, q->r[1], g); for (int k = 0; k < 4; k++) c[k] *= g[k]; }
            for (int k = 0; k < 4; k++) c[k] *= m->tint[k];
            if (c[3] <= 0.01f) continue;
            int fr = m->fr;
            if (m->hasUV && m->uvN > 1) {
                float f = cval(&m->stf, tn, q->r[0]) + cval(&m->fot, tn, q->r[0]) * m->uvN * m->uvCyc;
                int fi = (int)f % m->uvN; if (fi < 0) fi += m->uvN; fr += fi;
            }
            if (fr < 0 || fr >= m->set->nframes) continue;
            const Frame *f = &m->set->frames[fr];
            float sx = size / (f->w > 0 ? f->w : 1), sy = size / (f->h > 0 ? f->h : 1), ang = -q->rot;
            if (m->stretch) {       // stretched billboard: along the velocity, length = size * lengthScale + speed * velocityScale
                float sp = sqrtf(q->vx * q->vx + q->vy * q->vy);
                float len = (q->size * m->ls + sp * m->vs) * m->F * PPU;
                sx = len / (f->w > 0 ? f->w : 1);
                ang = atan2f(q->vy, q->vx) * 180 / (float)M_PI;
            }
            if (!any) { if (pass == 1) gfx_additive(true); any = true; }
            float a = c[3] > 1 ? 1 : c[3];
#ifdef __HOST__
            { static int nd; if (getenv("HK_PTCDRAW") && m->owner && strstr(m->owner->name, getenv("HK_PTCDRAW")) && nd++ < 40) fprintf(stderr, "[ptcdraw] %s x %.0f y %.0f size %.1f a %.2f rgb %.2f %.2f %.2f fr %d (%dx%d)\n", m->owner->name, x, y, size, a, c[0], c[1], c[2], fr, f->w, f->h); }
#endif
            spr_draw_col(m->set, fr, x, y, sx, sy, ang, c[0] > 1 ? 1 : c[0], c[1] > 1 ? 1 : c[1], c[2] > 1 ? 1 : c[2], a, 0);
        }
        if (any && pass == 1) gfx_additive(false);
    }
}

int particles_alive(void) { return nparts; }

/* ---------------- objects (phase 2: emitters on runtime objects) ---------------- */
void particles_obj_forget(struct UObj *o) {      // destroyed: world-space particles live on where they are
    for (int i = 0; i < nems; i++) if (ems[i].used && ems[i].owner == o) {
        ems[i].owner = NULL; ems[i].emitting = false;
        if (!ems[i].world) kill_parts(i);
    }
}
