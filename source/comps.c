// Ports of the game's MonoBehaviours used by enemies (from the decompiled Assembly-CSharp).
#include "comps.h"
#include "particles.h"
#include "comps2.h"
#include "charms.h"
#include "fsm.h"
#include "world.h"
#include "hero.h"
#include "audio.h"
#include "pd.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

void game_spawn_geo(float x, float y, int n);
void game_freeze(float t);
void game_shake(float t);
void uo_anim_set_callbacks(UObj *o, Fsm *f, const char *complete, const char *trigger, bool setComplete, bool setTrigger);

static float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
static float ff(UComp *c, const char *k, float d) { return (float)jnum(jget(c->f, k), d); }
static int fi(UComp *c, const char *k, int d) { return jint(jget(c->f, k), d); }
static bool fb(UComp *c, const char *k, bool d) { return jbool(jget(c->f, k), d); }
static const char *fs(UComp *c, const char *k, const char *d) { return jstr(jget(c->f, k), d); }
static int ftmpl(UComp *c, const char *k) { JVal *v = jget(c->f, k); JVal *p = v ? jget(v, "prefab") : NULL; return p ? uo_template_by_key(p->s) : -1; }
static UObj *fobj(UObj *o, UComp *c, const char *k);

CompType comp_type_of(const char *cls) {
    static const struct { const char *n; CompType t; } m[] = {
        { "HealthManager", C_HEALTH }, { "DamageHero", C_DAMAGEHERO }, { "Walker", C_WALKER }, { "Recoil", C_RECOIL },
        { "EnemyDeathEffects", C_DEATHFX }, { "EnemyDeathEffectsUninfected", C_DEATHFX }, { "EnemyDeathEffectsNoEffect", C_DEATHFX },
        { "EnemyDeathEffectsBubble", C_DEATHFX }, { "EnemyDeathEffectsDream", C_DEATHFX },
        { "Corpse", C_CORPSE }, { "CorpseDeathStun", C_CORPSE }, { "CorpseChunker", C_CORPSE }, { "CorpseHatcher", C_CORPSE },
        { "CorpseFungusExplode", C_CORPSE }, { "CorpseGoopExplode", C_CORPSE }, { "CorpseSpineBurst", C_CORPSE }, { "CorpseZomHive", C_CORPSE },
        { "CorpseDeathStunChunker", C_CORPSE },
        { "AlertRange", C_ALERTRANGE }, { "LineOfSightDetector", C_LOS }, { "DeactivateAfter2dtkAnimation", C_DEACT_ANIM },
        { "NonBouncer", C_NONBOUNCER }, { "PlayMakerUnity2DProxy", C_PROXY2D }, { "Climber", C_CLIMBER }, { "Crawler", C_CRAWLER },
        { "GeoControl", C_GEO }, { "RecycleAfter2dtkAnimation", C_RECYCLE_ANIM }, { "AutoRecycleSelf", C_AUTORECYCLE },
        { "PlayAudioAndRecycle", C_PLAYAUDIO }, { "ObjectBounce", C_BOUNCE }, { "PersistentBoolItem", C_PBI }, { "FSMActivator", C_FSMACTIVATOR }, { "EnemyBullet", C_BULLET },
        { "DeactivateIfPlayerdataTrue", C_DEACT_PD_TRUE }, { "DeactivateIfPlayerdataFalse", C_DEACT_PD_FALSE },
        { "DeactivateIfPlayerdataFalseDelayed", C_DEACT_PD_FALSE }, { "FadeGroup", C_FADEGROUP },
        { "Breakable", C_BREAKABLE }, { "BreakablePoleSimple", C_POLE }, { "BreakablePoleTopLand", C_POLETOP },
        { "BreakableInfectedVine", C_VINE }, { "GeoRock", C_GEOROCK }, { "DropPlatform", C_DROPPLAT }, { "LiftPlatform", C_LIFTPLAT },
        { "FlipPlatform", C_FLIPPLAT }, { "BounceShroom", C_BOUNCESHROOM }, { "HealthCocoon", C_HEALTHCOCOON },
        { "StalactiteControl", C_STALACTITE }, { "ShadowGateColliderControl", C_SHADOWGATE }, { "ConveyorBelt", C_CONVEYOR },
        { "ConveyorZone", C_CONVEYORZONE }, { "ConveyorSpeedZone", C_CONVEYORSPEED }, { "MusicRegion", C_MUSICREGION },
        { "SetPosIfPlayerdataBool", C_SETPOS_PD }, { "DeactivateInDarknessWithoutLantern", C_DARK_NOLANTERN },
        { "BridgeLever", C_BRIDGELEVER }, { "BridgeSection", C_BRIDGESECTION }, { "JellyEgg", C_JELLYEGG },
        { "ActivateChildrenOnContact", C_ACTCHILDREN }, { "SendEnemyMessageTrigger", C_ENEMYMSG }, { "EnemyWakeRegion", C_WAKEREGION },
        { "DamageEnemies", C_DAMAGEENEMIES }, { "PersistentIntItem", C_PII }, { "SlopePlat", C_SLOPEPLAT }, { "RuinsLift", C_RUINSLIFT },
        { "LiftChain", C_LIFTCHAIN }, { "DreamPlatform", C_DREAMPLAT }, { "TriggerEnterEvent", C_TRIGGERENTER },
        { "CollisionEnterEvent", C_COLLENTER }, { "PushableRubble", C_PUSHRUBBLE }, { "GrubBGControl", C_GRUBBG },
        { "BasicSpriteAnimator", C_SPRANIM }, { "TouchShake", C_TOUCHSHAKE }, { "DamageEffectTicker", C_DMGTICKER }, { "ExtraDamageable", C_EXTRADMG }, { "SpinSelf", C_SPINSELF }, { "SpinSelfSimple", C_SPINSIMPLE }, { "SetZ", C_SETZ },
        { "SetZRandom", C_SETZRAND }, { "RandomRotation", C_RANDROT }, { "RandomRotationRange", C_RANDROTRANGE }, { "RandomScale", C_RANDSCALE },
        { "DeactivateAfterDelay", C_DEACTDELAY }, { "DisableAfterTime", C_DISABLETIME }, { "SnapToGround", C_SNAPGROUND },
        { "SoulOrb", C_SOULORB }, { "ScuttlerControl", C_SCUTTLER }, { "EventRegister", C_EVENTREG }, { "VinePlatform", C_VINEPLAT },
        { "VinePlatformCut", C_VINECUT }, { "SteepSlope", C_STEEPSLOPE }, { "EnemyKillEventListener", C_KILLLISTENER }, { "DreamPlant", C_DREAMPLANT }, { "DreamPlantOrb", C_DREAMORB }, { "EnemyDreamnailReaction", C_DREAMREACT }, { "InfectedEnemyEffects", C_INFECTEDFX }, { "EnemyHitEffectsUninfected", C_UNINFFX },
        { "EnemyHitEffectsShade", C_UNINFFX }, { "EnemyHitEffectsGhost", C_UNINFFX }, { "EnemyHitEffectsArmoured", C_UNINFFX }, { "SimpleSpriteFade", C_SPRFADE }, { "BigCentipede", C_CENTIPEDE }, { "BigCentipedeSection", C_CENTISECTION }, { "BossSceneController", C_BOSSSCENE }, { "BossSequenceDoor", C_BOSSSEQDOOR }, { "SpatterOrange", C_SPATTER }, { "ColorFader", C_COLORFADER }, { "ActionButtonIcon", C_ACTIONICON }, { "WaveEffectControl", C_WAVE }, { "ParticleSystemAutoRecycle", C_PSRECYCLE }, { "ParticleSystemAutoDestroy", C_PSRECYCLE }, { "SendPlaymakerEventOnEnable", C_SENDONENABLE }, { "SpawnJarControl", C_SPAWNJAR },
    };
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) if (!strcmp(m[i].n, cls)) return m[i].t;
    return C_UNKNOWN;
}

UObj *comps_tree_obj(UObj *o, int idx) {
    if (o && o->treeArr && idx >= 0 && idx < o->treeN) return o->treeArr[idx];
    return NULL;
}
static UObj *fobj(UObj *o, UComp *c, const char *k) {
    // references inside the same tree are {"o": index}; we resolve against the object's root tree by walking
    JVal *v = jget(c->f, k);
    if (!v || v->t != J_OBJ) return NULL;
    JVal *oi = jget(v, "o");
    if (!oi) return NULL;
    if (o->treeArr) return comps_tree_obj(o, jint(oi, -1));
    // find the FSM tree of the root (all objects of a tree share it); fall back to name search
    UObj *r = uo_root(o);
    for (int i = 0; i < r->nfsm; i++) { Fsm *f = r->fsm[i]; int idx = jint(oi, -1); if (f->tree && idx >= 0 && idx < f->ntree) return f->tree[idx]; }
    for (int i = 0; i < o->nfsm; i++) { Fsm *f = o->fsm[i]; int idx = jint(oi, -1); if (f->tree && idx >= 0 && idx < f->ntree) return f->tree[idx]; }
    return NULL;
}

/* ================= FadeGroup ================= */
// fades the alpha of listed SpriteRenderers / TextMeshPros (FadeGroupUp / FadeGroupDown actions)
typedef struct { int state; float timer, alpha, from, dur; } FG;
static UObj *fobj_v(UObj *o, JVal *v) {
    JVal *oi = v && v->t == J_OBJ ? jget(v, "o") : NULL; if (!oi) return NULL;
    if (o->treeArr) return comps_tree_obj(o, jint(oi, -1));
    int idx = jint(oi, -1); UObj *r = uo_root(o);
    for (int i = 0; i < r->nfsm; i++) { Fsm *f = r->fsm[i]; if (f->tree && idx >= 0 && idx < f->ntree) return f->tree[idx]; }
    for (UObj *q = o; q; q = q->parent) for (int i = 0; i < q->nfsm; i++) { Fsm *f = q->fsm[i]; if (f->tree && idx >= 0 && idx < f->ntree) return f->tree[idx]; }
    return NULL;
}
static void fg_apply(UObj *o, UComp *c, float a) {
    JVal *l = jget(c->f, "spriteRenderers");
    for (JVal *e = l ? l->child : NULL; e; e = e->next) { UObj *t = fobj_v(o, e); if (t) t->sr.a = a; }
    l = jget(c->f, "texts");
    for (JVal *e = l ? l->child : NULL; e; e = e->next) { UObj *t = fobj_v(o, e); if (t) t->tmp.a = a; }
}
void fadegroup_set(UObj *o, bool up, bool fast) {
    UComp *c = uo_comp(o, C_FADEGROUP); if (!c) return;
    FG *g = c->st; if (!g) { g = calloc(1, sizeof(FG)); c->st = g; }
    g->state = up ? 1 : 2; g->timer = 0; g->from = g->alpha;
    g->dur = up ? ff(c, "fadeInTime", 0.2f) : (fast ? ff(c, "fadeOutTimeFast", 0.2f) : ff(c, "fadeOutTime", 0.2f));
}
static void fg_update(UObj *o, UComp *c, float dt) {
    FG *g = c->st; if (!g || !g->state) return;
    g->timer += dt;
    float to = g->state == 1 ? ff(c, "fullAlpha", 1) : ff(c, "downAlpha", 0);
    float t = g->dur > 0 ? g->timer / g->dur : 1; if (t >= 1) { t = 1; g->state = 0; }
    g->alpha = g->from + (to - g->from) * t;
    fg_apply(o, c, g->alpha);
}

/* ================= HealthManager ================= */
typedef struct { UObj *battleScene; int hp; bool isDead, specialDied, invincible, special, deathReset, damageOverride, preventInvEffect; int invDir, enemyType;
                 float evasion; int geoS, geoM, geoL; float ex, ey; } HM;

static HM *hm_of(UObj *o) { UComp *c = uo_comp(o, C_HEALTH); return c ? (HM *)c->st : NULL; }
bool hm_is_dead(UObj *o) { HM *h = hm_of(o); return h && h->isDead; }
bool hm_died(UObj *o) { HM *h = hm_of(o); return h && (h->isDead || h->specialDied); }
bool hm_invincible(UObj *o) { HM *h = hm_of(o); return h && h->invincible; }
void hm_set_invincible(UObj *o, bool inv) { HM *h = hm_of(o); if (h) h->invincible = inv; }
void hm_set_invincible_dir(UObj *o, int d) { HM *h = hm_of(o); if (h) h->invDir = d; }
void hm_set_geo(UObj *o, int s, int m, int l) { HM *h = hm_of(o); if (h) { h->geoS = s; h->geoM = m; h->geoL = l; } }
int hm_hp(UObj *o) { HM *h = hm_of(o); return h ? h->hp : 0; }
void hm_set_battle_scene(UObj *o, UObj *bs) { HM *h = hm_of(o); if (h) h->battleScene = bs; }
int hm_get_hp(UObj *o) { HM *h = hm_of(o); return h ? h->hp : 0; }
// HealthManager.Die: the battle scene it belongs to counts one enemy fewer ("Battle Enemies" in its Battle Control)
static void hm_battle_scene_kill(HM *h) {
    if (!h->battleScene) return;
    for (int k = 0; k < h->battleScene->nfsm; k++) { FVar *v = fsm_var(h->battleScene->fsm[k], "Battle Enemies"); if (v) { if (v->t == V_FLOAT) v->f -= 1; else v->i -= 1; break; } }
    h->battleScene = NULL;
}
void hm_set_hp(UObj *o, int hp) { HM *h = hm_of(o); if (h) { h->hp = hp; if (hp > 0) h->specialDied = false; } }
void hm_set_special(UObj *o, bool sp) { HM *h = hm_of(o); if (h) h->special = sp; }
void hm_set_damage_override(UObj *o, bool d) { HM *h = hm_of(o); if (h) h->damageOverride = d; }
void hm_set_dead(UObj *o, bool d) { HM *h = hm_of(o); if (h) h->isDead = d; }
void hm_set_death_reset(UObj *o, bool r) { HM *h = hm_of(o); if (h) h->deathReset = r; }

static int cardinal(float deg) {   // DirectionUtils.GetCardinalDirection
    float a = fmodf(deg, 360); if (a < 0) a += 360;
    if (a < 45 || a >= 315) return 0;
    if (a < 135) return 1;
    if (a < 225) return 2;
    return 3;
}
// HealthManager.IsBlockingByDirection. Spells (and Sharp Shadow) go through the shell of a "Spell Vulnerable" enemy
// however invincible it is: that's how the Elder Baldurs, curled up against the nail, die to Vengeful Spirit (#8)
static bool spell_through(UObj *o, AttackType type) {
    return (type == ATK_SPELL || type == ATK_SHARPSHADOW) && !strcmp(o->tag, "Spell Vulnerable");
}
static bool blocking(HM *h, int dir) {
    if (!h->invincible) return false;
    if (h->invDir == 0) return true;
    switch (dir) {
        case 0: return h->invDir == 1 || h->invDir == 5 || h->invDir == 8 || h->invDir == 10;
        case 1: return h->invDir == 2 || (unsigned)(h->invDir - 5) <= 4u;
        case 2: return h->invDir == 3 || h->invDir == 6 || h->invDir == 9 || h->invDir == 11;
        case 3: return h->invDir == 4 || (unsigned)(h->invDir - 7) <= 4u;
    }
    return false;
}

static void recoil_by_direction(UObj *o, int dir, float mag);
void comps_recoil_by_direction(UObj *o, float angle, float mag) { int d = ((int)floorf((angle + 45) / 90)) & 3; recoil_by_direction(o, d, mag); }
static void deathfx_receive(UObj *o, float dir, bool hasDir, bool reset, bool spell, bool watery);

// The save key is the object's name when it first asks (PersistentBoolItem keeps its id from Start): a freed grub's
// bottle renames itself "Collected", and keying on the new name lost it, so the grub was back after leaving the room.
static void pbi_key(UObj *o, UComp *c, char *buf, int n) {
    if (!c->st) { char *k = malloc(96); snprintf(k, 96, "%s:%s:%s", fb(c, "semiPersistent", false) ? "pbs" : "pbi", uo_room_name(), o->name); c->st = k; }
    snprintf(buf, n, "%s", (const char *)c->st);
}
void comps_pbi_key(UObj *o, UComp *c, char *buf, int n) { pbi_key(o, c, buf, n); }
// an enemy the save records as killed stays gone even when a script switches its group back on (Crossroads rooms switch
// their infected / non-infected groups on with ActivateAllChildren a frame in): it came back walking around but already
// dead, so the nail bounced off it without hurting it
bool hm_saved_dead(UObj *o) {
    HM *h = hm_of(o); if (!h || !h->isDead) return false;
    UComp *pc = uo_comp(o, C_PBI); if (!pc) return false;
    char k[96]; pbi_key(o, pc, k, sizeof k); return pd_bool(k);
}
void hm_die(UObj *o, float direction, AttackType type) {
    HM *h = hm_of(o); if (!h || h->isDead) return;
    uo_send_event(o, "ZERO HP");
    if (h->special) { h->evasion = 0.2f; h->specialDied = true; hm_battle_scene_kill(h); return; }   // OnDeath still fires (boss scenes count it)
    h->isDead = true;
    hm_battle_scene_kill(h);
    { UComp *pc = uo_comp(o, C_PBI); if (pc && !fb(pc, "dontSave", false)) { char k[96]; pbi_key(o, pc, k, sizeof k); pd_set_bool(k, true); } }
    UComp *dh = uo_comp(o, C_DAMAGEHERO); if (dh && dh->st) *(int *)dh->st = 0;
    float x = uo_wx(o) + h->ex, y = uo_wy(o) + h->ey;
    if (type == ATK_SPLATTER) { game_shake(0.2f); uo_destroy(o); return; }
    int n = charm_geo_drop(h->geoS + h->geoM * 5 + h->geoL * 25);
    if (n > 0) game_spawn_geo(x, y, n > 60 ? 60 : n);
    UComp *df = uo_comp(o, C_DEATHFX);
    if (df) deathfx_receive(o, direction, true, h->deathReset, type == ATK_SPELL, type == ATK_ACID || type == ATK_RUINSWATER);
    else uo_destroy(o);
}

// why a nail swing that touched this enemy did no damage (for the log: "nail touched X, no damage: ...")
const char *hm_no_hit_reason(UObj *o, int damage, float direction) {
    HM *h = hm_of(o); if (!h) return "no health";
    if (h->isDead) return "already dead";
    if (damage <= 0) return "nail damage 0";
    UComp *hc = uo_comp(o, C_HEALTH); if (hc && !hc->en) return "health component off";
    if (h->evasion > 0) return "just hit (evasion)";
    if (blocking(h, cardinal(direction))) return h->invincible ? "invincible" : "blocked from this side";
    return NULL;
}
bool hm_hit(UObj *o, int damage, float direction, AttackType type, float magnitude) {
    HM *h = hm_of(o);
    if (!h || h->isDead || h->evasion > 0 || damage <= 0) return false;
    int dir = cardinal(direction);
    UComp *hc = uo_comp(o, C_HEALTH);
    if (!spell_through(o, type) && blocking(h, dir)) {
        uo_send_event(o, "BLOCKED HIT"); uo_send_event(o, "HIT");
        if (!h->preventInvEffect) {
            if (type == ATK_NAIL) { extern void hero_recoil_left(void); extern void hero_recoil_right(void); if (dir == 0) hero_recoil_left(); else if (dir == 2) hero_recoil_right(); }
            game_freeze(0.08f); game_shake(0.1f);
            sfx_play("sword_tink", 0.9f);
            switch (dir) { case 0: uo_send_event(o, "BLOCKED HIT R"); break; case 2: uo_send_event(o, "BLOCKED HIT L"); break; case 1: uo_send_event(o, "BLOCKED HIT U"); break; case 3: uo_send_event(o, "BLOCKED DOWN"); break; }
        }
        h->evasion = 0.15f;
        return true;
    }
    (void)hc;
    uo_send_event(o, "HIT");
    uo_send_event(o, "TOOK DAMAGE");
    recoil_by_direction(o, dir, magnitude);
    if ((type == ATK_NAIL) && h->enemyType != 3 && h->enemyType != 6) { P.soul += charm_soul_per_hit(11); if (P.soul > 99) P.soul = 99; }
    // hit effects: flash + damage sound + IHitEffectReciever (InfectedEnemyEffects / EnemyHitEffects*)
    o->flash = 0.12f;
    if (!comps_hit_effect(o, direction)) sfx_play("enemy_damage", 1.0f);
    int dmg = h->damageOverride ? 1 : damage;
#ifdef __HOST__
    if (getenv("HK_ONEHIT")) dmg = 9999;      // test: everything dies in one hit
#endif
    h->hp = h->hp - dmg < -50 ? -50 : h->hp - dmg;
#ifdef __HOST__
    if (getenv("HK_FSMTRACE")) fprintf(stderr, "[hit %d] %s dmg=%d hp=%d dir=%.0f\n", uo_frame, o->name, dmg, h->hp, direction);
#endif
    { extern bool g_testHeroLog; extern void hk_log(const char *, ...); if (g_testHeroLog) hk_log("hit %s dmg %d hp %d", o->name, dmg, h->hp); }
    if (h->hp > 0) {
        h->evasion = 0.2f;
        Fsm *st = fsm_find_on(o, "Stun Control"); if (!st) st = fsm_find_on(o, "Stun");
        if (st) fsm_event(st, "STUN DAMAGE");
    } else {
        sfx_play("enemy_death_sword", 1.0f);
        hm_die(o, direction, type);
    }
    return true;
}

/* ================= Recoil ================= */
typedef struct { int state; bool freezeInPlace, preventRecoilUp, stopVxUp; float speedBase, duration, timeLeft, speed; int dir; bool sweeping; bool skipFreeze; } RC;
void recoil_set_speed(UObj *o, float s) { UComp *c = uo_comp(o, C_RECOIL); if (c && c->st) ((RC *)c->st)->speedBase = s; }
void recoil_set_freeze(UObj *o, bool fr) { UComp *c = uo_comp(o, C_RECOIL); if (c && c->st) ((RC *)c->st)->freezeInPlace = fr; }
static void recoil_cancel(UObj *o, RC *r);
static void climber_stun(UObj *o);
static void recoil_by_direction(UObj *o, int dir, float mag) {
    UComp *c = uo_comp(o, C_RECOIL); if (!c || !c->st) return;
    RC *r = c->st;
    if (r->state != 0) return;
    if (r->freezeInPlace) {
        if (r->skipFreeze) { climber_stun(o); r->state = 0; return; }
        r->state = 1; if (o->rb.has) { o->rb.vx = o->rb.vy = 0; }
        Fsm *cc = fsm_find_on(o, "Climber Control"); if (cc) fsm_event(cc, "FREEZE IN PLACE");
        r->timeLeft = r->duration;
        return;
    }
    if (dir == 1 && r->preventRecoilUp) return;
    r->state = 2; r->speed = r->speedBase * mag; r->dir = dir; r->sweeping = true; r->timeLeft = r->duration;
    switch (dir) {
        case 2: uo_send_event(o, "RECOIL HORIZONTAL"); uo_send_event(o, "HIT LEFT"); break;
        case 0: uo_send_event(o, "RECOIL HORIZONTAL"); uo_send_event(o, "HIT RIGHT"); break;
        case 3: uo_send_event(o, "HIT DOWN"); break;
        case 1: uo_send_event(o, "HIT UP"); break;
    }
}
// Sweep: three rays from the collider's leading edge; returns the clipped distance
static float sweep(UObj *o, int dir, float dist, float ox, float oy) {
    float x0, y0, x1, y1;
    if (!uo_body_aabb(o, &x0, &y0, &x1, &y1)) return dist;
    x0 += ox; x1 += ox; y0 += oy; y1 += oy;
    float dx = dir == 0 ? 1 : dir == 2 ? -1 : 0, dy = dir == 1 ? 1 : dir == 3 ? -1 : 0;
    const float skin = 0.1f; float best = dist;
    for (int i = 0; i < 3; i++) {
        float t = i / 2.0f;
        float sx, sy;
        // Box2D keeps resting bodies a contact offset off the floor/wall, so the edge rays of Sweep.cs never graze the
        // surface the body rests on; the cell grid has no such gap, so inset the edge rays a little
        const float in = 0.03f;
        if (dx != 0) { sx = dx > 0 ? x1 : x0; sy = (y0 + in) + (y1 - y0 - 2 * in) * t; }
        else { sy = dy > 0 ? y1 : y0; sx = (x0 + in) + (x1 - x0 - 2 * in) * t; }
        sx -= dx * skin; sy -= dy * skin;
        float h;
        if (w_raycast(sx, sy, dx, dy, dist + skin, &h)) { float d = h - skin; if (d < best) best = d; }
    }
    return best < 0 ? 0 : best;
}
static bool sweep_hits(UObj *o, int dir, float dist, float ox, float oy) { return dist - sweep(o, dir, dist, ox, oy) > 1e-5f; }
static void recoil_cancel(UObj *o, RC *r) {
    if (r->state != 0) {
        r->state = 0;
        UComp *cw = uo_comp(o, C_CRAWLER);
        if (cw && cw->st) { float *v = (float *)cw->st; if (o->rb.has) { o->rb.vx = v[0]; o->rb.vy = v[1]; } }   // Crawler.OnCancelRecoil
    }
}
static void recoil_fixed(UObj *o, RC *r, float dt) {
    if (r->state == 1) {
        if (o->rb.has) o->rb.vx = o->rb.vy = 0;
        r->timeLeft -= dt; if (r->timeLeft <= 0) recoil_cancel(o, r);
    } else if (r->state == 2) {
        if (r->sweeping) {
            float d = r->speed * dt;
            float cl = sweep(o, r->dir, d, 0, 0);
            if (d - cl > 1e-5f) r->sweeping = false;
            if (cl > 1e-5f) {
                float dx = r->dir == 0 ? 1 : r->dir == 2 ? -1 : 0, dy = r->dir == 1 ? 1 : r->dir == 3 ? -1 : 0;
                uo_set_world_pos(o, uo_wx(o) + dx * cl, uo_wy(o) + dy * cl);
            }
        }
        r->timeLeft -= dt; if (r->timeLeft <= 0) recoil_cancel(o, r);
    }
}

/* ================= Walker ================= */
enum { WK_NOTREADY, WK_WAITING, WK_STOPPED, WK_WALKING, WK_TURNING };
typedef struct { int state, stopReason, facing, turningFacing; float walkLeft, pauseLeft, turnCool;
                 bool camOk, heroXOk; } WK;
static void wk_begin_walking(UObj *o, UComp *c, WK *w, int facing);
static void wk_begin_turning(UObj *o, UComp *c, WK *w, int facing);
static void wk_play(UObj *o, const char *clip) { if (clip && clip[0]) uo_anim_play(o, clip, false); }
static void wk_begin_stopped(UObj *o, UComp *c, WK *w, int reason) {
    w->state = WK_STOPPED; w->stopReason = reason;
    if (reason == 0) {
        wk_play(o, fs(c, "idleClip", NULL));
        if (o->rb.has) o->rb.vx = 0;
        if (fb(c, "pauses", false)) w->pauseLeft = frand(ff(c, "pauseTimeMin", 0), ff(c, "pauseTimeMax", 0));
        else {   // EndStopping
            if (w->facing == 0) { int f = rand() % 2 ? 1 : -1; if (w->facing == f) wk_begin_walking(o, c, w, f); else wk_begin_turning(o, c, w, f); }
            else if (rand() % 100 < fi(c, "turnAfterIdlePercentage", 0)) wk_begin_turning(o, c, w, -w->facing);
            else wk_begin_walking(o, c, w, w->facing);
        }
    }
}
static void wk_begin_walking(UObj *o, UComp *c, WK *w, int facing) {
    w->state = WK_WALKING;
    wk_play(o, fs(c, "walkClip", NULL));
    if (!fb(c, "preventScaleChange", false)) o->sx = facing * ff(c, "rightScale", 1);
    w->walkLeft = frand(ff(c, "pauseWaitMin", 0), ff(c, "pauseWaitMax", 0));
    if (o->rb.has) o->rb.vx = facing > 0 ? ff(c, "walkSpeedR", 0) : ff(c, "walkSpeedL", 0);
}
static void wk_end_turning(UObj *o, UComp *c, WK *w) { w->facing = w->turningFacing; wk_begin_walking(o, c, w, w->facing); }
static void wk_begin_turning(UObj *o, UComp *c, WK *w, int facing) {
    w->state = WK_TURNING; w->turningFacing = facing;
    if (fb(c, "preventTurn", false)) { wk_end_turning(o, c, w); return; }
    w->turnCool = ff(c, "turnPause", 0);
    if (o->rb.has) o->rb.vx = 0;
    wk_play(o, fs(c, "turnClip", NULL));
    uo_send_event(o, facing > 0 ? "TURN RIGHT" : "TURN LEFT");
}
static void wk_walk_or_turn(UObj *o, UComp *c, WK *w, int f) { if (w->facing == f) wk_begin_walking(o, c, w, f); else wk_begin_turning(o, c, w, f); }
static void wk_update(UObj *o, UComp *c, WK *w, float dt);
static void wk_start_moving(UObj *o, UComp *c, WK *w) {
    if (w->state == WK_STOPPED || w->state == WK_WAITING) {
        int f = w->facing ? w->facing : (rand() % 2 ? 1 : -1);
        wk_walk_or_turn(o, c, w, f);
    }
}
static void wk_update(UObj *o, UComp *c, WK *w, float dt) {
    w->turnCool -= dt;
    switch (w->state) {
        case WK_WAITING: {
            float cx = uo_hero_x() - uo_wx(o), cy = uo_hero_y() - uo_wy(o);
            if (!w->camOk && cx * cx + cy * cy < 3600) w->camOk = true;
            if (w->camOk && !w->heroXOk && fabsf(uo_hero_x() - ff(c, "waitHeroX", 0)) < 1) w->heroXOk = true;
            if (w->camOk && (!fb(c, "waitForHeroX", false) || w->heroXOk) && !fb(c, "startInactive", false) && !fb(c, "ambush", false)) {
                wk_begin_stopped(o, c, w, 0); wk_start_moving(o, c, w);
            }
            break; }
        case WK_STOPPED:
            if (w->stopReason == 0) {
                w->pauseLeft -= dt;
                if (w->pauseLeft <= 0) {
                    if (w->facing == 0) wk_walk_or_turn(o, c, w, rand() % 2 ? 1 : -1);
                    else if (rand() % 100 < fi(c, "turnAfterIdlePercentage", 0)) wk_begin_turning(o, c, w, -w->facing);
                    else wk_begin_walking(o, c, w, w->facing);
                }
            }
            break;
        case WK_WALKING: {
            if (w->turnCool <= 0) {
                float x0, y0, x1, y1; uo_body_aabb(o, &x0, &y0, &x1, &y1);
                float ext = (x1 - x0) / 2;
                if (sweep_hits(o, 1 - w->facing, ext + 0.5f, 0, 0)) { wk_begin_turning(o, c, w, -w->facing); return; }
                UObj *los = fobj(o, c, "lineOfSightDetector"), *ar = fobj(o, c, "alertRange");
                if (!los) for (int k = 0; k < o->ncomp; k++) if (o->comp[k].type == C_LOS) los = o;
                if (!fb(c, "preventTurningToFaceHero", false) && ((uo_hero_x() > uo_wx(o)) != (w->facing > 0)) && los && los_can_see(los) && ar && alert_in_range(ar)) {
                    wk_begin_turning(o, c, w, -w->facing); return;
                }
                if (!fb(c, "ignoreHoles", false) && !sweep_hits(o, 3, 0.25f, (ext + 0.5f + ff(c, "edgeXAdjuster", 0)) * w->facing, 0)) { wk_begin_turning(o, c, w, -w->facing); return; }
            }
            if (fb(c, "pauses", false)) { w->walkLeft -= dt; if (w->walkLeft <= 0) { wk_begin_stopped(o, c, w, 0); return; } }
            if (o->rb.has) o->rb.vx = w->facing > 0 ? ff(c, "walkSpeedR", 0) : ff(c, "walkSpeedL", 0);
            break; }
        case WK_TURNING:
            if (o->rb.has) o->rb.vx = 0;
            if (!o->an.playing) wk_end_turning(o, c, w);
            break;
    }
}
static WK *wk_of(UObj *o, UComp **cc) { UComp *c = uo_comp(o, C_WALKER); if (cc) *cc = c; return c ? (WK *)c->st : NULL; }
void walker_start(UObj *o) { UComp *c; WK *w = wk_of(o, &c); if (!w) return; if (w->state == WK_STOPPED || w->state == WK_WAITING) { c->f = c->f; wk_start_moving(o, c, w); } wk_update(o, c, w, 0); }
void walker_stop(UObj *o, int reason) { UComp *c; WK *w = wk_of(o, &c); if (w) wk_begin_stopped(o, c, w, reason); }
void walker_go(UObj *o, int facing) {
    UComp *c; WK *w = wk_of(o, &c); if (!w) return;
    w->turnCool = -1e-6f;
    if (w->state == WK_STOPPED || w->state == WK_WALKING) wk_walk_or_turn(o, c, w, facing);
    else if (w->state == WK_TURNING && w->facing == facing) wk_begin_walking(o, c, w, w->facing);
    wk_update(o, c, w, 0);
}
void walker_change_facing(UObj *o, int facing) { UComp *c; WK *w = wk_of(o, &c); if (!w) return; if (w->state == WK_TURNING) { w->turningFacing = facing; w->facing = -facing; } else w->facing = facing; }
void walker_clear_turn_cooldown(UObj *o) { WK *w = wk_of(o, NULL); if (w) w->turnCool = -1e-6f; }

/* ================= AlertRange / LineOfSightDetector ================= */
typedef struct { bool inRange; } AR;
bool alert_in_range(UObj *ro) { UComp *c = uo_comp(ro, C_ALERTRANGE); return c && c->st && ((AR *)c->st)->inRange && uo_active(ro); }
typedef struct { bool canSee; } LOS;
bool los_can_see(UObj *o) { UComp *c = uo_comp(o, C_LOS); return c && c->st && ((LOS *)c->st)->canSee; }
static void los_update(UObj *o, UComp *c) {
    LOS *l = c->st;
    JVal *rs = jget(c->f, "alertRanges");
    bool any = false; int n = rs ? rs->n : 0;
    if (n > 0) {
        UObj *r = uo_root(o); Fsm *tf = r->nfsm ? r->fsm[0] : (o->nfsm ? o->fsm[0] : NULL);
        for (JVal *e = rs->child; e; e = e->next) {
            int idx = jint(jget(e, "o"), -1); UObj *ro = NULL;
            if (tf && idx >= 0 && idx < tf->ntree) ro = tf->tree[idx];
            if (!ro) { for (UObj *ch = o->child; ch; ch = ch->next) if (uo_comp(ch, C_ALERTRANGE)) { ro = ch; break; } }
            if (ro && alert_in_range(ro)) any = true;
        }
        if (!any) { l->canSee = false; return; }
    }
    float x = uo_wx(o), y = uo_wy(o), hx = uo_hero_x(), hy = uo_hero_y(), dx = hx - x, dy = hy - y, d = sqrtf(dx * dx + dy * dy), h;
    l->canSee = !(d > 0.001f && w_raycast(x, y, dx, dy, d, &h));
}

/* ================= DamageHero ================= */
int damagehero_amount(UObj *o) { UComp *c = uo_comp(o, C_DAMAGEHERO); return c && c->st ? *(int *)c->st : 0; }
void damagehero_set(UObj *o, int d) { UComp *c = uo_comp(o, C_DAMAGEHERO); if (c && c->st) *(int *)c->st = d; }

/* ================= EnemyDeathEffects / Corpse ================= */
// EnemyDeathEffects.RecordKillForJournal: killedX, killsX counts down to the full entry, newDataX
void comps_record_kill(const char *name) {
    if (!name || !name[0]) return;
    char b[64], k[64], nd[64]; snprintf(b, sizeof b, "killed%s", name); snprintf(k, sizeof k, "kills%s", name); snprintf(nd, sizeof nd, "newData%s", name);
    bool first = !pd_bool(b), full = false;
    if (first) { pd_set_bool(b, true); pd_set_bool(nd, true); }
    int n = pd_int(k); if (n > 0) { n--; pd_set_int(k, n); if (n <= 0) { full = true; pd_set_bool(nd, true); } }
    if (pd_bool("hasJournal")) {
        if (full) pd_set_int("journalEntriesCompleted", pd_int("journalEntriesCompleted") + 1);
        else if (first) pd_set_int("journalNotesCompleted", pd_int("journalNotesCompleted") + 1);
        if (full || first) { extern void msgs_journal_update(bool full); msgs_journal_update(full); }
    }
}
static void deathfx_receive(UObj *o, float dir, bool hasDir, bool reset, bool spell, bool watery) {
    UComp *c = uo_comp(o, C_DEATHFX);
    if (!c) { uo_destroy(o); return; }
    comps_record_kill(fs(c, "playerDataName", NULL));
    int ct = ftmpl(c, "corpsePrefab");
    if (ct >= 0) {
        JVal *sp = jget(c->f, "corpseSpawnPoint");
        float spx = (float)jnum(jget(sp, "x"), 0) * uo_wsx(o), spy = (float)jnum(jget(sp, "y"), 0) * uo_wsy(o);
        UObj *corpse = uo_spawn(ct, uo_wx(o) + spx, uo_wy(o) + spy, 0);
        if (corpse) {
            corpse->sx = fabsf(corpse->sx) * (uo_wsx(o) < 0 ? -1 : 1);
            corpse->lz = 0.0085f;
            int cd = cardinal(dir);
            float speed = ff(c, "corpseFlingSpeed", 0), ang;
            bool low = fb(c, "lowCorpseArc", false), facesR = fb(c, "corpseFacesRight", false);
            float sgn = uo_wsx(o) < 0 ? -1.0f : 1.0f;
            switch (cd) {
                case 0: ang = low ? 10 : 60; corpse->sx = corpse->sx * (facesR ? -1.0f : 1.0f) * sgn; break;
                case 2: ang = low ? 170 : 120; corpse->sx = corpse->sx * (facesR ? 1.0f : -1.0f) * sgn; break;
                case 3: ang = 270; break;
                case 1: ang = frand(75, 105); speed *= 1.3f; break;
                default: ang = 90;
            }
            if (!hasDir) speed = 0;
            if (corpse->rb.has && !corpse->rb.kin && !watery) {
                corpse->rb.vx = cosf(ang * (float)M_PI / 180) * speed; corpse->rb.vy = sinf(ang * (float)M_PI / 180) * speed;
            }
            (void)spell;
            uo_set_active(corpse, true);    // EmitCorpse: corpse.SetActive(true) (Volatile Mosskin's corpse prefab is saved inactive)
        }
    }
    game_freeze(0.08f);   // FreezeMoment(1)
    if (!reset) uo_destroy(o);
    else uo_send_event(o, "CENTIPEDE DEATH");
}

enum { CP_NOTSTARTED, CP_INAIR, CP_DEATHANIM, CP_COMPLETE, CP_PENDING };
typedef struct { int state; float landDelay; float jitter; float vx, vy; bool deathStun; int bounces, bounceFrame;
                 int lfx, lphase; float lt, jx, gsx, gsy; } CP;     // lfx: 1 CorpseFungusExplode, 2 CorpseGoopExplode, 3 CorpseSpineBurst
static void corpse_lfx_begin(UObj *o, UComp *c, CP *k);
extern const char *clip_table_pick(JVal *v, float *pitch);
static void corpse_table(UObj *o, UComp *c, const char *k) { float p = 1; const char *cl = clip_table_pick(jget(c->f, k), &p); if (cl) uo_play_at(o, cl, 1, p); }
static void corpse_event(UObj *o, UComp *c, const char *k) {      // an AudioEvent field
    JVal *v = jget(c->f, k), *cl = v ? jget(v, "Clip") : NULL, *a = cl ? jget(cl, "audio") : NULL;
    if (!a || a->t != J_STR) return;
    float lo = (float)jnum(jget(v, "PitchMin"), 1), hi = (float)jnum(jget(v, "PitchMax"), 1);
    uo_play_at(o, a->s, (float)jnum(jget(v, "Volume"), 1), lo > 0 && hi > 0 ? frand(lo, hi) : 1);
}
static bool corpse_chunker(UComp *c) { return !strcmp(c->cls, "CorpseChunker") || !strcmp(c->cls, "CorpseZomHive") || !strcmp(c->cls, "CorpseDeathStunChunker"); }
// the corpses that go off after landing (their LandEffects coroutines): Volatile Mosskin and Fungified Husks steam,
// shake and burst into a cloud of gas that hurts (CorpseFungusExplode); Volatile Gruzzers and Furious Vengeflies shake
// and leave a gas explosion (CorpseGoopExplode); Spiny Husks shoot out their spines (CorpseSpineBurst)
static void corpse_lfx_begin(UObj *o, UComp *c, CP *k) {
    (void)c;
    o->rb.kin = true; o->rb.vx = o->rb.vy = 0;
    k->lphase = 1; k->lt = 0;
    if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s landed: %s\n", uo_frame, o->name, c->cls);
}
static void corpse_ps(UObj *o, UComp *c, const char *k, bool on) { UObj *p = fobj(o, c, k); if (p) particles_obj_play(p, on); }
static void corpse_lfx(UObj *o, UComp *c, CP *k, float dt) {
    k->lt += dt;
    if (k->lphase == 1) {                       // WaitForSeconds(1)
        if (k->lt < 1) return;
        k->lt = 0; k->lphase = 2;
        if (k->lfx == 3) { uo_anim_play(o, "Burst Antic", false); corpse_event(o, c, "shakerExplode"); return; }
        corpse_ps(o, c, "corpseFlame", false);
        corpse_ps(o, c, k->lfx == 1 ? "anticSteam" : "corpseSteam", true);
        o->rb.vx = o->rb.vy = 0;
        corpse_event(o, c, "gushSound");
        k->jx = o->lx;
        return;
    }
    if (k->lphase == 2) {
        if (k->lfx == 3) {                      // Spiny Husk: 0.9 s of "Burst Antic", then the spines
            if (k->lt < 0.9f) return;
            uo_anim_play(o, "Burst", false);
            corpse_event(o, c, "zombiePrep"); corpse_event(o, c, "zombieShoot");
            UComp *nb = uo_comp(o, C_NONBOUNCER); if (nb) nb->en = false;
            UObj *sh = fobj(o, c, "spineHit"), *ln = fobj(o, c, "lines");
            if (sh) uo_set_active(sh, true);
            if (ln) uo_set_active(ln, true);
            float dx = uo_hero_x() - uo_wx(o), dy = uo_hero_y() - uo_wy(o);
            if (dx * dx + dy * dy <= 44 * 44) game_shake(0.25f);
            if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s bursts its spines\n", uo_frame, o->name);
            k->lphase = 0; return;
        }
        float jt = k->lfx == 1 ? 0.9f : 0.7f;   // Jitter: the sprite shakes from side to side
        if (k->lt < jt) { o->lx = k->jx + frand(-0.1f, 0.1f); return; }
        o->lx = k->jx;
        game_shake(0.25f);                      // EnemyKillShake
        if (k->lfx == 1) {
            corpse_event(o, c, "explodeSound");
            corpse_ps(o, c, "anticSteam", false);
            corpse_ps(o, c, "gasAttack", true);
            UObj *gh = fobj(o, c, "gasHitBox");
            if (gh) { uo_set_active(gh, true); k->gsx = gh->sx; k->gsy = gh->sy; gh->sx = gh->sy = 0.2f; }
            else o->mr = false;
            k->lphase = 3; k->lt = 0;
        } else {
            corpse_ps(o, c, "corpseSteam", false);
            int t = ftmpl(c, "gasExplosion");
            if (t >= 0) { UObj *e = uo_spawn(t, uo_wx(o), uo_wy(o), 0); if (e && fb(c, "gasExplosionRecyclable", false)) uo_set_active(e, true); }
            o->mr = false;
            k->lphase = 0;
        }
#ifdef __HOST__
        if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s explodes\n", uo_frame, o->name);
        if (getenv("HK_CORPSESHOTS")) { extern void host_request_shots(int, int); host_request_shots(4, 12); }
#endif
        return;
    }
    UObj *gh = fobj(o, c, "gasHitBox");
    if (k->lphase == 3) {                       // the gas cloud grows over 0.4 s (iTween ScaleTo, easeOutCirc)
        float t = fminf(k->lt / 0.4f, 1), e = sqrtf(1 - (t - 1) * (t - 1));
        if (gh) { gh->sx = 0.2f + (k->gsx - 0.2f) * e; gh->sy = 0.2f + (k->gsy - 0.2f) * e; }
#ifdef __HOST__
        if (gh && getenv("HK_GASTRACE") && gh->ncol) { float a0, b0, a1, b1, h0, i0, h1, i1; uo_col_aabb(gh, &gh->col[0], &a0, &b0, &a1, &b1); uo_hero_hurtbox(&h0, &i0, &h1, &i1);
            fprintf(stderr, "[gas %d] box %.2f,%.2f..%.2f,%.2f en %d act %d hero %.2f,%.2f..%.2f,%.2f dhk %d\n", uo_frame, a0, b0, a1, b1, gh->col[0].en, uo_active(gh), h0, i0, h1, i1, gh->dhk); }
#endif
        if (t >= 1) { o->mr = false; k->lphase = 4; k->lt = 0; }
    } else if (k->lphase == 4 && k->lt >= 0.4f) {
        if (gh) uo_set_active(gh, false);
        k->lphase = 0;
    }
}
static void corpse_smash(UObj *o, UComp *c, CP *k) {      // Corpse.Smash: a flyer bursts when it hits the ground
    if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s smashes\n", uo_frame, o->name);
    if (!strcmp(c->cls, "CorpseHatcher")) {   // Aspid Mother: two of her hatchlings (held under the "Extra Tag" object) come out
        UObj *et = uo_find_tag("Extra Tag");
        for (int i = 0; et && i < 2; i++) {
            int n = 0; for (UObj *q = et->child; q; q = q->next) if (!q->destroyed) n++;
            if (!n) break;
            int pick = rand() % n; UObj *ch = NULL;
            for (UObj *q = et->child; q; q = q->next) if (!q->destroyed && pick-- == 0) { ch = q; break; }
            if (!ch) break;
            uo_set_parent(ch, NULL, false);
            uo_set_world_pos(ch, uo_wx(o), uo_wy(o));
            uo_send_event(ch, "SPAWN");
            if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s releases %s\n", uo_frame, o->name, ch->name);
        }
    }
    corpse_table(o, c, "splatAudioClipTable");
    uo_anim_play(o, "Death Land", false);
    if (o->rb.has) o->rb.vx = o->rb.vy = 0;
    k->state = CP_DEATHANIM;
    if (fb(c, "bigBreaker", false)) game_cam_shake(1);
}
static void corpse_land(UObj *o, UComp *c, CP *k) {
    if (fb(c, "breaker", false)) {          // bounces smashBounces times, then smashes
        if (k->bounceFrame != uo_frame) { k->bounceFrame = uo_frame; if (++k->bounces >= jint(jget(c->f, "smashBounces"), 0)) corpse_smash(o, c, k); }
        return;
    }
    uo_anim_play(o, "Death Land", false);
    k->landDelay = 1; k->state = CP_PENDING;
    { UObj *le = fobj(o, c, "landEffects"); if (le) uo_set_active(le, true); }
    if (k->lfx) corpse_lfx_begin(o, c, k);
    if (corpse_chunker(c)) {                 // CorpseChunker.LandEffects: bursts into chunks
        if (getenv("HK_FSMTRACE")) fprintf(stderr, "[corpse %d] %s bursts into chunks\n", uo_frame, o->name);
        if (o->rb.has) o->rb.vx = o->rb.vy = 0;
        corpse_table(o, c, "splatAudioClipTable");
        game_cam_shake(1);
        UObj *fx = fobj(o, c, "effects"); if (fx) uo_set_active(fx, true);
        UObj *ch = fobj(o, c, "chunks");
        if (ch) {
            uo_set_active(ch, true); uo_set_parent(ch, NULL, false);
            for (UObj *q = ch->child; q; q = q->next) {
                float a = frand(60, 120) * (float)M_PI / 180, sp = frand(15, 20);
                UObj *b = q; if (!b->rb.has) for (UObj *z = q->child; z; z = z->next) if (z->rb.has) { b = z; break; }
                if (b->rb.has) { b->rb.vx = cosf(a) * sp; b->rb.vy = sinf(a) * sp; }
            }
        }
        o->mr = false;
    }
}

/* ================= Crawler / Climber ================= */
typedef struct { float vx, vy; int type; int phase; } CW;    // vx,vy first (recoil reads them)
static void cw_dir(UObj *o, float lx, float ly, float *dx, float *dy) {   // TransformDirection (rotation only)
    float a = uo_wrz(o) * (float)M_PI / 180, ca = cosf(a), sa = sinf(a);
    *dx = ca * lx - sa * ly; *dy = sa * lx + ca * ly;
}
static bool cw_ray(UObj *o, UObj *from, float lx, float ly, float len) {
    float ox = from ? uo_wx(from) : uo_wx(o), oy = from ? uo_wy(from) : uo_wy(o);
    (void)lx; (void)ly;
    float dx, dy; cw_dir(o, lx, ly, &dx, &dy);
    float h; return w_raycast(ox, oy, dx, dy, len, &h);
}

typedef struct { int dir; bool clockwise, b_cw; float px, py, tpx, tpy; int phase; float t, r0, r1, sx0, sy0, sx1, sy1; bool tween; int stun; } CL;
static void climber_stun(UObj *o) {
    UComp *c = uo_comp(o, C_CLIMBER); if (!c || !c->st) return;
    CL *l = c->st;
    if (l->phase == 1) return;   // turning
    l->stun = 1; if (o->rb.has) o->rb.vx = o->rb.vy = 0;
    uo_anim_play(o, "Stun", true);
}
static void cl_vel(UObj *o, UComp *c, int d) {
    float s = ff(c, "speed", 2);
    float vx = d == 0 ? s : d == 2 ? -s : 0, vy = d == 3 ? s : d == 1 ? -s : 0;
    if (o->rb.has) { o->rb.vx = vx; o->rb.vy = vy; }
}
static bool cl_ray(UObj *o, UComp *c, float lx, float ly, float len) {
    UCol *col = NULL; for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { col = &o->col[i]; break; }
    float ox = uo_wx(o), oy = uo_wy(o);
    if (col) { float a = uo_wrz(o) * (float)M_PI / 180; float px = col->ox * uo_wsx(o), py = col->oy * uo_wsy(o); ox += cosf(a) * px - sinf(a) * py; oy += sinf(a) * px + cosf(a) * py; }
    float dx, dy; cw_dir(o, lx, ly, &dx, &dy);
    float h; (void)c; return w_raycast(ox, oy, dx, dy, len, &h);
}

/* ================= EnemyBullet ================= */
typedef struct { bool active, colOff; float scale, t; } EB;
static void eb_collide(UObj *o, UComp *c, EB *b, float nx, float ny, bool rot) {
    b->active = false;
    o->sx = b->scale; o->sy = b->scale;
    o->rb.kin = true; o->rb.vx = o->rb.vy = 0;
    uo_anim_play(o, "Impact", false);
    if (!rot || (ny >= 0.75f && fabsf(nx) < 0.5f)) o->rz = 0;
    else if (ny <= 0.75f && fabsf(nx) < 0.5f) o->rz = 180;
    else if (nx >= 0.75f && fabsf(ny) < 0.5f) o->rz = 270;
    else if (nx <= 0.75f && fabsf(ny) < 0.5f) o->rz = 90;
    int ci = uo_anim_clip(o, "Impact");
    const Anim *a = (ci >= 0 && uo_spriteset) ? &uo_spriteset->anims[ci] : NULL;
    b->t = a && a->fps > 0 ? (a->count - 1) / a->fps : 0.1f;
    b->colOff = true;
    (void)c;
}

/* ================= lifecycle ================= */
void comp_awake(UObj *o, UComp *c) {
    if (c->awoken) return;
    c->awoken = true;      // (an object's Awake can enable another object before that one's own Awake has run)
    if (c->type == C_PS) return;
    if (c->type >= C_BREAKABLE) { comp2_awake(o, c); return; }
    switch (c->type) {
        case C_HEALTH: { HM *h = calloc(1, sizeof(HM)); c->st = h;
            h->hp = fi(c, "hp", 1); h->invincible = fb(c, "invincible", false); h->invDir = fi(c, "invincibleFromDirection", 0);
            h->special = fb(c, "hasSpecialDeath", false); h->deathReset = fb(c, "deathReset", false); h->damageOverride = fb(c, "damageOverride", false);
            h->preventInvEffect = fb(c, "preventInvincibleEffect", false); h->enemyType = fi(c, "enemyType", 0);
            h->geoS = fi(c, "smallGeoDrops", 0); h->geoM = fi(c, "mediumGeoDrops", 0); h->geoL = fi(c, "largeGeoDrops", 0);
            JVal *eo = jget(c->f, "effectOrigin"); h->ex = (float)jnum(jget(eo, "x"), 0); h->ey = (float)jnum(jget(eo, "y"), 0);
            { JVal *bs = jget(c->f, "battleScene"), *bi = bs && bs->t == J_OBJ ? jget(bs, "o") : NULL; if (bi) h->battleScene = comps_tree_obj(o, jint(bi, -1)); }
            h->evasion = -1; break; }
        case C_RECOIL: { RC *r = calloc(1, sizeof(RC)); c->st = r;
            r->freezeInPlace = fb(c, "freezeInPlace", false); r->preventRecoilUp = fb(c, "preventRecoilUp", false);
            r->speedBase = ff(c, "recoilSpeedBase", 15); r->duration = ff(c, "recoilDuration", 0.5f);
            r->stopVxUp = fb(c, "stopVelocityXWhenRecoilingUp", true); break; }
        case C_WALKER: { WK *w = calloc(1, sizeof(WK)); c->st = w; break; }
        case C_ALERTRANGE: c->st = calloc(1, sizeof(AR)); break;
        case C_LOS: c->st = calloc(1, sizeof(LOS)); break;
        case C_DAMAGEHERO: { int *d = calloc(2, sizeof(int)); d[0] = fi(c, "damageDealt", 1); d[1] = d[0]; c->st = d; break; }
        case C_CORPSE: { CP *k = calloc(1, sizeof(CP)); c->st = k; k->deathStun = !strcmp(c->cls, "CorpseDeathStun") || !strcmp(c->cls, "CorpseDeathStunChunker"); o->rb.fric = 0.4f;
            k->lfx = !strcmp(c->cls, "CorpseFungusExplode") ? 1 : !strcmp(c->cls, "CorpseGoopExplode") ? 2 : !strcmp(c->cls, "CorpseSpineBurst") ? 3 : 0;
            break; }
        case C_CRAWLER: c->st = calloc(1, sizeof(CW)); break;
        case C_CLIMBER: c->st = calloc(1, sizeof(CL)); break;
        case C_FADEGROUP: { FG *g = calloc(1, sizeof(FG)); c->st = g; g->alpha = ff(c, "downAlpha", 0); break; }
        default: break;
    }
}

void comp_start(UObj *o, UComp *c) {
    if (!c->awoken) comp_awake(o, c);
    if (c->type == C_PS) return;
    if (c->type >= C_BREAKABLE || c->type == C_PBI) { comp2_start(o, c); if (c->type != C_PBI) return; }
    switch (c->type) {
        case C_PBI: {     // HealthManager.Start: an enemy recorded dead stays gone
            HM *h = hm_of(o); char k[96]; pbi_key(o, c, k, sizeof k);
            if (h && pd_bool(k)) { h->isDead = true; uo_set_active(o, false); }
            break; }
        case C_WALKER: { WK *w = c->st;
            if (w->facing == 0) w->facing = (o->sx * ff(c, "rightScale", 1) >= 0) ? 1 : -1;
            if (w->state == WK_NOTREADY) { w->turnCool = -1e-6f; w->state = WK_WAITING; w->camOk = w->heroXOk = false; wk_update(o, c, w, 0); }
            break; }
        case C_CORPSE: { CP *k = c->st;
            if (k->deathStun) { k->jitter = 0.75f; k->vx = o->rb.vx; k->vy = o->rb.vy; o->rb.kin = true; o->rb.vx = o->rb.vy = 0; o->flash = 0.75f; }
            else {
                corpse_event(o, c, "startAudio");
                k->state = fb(c, "massless", false) ? CP_DEATHANIM : CP_INAIR; if (k->state == CP_INAIR) uo_anim_play(o, "Death Air", false);
                if (fb(c, "instantChunker", false) && !fb(c, "breaker", false)) corpse_land(o, c, k);
            }
            break; }
        case C_CRAWLER: { CW *w = c->st;
            float z = fmodf(uo_wrz(o), 360); if (z < 0) z += 360;
            float sp = ff(c, "speed", 2), sx = o->sx, sy = o->sy;
            if (z >= 45 && z <= 135) { w->type = 2; w->vx = 0; w->vy = (sx > 0 ? -1 : 1) * sp; }
            else if (z >= 135 && z <= 225) { w->type = sy > 0 ? 1 : 0; w->vx = (sx > 0 ? 1 : -1) * sp; w->vy = 0; }
            else if (z >= 225 && z <= 315) { w->type = 2; w->vx = 0; w->vy = (sx > 0 ? 1 : -1) * sp; }
            else { w->type = !(sy > 0) ? 1 : 0; w->vx = (sx > 0 ? -1 : 1) * sp; w->vy = 0; }
            // vectors are world-space in Unity; rotate local velocity is already world for floor/roof; walls use y
            recoil_set_speed(o, 0);
            if (w->type == 0) { o->rb.g = 1; recoil_set_freeze(o, false); } else { o->rb.g = 0; recoil_set_freeze(o, true); }
            w->phase = 0; uo_anim_play(o, "Walk", false); if (o->rb.has) { o->rb.vx = w->vx; o->rb.vy = w->vy; }
            break; }
        case C_CLIMBER: { CL *l = c->st;
            // StickToGround
            UCol *col = NULL; for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { col = &o->col[i]; break; }
            { float dx, dy; cw_dir(o, 0, -1, &dx, &dy); float ox = uo_wx(o), oy = uo_wy(o), h;
              if (col) { float a = uo_wrz(o) * (float)M_PI / 180; float px = col->ox * uo_wsx(o), py = col->oy * uo_wsy(o); ox += cosf(a) * px - sinf(a) * py; oy += sinf(a) * px + cosf(a) * py; }
              if (w_raycast(ox, oy, dx, dy, 2, &h)) uo_set_world_pos(o, ox + dx * h, oy + dy * h); }
            float num = o->sx >= 0 ? 1 : -1; if (!fb(c, "startRight", true)) num = -num;
            l->clockwise = num > 0;
            float z = fmodf(uo_wrz(o), 360); if (z < 0) z += 360;
            if (z >= 45 && z <= 135) l->dir = l->clockwise ? 3 : 1;
            else if (z >= 135 && z <= 225) l->dir = l->clockwise ? 2 : 0;
            else if (z >= 225 && z <= 315) l->dir = l->clockwise ? 1 : 3;
            else l->dir = l->clockwise ? 0 : 2;
            UComp *rc = uo_comp(o, C_RECOIL); if (rc && rc->st) ((RC *)rc->st)->skipFreeze = true;
            l->px = uo_wx(o); l->py = uo_wy(o); l->tpx = l->px - 10; l->tpy = l->py;
            uo_anim_play(o, "Walk", false); cl_vel(o, c, l->dir);
            break; }
        default: break;
    }
}

void comp_enable(UObj *o, UComp *c) {
    if (!c->awoken) comp_awake(o, c);
    if (c->type == C_PS) { particles_comp_enable(o, c); return; }
    if (c->type >= C_SPRANIM) { comp3_enable(o, c); return; }
    switch (c->type) {
        case C_RECYCLE_ANIM: case C_AUTORECYCLE: case C_PLAYAUDIO: { float *t = c->st; if (!t) { t = calloc(1, sizeof(float)); c->st = t; }
            *t = c->type == C_AUTORECYCLE ? (fi(c, "afterEvent", 0) == 0 && ff(c, "timeToWait", 0) > 0 ? ff(c, "timeToWait", 0) : fi(c, "afterEvent", 0) == 3 ? 1.0f : -1) : 0;
            if (c->type == C_RECYCLE_ANIM && o->an.has) { if (fb(c, "randomiseRotation", false)) o->rz = (float)(rand() % 360); uo_anim_play_frame(o, 0); }
            break; }
        case C_DEACT_ANIM: { float *t = c->st; if (!t) { t = calloc(1, sizeof(float)); c->st = t; } *t = 0;
            if (o->an.has && o->an.clip >= 0) { o->an.playing = true; o->an.clipTime = 0; o->an.frame = 0; o->an.prevFrame = -1; } break; }
        case C_DAMAGEHERO: if (fb(c, "resetOnEnable", false) && c->st) ((int *)c->st)[0] = ((int *)c->st)[1]; break;
        case C_BULLET: { EB *b = c->st; if (!b) { b = calloc(1, sizeof(EB)); c->st = b; }
            b->active = true; b->colOff = false; b->t = 0; b->scale = frand(ff(c, "scaleMin", 1.15f), ff(c, "scaleMax", 1.45f));
            for (int i = 0; i < o->ncol; i++) o->col[i].en = true;
            o->rb.kin = false; o->rb.vx = o->rb.vy = 0; uo_anim_play(o, "Idle", false);
            break; }
        case C_RECOIL: if (c->st) recoil_cancel(o, c->st); break;
        case C_DEACT_PD_TRUE: case C_DEACT_PD_FALSE: {    // OnEnable: hide the object depending on a PlayerData bool
            JVal *bn = jget(c->f, "boolName");
            bool v = pd_bool(bn && bn->t == J_STR ? bn->s : NULL);
            if (v == (c->type == C_DEACT_PD_TRUE)) uo_set_active(o, false);
            break; }
        default: break;
    }
}
void comp_disable(UObj *o, UComp *c) { (void)o; if (c->type == C_ALERTRANGE && c->st) ((AR *)c->st)->inRange = false; if (c->type == C_PS) particles_comp_disable(o, c); }

// GeoControl: flung geo bounces with its own rigidbody; the Knight's HeroBox collects it (value by size), then it recycles
typedef struct { float t; bool landed; } GEOS;
static void geo_update(UObj *o, UComp *c, float dt) {
    GEOS *g = c->st; if (!g) { g = calloc(1, sizeof *g); c->st = g; }
    g->t += dt;
    int type = fi(c, "type", 0); JVal *sizes = jget(c->f, "sizes"), *sz = sizes ? jat(sizes, type) : NULL;
    if (!g->landed && o->rb.has && (o->rb.contacts & 1)) {
        g->landed = true;
        if (sz && o->an.has) uo_anim_play(o, jstr(jget(sz, "idleAnim"), "Small Idle"), true);
    }
    if (g->t < 0.25f) return;      // a moment before it can be picked up (it starts inside the enemy)
    float x0, y0, x1, y1, hx0, hy0, hx1, hy1; uo_hero_box(&hx0, &hy0, &hx1, &hy1);
    if (!uo_body_aabb(o, &x0, &y0, &x1, &y1)) { x0 = uo_wx(o) - 0.2f; x1 = uo_wx(o) + 0.2f; y0 = uo_wy(o) - 0.2f; y1 = uo_wy(o) + 0.2f; }
    const float m = 0.2f;
    if (x0 - m < hx1 && x1 + m > hx0 && y0 - m < hy1 && y1 + m > hy0) {
        game_add_geo(sz ? jint(jget(sz, "value"), 1) : 1);
        JVal *ps = jget(c->f, "pickupSounds"); int n = ps ? ps->n : 0;
        const char *snd = "geo_small_collect_1";
        if (n > 0) { JVal *e = jat(ps, rand() % n); const char *a = e ? jstr(jget(e, "audio"), NULL) : NULL; if (a) snd = a; }
        sfx_play_pitch(snd, 0.7f, 0.95f + (rand() % 10) / 100.0f);
        g->t = 0; g->landed = false;
        if (o->rb.has) { o->rb.vx = o->rb.vy = 0; }
        uo_set_active(o, false);     // back to the pool
    }
}

void comp_update(UObj *o, UComp *c, float dt) {
    if (c->type >= C_BREAKABLE) { comp2_update(o, c, dt); return; }
    switch (c->type) {
        case C_GEO: geo_update(o, c, dt); break;
        case C_FADEGROUP: fg_update(o, c, dt); break;
        case C_HEALTH: ((HM *)c->st)->evasion -= dt; break;
        case C_WALKER: wk_update(o, c, c->st, dt); break;
        case C_LOS: los_update(o, c); break;
        case C_DEACT_ANIM: { float *t = c->st; if (t && *t > 0.1f) *t -= dt; else if (!o->an.playing) uo_set_active(o, false); break; }
        case C_RECYCLE_ANIM: { float *t = c->st; if (t && *t > 0.1f) *t -= dt; else if (!o->an.playing) uo_destroy(o); break; }
        case C_AUTORECYCLE: { float *t = c->st; if (t && *t > 0) { *t -= dt; if (*t <= 0) uo_destroy(o); } break; }
        case C_PLAYAUDIO: uo_destroy(o); break;
        case C_BULLET: { EB *b = c->st; if (!b) break;
            if (b->active) {
                float sp = sqrtf(o->rb.vx * o->rb.vx + o->rb.vy * o->rb.vy), k = ff(c, "stretchFactor", 1.2f);
                o->rz = atan2f(o->rb.vy, o->rb.vx) * 180 / (float)M_PI;
                float sy = 1 - sp * k * 0.01f, sx = 1 + sp * k * 0.01f;
                if (sx < ff(c, "stretchMinX", 0.75f)) sx = ff(c, "stretchMinX", 0.75f);
                if (sy > ff(c, "stretchMaxY", 1.75f)) sy = ff(c, "stretchMaxY", 1.75f);
                o->sx = sx * b->scale; o->sy = sy * b->scale;
                float hx0, hy0, hx1, hy1; uo_hero_hurtbox(&hx0, &hy0, &hx1, &hy1);
                for (int i = 0; i < o->ncol; i++) if (uo_col_overlap_box(o, &o->col[i], hx0, hy0, hx1, hy1)) { eb_collide(o, c, b, 0, 0, false); break; }
            } else {
                if (b->colOff) { b->colOff = false; for (int i = 0; i < o->ncol; i++) o->col[i].en = false; }
                b->t -= dt; if (b->t <= 0) uo_destroy(o);
            }
            break; }
        case C_FSMACTIVATOR:     // the camera's ActiveRegion trigger switches the enemy's FSMs on when it comes near
            if (!c->st) {
                float dx = uo_wx(o) - uo_hero_x(), dy = uo_wy(o) - uo_hero_y();
                if (fabsf(dx) < 20 && fabsf(dy) < 13) {
                    c->st = calloc(1, 1);
                    for (int k = 0; k < o->nfsm; k++) if (!o->fsm[k]->enabled) { o->fsm[k]->enabled = true; fsm_start(o->fsm[k]); }
                    if (o->an.has && !o->an.playing && o->an.clip >= 0) { o->an.playing = true; }
                }
            }
            break;
        case C_CORPSE: { CP *k = c->st;
            if (k->lphase) corpse_lfx(o, c, k, dt);
            if (k->deathStun && k->jitter > 0) {
                k->jitter -= dt;
                if (k->jitter <= 0) { o->rb.kin = false; o->rb.vx = k->vx; o->rb.vy = k->vy; k->deathStun = false; k->state = CP_INAIR; uo_anim_play(o, "Death Air", false); corpse_event(o, c, "startAudio"); }
                break;
            }
            if (k->state == CP_DEATHANIM) { if (!o->an.playing) { k->state = CP_COMPLETE; uo_destroy(o); } }
            else if (k->state == CP_INAIR) { if (uo_wy(o) < -10) { k->state = CP_COMPLETE; uo_destroy(o); } }
            else if (k->state == CP_PENDING) { k->landDelay -= dt; if (k->landDelay <= 0) k->state = CP_COMPLETE; }
            break; }
        case C_CRAWLER: { CW *w = c->st;
            if (w->phase == 0) {    // Walk loop
                UObj *wc = fobj(o, c, "wallCheck"), *gc = fobj(o, c, "groundCheck");
                bool wall = cw_ray(o, wc, o->sx > 0 ? -1.0f : 1.0f, 0, 1);
                bool ground = cw_ray(o, gc, 0, o->sy > 0 ? -1.0f : 1.0f, 1);
                UComp *rc = uo_comp(o, C_RECOIL);
                bool recoiling = rc && rc->st && ((RC *)rc->st)->state != 0;
                if (!recoiling && (wall || !ground)) { w->phase = 1; if (o->rb.has) o->rb.vx = o->rb.vy = 0; uo_anim_play(o, "Turn", true); }
            } else if (w->phase == 1) {
                if (!o->an.playing) { o->sx = -o->sx; w->vx = -w->vx; w->vy = -w->vy; w->phase = 2; }
            } else { w->phase = 0; uo_anim_play(o, "Walk", false); if (o->rb.has) { o->rb.vx = w->vx; o->rb.vy = w->vy; } }
            break; }
        case C_CLIMBER: { CL *l = c->st;
            if (l->stun) { if (!o->an.playing) { l->stun = 0; uo_anim_play(o, "Walk", false); cl_vel(o, c, l->dir); } break; }
            if (l->phase == 1) {   // turning
                l->t += dt; float st = ff(c, "spinTime", 0.25f), t = l->t / st;
                if (t >= 1) {
                    o->rz = l->r1; if (l->tween) uo_set_world_pos(o, l->sx1, l->sy1);
                    int n = l->dir + (l->b_cw ? 1 : -1); if (n < 0) n = 3; if (n > 3) n = 0; l->dir = n;
                    cl_vel(o, c, l->dir); l->px = uo_wx(o); l->py = uo_wy(o); l->tpx = l->px; l->tpy = l->py; l->phase = 0;
                } else { o->rz = l->r0 + (l->r1 - l->r0) * t; if (l->tween) uo_set_world_pos(o, l->sx0 + (l->sx1 - l->sx0) * t, l->sy0 + (l->sy1 - l->sy0) * t); }
                break;
            }
            float x = uo_wx(o), y = uo_wy(o); bool fl = false;
            JVal *cn = jget(c->f, "constrain"); float cx = (float)jnum(jget(cn, "x"), 0.1), cy = (float)jnum(jget(cn, "y"), 0.1);
            if (fabsf(x - l->px) > cx) { x = l->px; fl = true; }
            if (fabsf(y - l->py) > cy) { y = l->py; fl = true; }
            if (fl) uo_set_world_pos(o, x, y); else { l->px = x; l->py = y; }
            float dd = sqrtf((l->tpx - x) * (l->tpx - x) + (l->tpy - y) * (l->tpy - y));
            if (dd >= ff(c, "minTurnDistance", 0.25f)) {
                UCol *col = NULL; for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { col = &o->col[i]; break; }
                float cw = col ? col->w : 1, chh = col ? col->h : 1, pad = ff(c, "wallRayPadding", 0.1f);
                bool ground = cl_ray(o, c, 0, -1, 1);
                bool wall = cl_ray(o, c, l->clockwise ? 1.0f : -1.0f, 0, cw / 2 + pad);
                if (!ground || wall) {
                    bool tcw = !ground ? l->clockwise : !l->clockwise;
                    l->b_cw = tcw; l->tween = ground && wall;
                    if (o->rb.has) o->rb.vx = o->rb.vy = 0;
                    l->r0 = o->rz; l->r1 = o->rz + (tcw ? -90 : 90);
                    l->sx0 = x; l->sy0 = y;
                    float tx = 0, ty = 0; bool cwz = l->clockwise;
                    switch (l->dir) {
                        case 0: tx = cw / 2; ty = cwz ? chh / 2 : -chh / 2; tx += pad; break;
                        case 3: tx = cwz ? -cw / 2 : cw / 2; ty = chh / 2 + pad; break;
                        case 1: tx = cwz ? cw / 2 : -cw / 2; ty = -chh / 2 - pad; break;
                        case 2: tx = -cw / 2 - pad; ty = cwz ? -chh / 2 : chh / 2; break;
                    }
                    l->sx1 = x + tx; l->sy1 = y + ty;
                    l->t = 0; l->phase = 1;
                }
            }
            break; }
        default: break;
    }
}

void comp_fixed(UObj *o, UComp *c, float dt) {
    if (c->type >= C_BREAKABLE) { comp2_fixed(o, c, dt); return; }
    if (c->type == C_RECOIL && c->st) recoil_fixed(o, c->st, dt);
}

void comp_collision(UObj *o, UComp *c, bool enter) {
    if (c->type >= C_BREAKABLE) { comp2_collision(o, c, enter); return; }
    if (c->type == C_BULLET && enter && c->st && ((EB *)c->st)->active) {
        int ct = o->rb.contacts & ~o->rb.prevContacts; float nx = 0, ny = 0;
        if (ct & 1) ny = 1; else if (ct & 2) ny = -1; else if (ct & 4) nx = 1; else if (ct & 8) nx = -1;
        eb_collide(o, c, c->st, nx, ny, true);
        return;
    }
    if (c->type == C_BOUNCE && enter && o->rb.has && !o->rb.kin) {   // ObjectBounce.OnCollisionEnter2D
        float sp = sqrtf(o->rb.pvx * o->rb.pvx + o->rb.pvy * o->rb.pvy);
        if (sp > ff(c, "speedThreshold", 1)) {
            float vx = o->rb.pvx / sp, vy = o->rb.pvy / sp, nx = 0, ny = 0;
            int ct = o->rb.contacts & ~o->rb.prevContacts;
            if (ct & 1) ny = 1; else if (ct & 2) ny = -1; else if (ct & 4) nx = 1; else if (ct & 8) nx = -1;
            float d = vx * nx + vy * ny; vx -= 2 * d * nx; vy -= 2 * d * ny;
            float k = sp * ff(c, "bounceFactor", 0) * (0.8f + 0.4f * (rand() / (float)RAND_MAX));
            o->rb.vx = vx * k; o->rb.vy = vy * k;
        }
    }
    if (c->type == C_CORPSE) {
        CP *k = c->st;
        if (k->state == CP_INAIR && sweep_hits(o, 3, 0.08f, 0, 0)) corpse_land(o, c, k);
    }
}

void comp_free(UComp *c) { free(c->st); c->st = NULL; }

/* ================= world-level checks each physics step ================= */
void comps_fixed_world(float dt) {
    (void)dt;
    float hx0, hy0, hx1, hy1, bx0, by0, bx1, by1;
    uo_hero_hurtbox(&hx0, &hy0, &hx1, &hy1);
    uo_hero_box(&bx0, &by0, &bx1, &by1);
    int n = uo_count();
    for (int i = 0; i < n; i++) {
        UObj *o = uo_obj(i);
        if (!o || o->destroyed || !uo_active(o)) continue;
        // AlertRange: trigger vs the Knight (layer 9)
        UComp *ar = uo_comp(o, C_ALERTRANGE);
        if (ar && ar->st) {
            bool in = false;
            for (int k = 0; k < o->ncol; k++) if (uo_col_overlap_box(o, &o->col[k], bx0, by0, bx1, by1)) in = true;
            ((AR *)ar->st)->inRange = in;
        }
        // DamageHero vs HeroBox
        UComp *dh = uo_comp(o, C_DAMAGEHERO);
        if (dh && dh->en && dh->st && ((int *)dh->st)[0] > 0 && !P.dead) {
            for (int k = 0; k < o->ncol; k++) {
                if (uo_col_overlap_box(o, &o->col[k], hx0, hy0, hx1, hy1)) {
                    int ht = fi(dh, "hazardType", 1);
#ifdef __HOST__
                    if (getenv("HK_DMGTRACE") && !P.c.invulnerable) fprintf(stderr, "[dmg %d] %s (%s) hits the Knight\n", uo_frame, o->name, o->parent ? o->parent->name : "-");
#endif
                    hero_take_damage(uo_wx(o), ((int *)dh->st)[0], ht);
                    break;
                }
            }
        }
        // a damages_hero FSM instead (HeroBox.CheckForDamage looks for it first): the Shade's shadow balls, Crystal
        // Peak's stompers, the infected gas explosions, Volatile Mosskin and Fungified Husk gas, Grimm's pillars...
        if (!o->dhk) { o->dhk = 1; for (int k = 0; k < o->nfsm; k++) if (!strcmp(o->fsm[k]->name, "damages_hero")) { o->dhk = 2; o->dhf = o->fsm[k]; } }
        if (o->dhk == 2 && !dh && !P.dead) {
            FVar *dv = fsm_var(o->dhf, "damageDealt"), *hv = fsm_var(o->dhf, "hazardType");
            if (dv && dv->i > 0)
                for (int k = 0; k < o->ncol; k++) {
                    if (!uo_col_overlap_box(o, &o->col[k], hx0, hy0, hx1, hy1)) continue;
#ifdef __HOST__
                    if (getenv("HK_DMGTRACE") && !P.c.invulnerable) fprintf(stderr, "[dmg %d] %s (%s) hits the Knight (damages_hero)\n", uo_frame, o->name, o->parent ? o->parent->name : "-");
#endif
                    hero_take_damage(uo_wx(o), dv->i, hv && hv->i > 0 ? hv->i : 1);
                    break;
                }
        }
    }
}

// the Knight's nail: called by the game while the slash collider is active. Returns hit flags:
// 1 = touched an enemy (layer 11) -> NailSlash recoil, 2 = it was a NonBouncer (no pogo), 4 = took damage
void comps_hit_taker(UObj *t, int damage, float dir, int atk, float mag, UObj *src) {
    UObj *q = t;
    for (int up = 0; q && up < 3; up++, q = q->parent) {
        if (uo_comp(q, C_HEALTH)) { if (!hm_is_dead(q)) hm_hit(q, damage, dir, (AttackType)atk, mag); return; }
        if (comp2_hit(q, dir, atk, src)) return;
    }
}
int uo_nail_hits(float x0, float y0, float x1, float y1, int swing, float angle, int damage) {
    int res = 0, n = uo_count();
    for (int i = 0; i < n; i++) {
        UObj *o = uo_obj(i);
        if (!o || o->destroyed || !uo_active(o)) continue;
        bool hm = uo_comp(o, C_HEALTH) != NULL, br = comp2_is_responder(o);
        {   // NailSlash on a bounce shroom (Fungal Wastes): a down-slash gives the big ShroomBounce, other slashes recoil long.
            // Down: enemies, interactive objects, hero-attack layer; others: enemies and interactive objects
            UComp *bsc = uo_comp(o, C_BOUNCESHROOM);
            if (bsc && bsc->en) {
                UComp *nb = uo_comp(o, C_NONBOUNCER); bool nbOn = nb && nb->en && fb(nb, "active", true);
                bool ok = angle == 270 ? (o->layer == 11 || o->layer == 19 || o->layer == 17) && !nbOn
                                       : (o->layer == 11 && !nbOn) || o->layer == 19;
                if (ok) for (int k = 0; k < o->ncol; k++) if (uo_col_overlap_box(o, &o->col[k], x0, y0, x1, y1)) { res |= 8; comp2_shroom_nail(o, angle == 270); break; }
            }
        }
        if (!hm && !br && o->layer != 11 && !(o->nfsm && (o->layer == 0 || o->layer == 19 || o->layer == 12))) {
            // other layers: only objects whose scripts listen for TAKE DAMAGE (breakable walls are on the terrain
            // layer: without this the walls hiding secret areas - and the grubs in them - couldn't be broken).
            // Layer 22 is the Knight's own slash effects.
            if (!o->nfsm || o->layer == 22) continue;
            if (!o->tdk) { o->tdk = 1; for (int k = 0; k < o->nfsm; k++) if (fsm_listens(o->fsm[k], "TAKE DAMAGE")) o->tdk = 2; }
            if (o->tdk != 2) continue;
        }
        bool touch = false;
        for (int k = 0; k < o->ncol; k++) if (uo_col_overlap_box(o, &o->col[k], x0, y0, x1, y1)) { touch = true; break; }
#ifdef __HOST__
        if (getenv("HK_SLASHTRACE") && strstr(o->name, getenv("HK_SLASHTRACE"))) { float a = 0, b = 0, c2 = 0, d = 0; if (o->ncol) uo_col_aabb(o, &o->col[0], &a, &b, &c2, &d);
            fprintf(stderr, "[slash %d] box %.2f,%.2f..%.2f,%.2f vs %s aabb %.2f,%.2f..%.2f,%.2f touch %d\n", uo_frame, x0, y0, x1, y1, o->name, a, b, c2, d, touch); }
#endif
        if (!touch) continue;
        if (o->layer == 11 || hm) {
            res |= 1;
            UComp *nb = uo_comp(o, C_NONBOUNCER); if (nb && nb->en && fb(nb, "active", true)) res |= 2;
        }
        if (br && o->lastSwing != swing) { o->lastSwing = swing; comp2_hit(o, angle, ATK_NAIL, uo_slash_obj()); res |= 1; continue; }
        if (o->lastSwing != swing && (hm || o->nfsm)) {
            o->lastSwing = swing;
            uo_set_event_sender(uo_slash_obj());
            uo_send_event(o, "TAKE DAMAGE");          // the nail's damages_enemy FSM
            if (hm && !hm_is_dead(o) && hm_hit(o, damage, angle, ATK_NAIL, 1)) res |= 4;
            else if (hm) {      // the log says why a swing that touched an enemy did nothing (playtest: "my nail doesn't hurt them")
                static int told; static char lastRoom[32];
                if (strcmp(lastRoom, uo_room_name())) { snprintf(lastRoom, sizeof lastRoom, "%s", uo_room_name()); told = 0; }
                const char *why = hm_no_hit_reason(o, damage, angle);
                if (why && strcmp(why, "just hit (evasion)") && told < 12) { told++; extern void hk_log(const char *, ...);
                    hk_log("nail touched %s, no damage: %s (damage %d, layer %d)", o->name, why, damage, o->layer); }
            }
        }
    }
    return res;
}
