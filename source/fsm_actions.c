// PlayMaker actions used by Hollow Knight's enemy FSMs, ported from the decompiled action sources
// (HutongGames.PlayMaker.Actions in Assembly-CSharp). Each action keeps the original OnEnter / OnUpdate /
// OnFixedUpdate / OnLateUpdate / OnExit split and its Finish() points.
#include "particles.h"
#include "fsm.h"
#include "dialogue.h"
#include "input.h"
#include "lang.h"
#include "game.h"
#include "msgs.h"
#include "comps.h"
#include "comps2.h"
#include "world.h"
#include "hero.h"
#include "audio.h"
#include "pd.h"
#include "map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

void game_freeze(float t);
void game_shake(float t);
void uo_anim_set_callbacks(UObj *o, Fsm *f, const char *complete, const char *trigger, bool setComplete, bool setTrigger);

#define ACTS(X) \
    X(Wait) X(WaitRandom) X(NextFrameEvent) X(SendEventByName) X(SendEvent) X(SendRandomEvent) X(SendRandomEventV2) \
    X(BoolTest) X(BoolAllTrue) X(BoolNoneTrue) X(BoolFlip) X(BoolFlipEveryFrame) X(SetBoolValue) \
    X(FloatCompare) X(IntCompare) X(StringCompare) X(FloatSwitch) X(FloatInRange) X(FloatTestToBool) X(GameObjectIsNull) \
    X(SetFloatValue) X(SetFloatValueV2) X(SetIntValue) X(SetStringValue) X(SetGameObject) X(SetVector2XY) X(SetVector3XYZ) \
    X(GetVector3XYZ) X(FloatAdd) X(FloatAddV2) X(FloatSubtract) X(FloatMultiply) X(FloatMultiplyV2) X(FloatDivide) \
    X(FloatClamp) X(FloatOperator) X(IntAdd) X(IntOperator) X(RandomFloat) X(RandomInt) X(RandomBool) \
    X(DistanceBetweenPoints2D) X(GetAngleToTarget2D) X(GetDistance) X(CheckTargetDirection) \
    X(GetOwner) X(GetHero) X(FindChild) X(GetChild) X(FindGameObject) X(GetParent) X(GetChildCount) X(GetRandomChild) \
    X(SetParent) X(GetComponent) \
    X(GetPosition) X(SetPosition) X(GetScale) X(SetScale) X(GetRotation) X(SetRotation) X(SetRandomRotation) \
    X(FlipScale) X(Translate) X(ObjectJitter) X(FaceObject) X(FaceDirection) \
    X(SetVelocity2d) X(GetVelocity2d) X(SetVelocityAsAngle) X(AddForce2dV2) X(SetGravity2dScale) X(Decelerate) \
    X(DecelerateV2) X(IdleBuzz) X(DistanceFly) X(ChaseObject) X(FireAtTarget) X(SetIsFixedAngle2d) X(AddTorque2d) \
    X(RayCast2d) X(CheckCollisionSide) X(CheckCollisionSideEnter) X(Collision2dEvent) X(GetCollision2dInfo) \
    X(Trigger2dEvent) X(Trigger2dEventLayer) X(SendTrigger2DEventByName) \
    X(ActivateGameObject) X(ActivateAllChildren) X(CreateObject) X(SpawnObjectFromGlobalPool) \
    X(SpawnObjectFromGlobalPoolOverTimeV2) X(SpawnRandomObjects) X(FlingObjectsFromGlobalPool) X(FlingObjects) \
    X(FlingObject) X(DestroyObject) X(DestroySelf) X(RecycleSelf) \
    X(SetMeshRenderer) X(SetCollider) X(SetPolygonCollider) \
    X(Tk2dPlayAnimation) X(Tk2dPlayAnimationWithEvents) X(Tk2dWatchAnimationEvents) X(Tk2dPlayFrame) \
    X(Tk2dSpriteSetColor) X(Tk2dSpriteGetId) \
    X(StopWalker) X(StartWalker) X(SetWalkerFacing) X(WalkLeftRight) \
    X(SetInvincible) X(SetHP) X(InstaDeath) X(SetDamageHeroAmount) X(SetGeoDrop) \
    X(CheckAlertRange) X(CheckAlertRangeByName) X(FindAlertRange) X(CheckCanSeeHero) X(GetCanSeeHero) \
    X(SetFsmBool) X(SetFsmInt) X(SetFsmFloat) X(SetFsmString) X(SetFsmGameObject) X(GetFsmBool) X(GetFsmFloat) \
    X(PlayerDataBoolTest) X(SetPlayerDataBool) X(GetPlayerDataInt) X(SetPlayerDataInt) \
    X(AudioPlayerOneShotSingle) X(AudioPlayerOneShot) X(AudioPlaySimple) X(AudioPlay) X(AudioPlayV2) \
    X(AudioPlayRandom) X(AudioStop) X(SetAudioPitch) X(SetAudioClip) \
    X(SendMessage) X(CallMethodProper) X(WaitForFinishedEnteringScene) X(CheckSceneName) \
    X(GGCheckIsBossRushMode) X(GGCheckIfBossScene) X(CheckIsChineseBuild) \
    X(iTweenMoveBy) X(iTweenScaleBy) X(EaseFloat) X(EaseColor) X(SpawnBlood) X(SpawnBloodTime) \
    X(FaceAngle) X(SetRecoilSpeed) X(RayCast2dV2) X(SetRecoilFreeze) X(ConstrainMovement) X(CheckSendEventLimit) \
    X(TakeDamage) X(AccelerateVelocity) X(LookAt2dGameObjectSmooth) X(BoundsBoxCollider) X(GetVelocityAsAngle) \
    X(PreventInvincibleEffect) X(BoolTestMulti) X(ReceivedDamage) X(GetXDistance) X(SpawnRandomObjectsV2) \
    X(FlingObjectsFromGlobalPoolTime) X(GameObjectIsVisible) X(ProjectileSquash) X(Collision2dEventLayer) \
    X(SetPositionToObject) X(Tk2dSpriteSetScale) X(RandomFloatV2) X(IntAddV2) X(GetVector2XY) X(AddForce2d) \
    X(SetIsKinematic2d) X(SetCircleCollider) X(GetName) X(CompareNames) X(SetVector3Value) X(Vector3Multiply) \
    X(DetachChildren) X(iTweenScaleTo) X(iTweenRotateTo) X(BoolAnyTrue) X(IntSwitch) X(StringSwitch) X(SetTag) \
    X(GetLayer) X(GetTag) X(IncrementPlayerDataInt) X(Rotate) X(GotoPreviousState) X(BuildString) \
    X(SelectRandomString) X(Vector3AddXYZ) X(ConvertIntToFloat) X(ConvertFloatToInt) X(BoxColliderOffset) \
    X(GetFsmInt) X(SetAngularVelocity2d) X(GetSpeed2d) X(PlayerDataBoolAllTrue) X(GetPlayerDataString) \
    X(ShowPromptMarker) X(HidePromptMarker) X(ListenForUp) X(ListenForDown) X(ListenForLeft) X(ListenForRight) \
    X(ListenForJump) X(ListenForAttack) X(ListenForCast) X(ListenForDash) X(ListenForPromptContinue) X(ListenForMenuSubmit) \
    X(ListenForMenuCancel) X(ListenForDreamNail) X(ListenForQuickMap) X(ListenForInventory) X(ListenForSuperdash) X(GetButtonDown) \
    X(AddTrackTrigger) X(CheckTrackTriggerCount) X(PlayerDataBoolTrueAndFalse) X(GetPlayerDataBool) X(SetPlayerDataFloat) \
    X(SetPlayerDataString) X(PlayerDataIntAdd) X(GetPlayerDataFloat) X(ForceHeroFootstepSound) X(IntCompareToBool) \
    X(SendEventToRegister) X(GetLanguageString) X(GetLanguageStringProcessed) X(ConvertIntToString) X(SetSpriteRenderer) \
    X(GetPosition2D) X(Tk2dPlayAnimationDelay) X(SetGameObjectActive) X(LoadLevel) X(SendEventByNameV2) \
    X(SetMaterialColor) X(GetMaterialColor) X(SetColorValue) X(GetColorRGBA) X(Tk2dSpriteGetColor) X(BeginSceneTransition) \
    X(SetTextMeshProColor) X(SetSpriteRendererColor) X(ConvertStringToInt) X(IntTestToBool) \
    X(SetSpriteRendererSprite) X(SetTextMeshProText) X(SetTextMeshText) X(CreateUIMsgGetItem) X(SetTextMeshProGameText) X(GetMeshRendererBounds) \
    X(FadeGroupUp) X(FadeGroupDown) X(SetFloatToHighest) X(DestroyAllChildren) X(WaitForBossLoad) X(SetBoxCollider2DSize) X(GetLastEvent) \
    X(SetFsmVector3) X(iTweenMoveTo) X(SetTextMeshProAlignment) X(SendEventByScale) \
    X(PlayParticleEmitter) X(SetParticleEmission) X(StopParticleEmitter) X(SetParticleEmissionRate) X(SetParticleEmissionSpeed) \
    X(TransitionToAudioSnapshot) X(VibrationPlayerPlay) X(PlayVibration) X(PlayVibrationV2) X(SetProperty) X(ApplyMusicCue) \
    X(GetCurrentLanguageAsString) X(SetName) X(FadeColorFader) X(iTweenShakePosition) X(GetEventSender) X(GetAxis) X(FadeAudio) \
    X(SetAudioVolume) X(GetAngleToTarget) X(ChaseObjectV2) X(DecelerateXY) X(TimeLimitSet) X(TimeLimitCheck) X(DistanceBetweenPoints) \
    X(CheckCurrentMapZone) X(AudioPlayInState) X(SetTrailRenderer) X(GetPreInstantiatedGameObject) X(SetFloatToSmallest) \
    X(GGCheckIfBossSequence) X(DebugLog) X(HasComponent) X(GetVelocity) X(SetVelocity) X(ColorInterpolate) X(RotateTo) \
    X(SpawnRandomObjectsOverTime) X(VibrationPlayerStop) X(SendEventToGameObjectOptimized) X(WaitForHeroInPosition) \
    X(ScaleTo) X(GetTagCount) X(GetNailDamage) X(CreateGameObjectPool) X(BeginRecoil) X(Tk2dStopAnimation) X(SetStaticVariable) \
    X(GetStaticVariable) X(CheckStaticBool) X(IntClamp) X(ListenForQuickCast) X(GGSetCanTransition) X(GetConstantsValue) \
    X(CheckCanDreamWarpInScene) X(SetInterpolate) X(SetExtrapolate) X(SetGameObjectSelf) X(SendMessageV2) \
    X(SpawnObjectFromGlobalPoolOverTime) X(SendDreamImpact) X(FindClosest) \
    X(FlingObjectsFromGlobalPoolVel) X(GetFsmGameObject) X(SendTrigger2DEvent) \
    X(iTweenFadeTo) X(iTweenFadeFrom) \
    X(SendEnemyMessage) X(IdleBuzzV2) X(SetIsDead) X(AnimatorPlay) X(ChaseObjectGround) X(IdleBuzzV3) X(SpawnFromPool) X(GetHP) X(PreBuildTK2DSprites) X(SetHealthManagerReset) X(DistanceFlyV2) X(SetBoxColliderTrigger) X(Tk2dPlayAnimationV2) X(RandomFloatEither) X(GetYDistance) X(RandomlyFlipFloat) X(SendRandomEventV3) X(SpawnRandomObjectsOverTimeV2) X(CallStaticMethod) X(GetChildNum) X(ReflectAngle) X(FloatSignTest) X(BehaviourListAdd) X(BehaviourListRemove) X(GetRandomObject) X(GetColliderRange) X(FlingFlashingGeo) X(SetDreamNailConvo) X(GetMidPoint) X(TransformPoint) X(Vector3Add) X(SetVelocity2dIfFalse) X(SetLayer) X(SetBoxCollider2DSizeVector) X(CreatePoolObjects) X(SetColorRGBA) X(DistanceWalk) X(Tk2dSpriteTweenColor) X(AddForce2dAsAngle) X(ListenForRsDown) X(ListenForRsUp) X(DistanceFlySmooth) X(GameObjectCompareTag) X(CameraRepositionToHero) X(KeepFloatPositive) X(SelectRandomVector3) X(FaceAngleV2) X(GetPersistentBool) X(SetFsmColor) X(SendMessageByTag) X(Vector3Lerp) X(GetAngleBetweenPoints) X(PlayParticleEmitterInState) X(RunAway) X(GetFsmString) X(TranslateContinuous) X(EndGGBossScene) X(MoveLiftChain) X(StopLiftChain) X(GetIsDead) X(EnableFSM) X(SetRigidbodySimulated2D) X(SetCollider2dIsTrigger) X(GetHPEveryFrame) X(PreSpawnCorpse) X(EnemyPusherIgnore) X(AudioPlayRandomSingle) X(ShowGodfinderIcon) X(ShowBossChallengeUI) X(ShowBossDoorChallengeUI) X(GGCheckIfFirstBossScene) X(CheckGGBossLevel) X(GGWaitForBossSceneTransitionIn) \
    X(TranslateV2) X(RandomWait) X(SetBattleScene) X(GetGrandparent) X(SetSpecialDeath) X(SetDamageOverride) X(FloatDivideV2) \
    X(CheckGeoCap) X(AnyKey) X(Vector3Compare) X(SendEventToRegisterV2) X(LerpTk2dSpriteColor) X(LineCast2d) X(GetNextChild) \
    X(FinishFSM) X(Tk2dSpriteSetId) X(SetSpawnJarContents) X(iTweenRotateBy) X(GameObjectHasChildren) X(GameObjectCompare) \
    X(SwitchOnPlatform)

enum {
    A_UNKNOWN = 0,
#define X(n) A_##n,
    ACTS(X)
#undef X
    A_COUNT
};
static const char *anames[] = { "",
#define X(n) #n,
    ACTS(X)
#undef X
};

int act_lookup(const char *name) {
    if (!name) return A_UNKNOWN;
    const char *d = strrchr(name, '.'); if (d) name = d + 1;
    for (int i = 1; i < A_COUNT; i++) if (!strcmp(anames[i], name)) return i;
    return A_UNKNOWN;
}

bool act_has_fixed(int t) {
    switch (t) {
        case A_SetVelocity2d: case A_SetVelocityAsAngle: case A_IdleBuzz: case A_DistanceFly: case A_ChaseObject:
        case A_FireAtTarget: case A_AddForce2dV2: case A_Decelerate: case A_DecelerateV2: case A_ObjectJitter:
        case A_Translate: case A_FloatAddV2: case A_FloatMultiplyV2: case A_WalkLeftRight: case A_FaceAngle:
        case A_AccelerateVelocity: case A_ProjectileSquash: case A_RandomFloatV2: case A_IntAddV2: case A_AddForce2d:
        case A_CheckTrackTriggerCount: case A_ChaseObjectV2: case A_DecelerateXY: case A_TranslateV2: case A_FloatDivideV2: return true;
        case A_IdleBuzzV2: case A_IdleBuzzV3: case A_ChaseObjectGround: case A_DistanceFlyV2: case A_DistanceFlySmooth: case A_DistanceWalk:
        case A_RunAway: case A_SetVelocity2dIfFalse: case A_AddForce2dAsAngle: case A_FaceAngleV2: return true;
    }
    return false;
}
bool act_has_late(int t) {
    switch (t) { case A_SetScale: case A_SetPosition: case A_SetRotation: case A_FlipScale: case A_Translate: case A_TranslateV2: case A_Rotate: case A_GetAngleToTarget: return true; }
    return false;
}
bool act_is_trigger(int t) { return t == A_CheckTrackTriggerCount || t == A_Trigger2dEvent || t == A_SendTrigger2DEvent || t == A_Trigger2dEventLayer || t == A_SendTrigger2DEventByName || t == A_ReceivedDamage; }

/* ---------------- helpers ---------------- */
#define PF(k, d) pf(f, a, k, d)
#define PB(k, d) pb(f, a, k, d)
#define PI(k, d) pi_(f, a, k, d)
#define PS(k, d) ps(f, a, k, d)
#define EV(k) pev(a, k)
#define GO(k) jv_go(f, praw(a, k))
#define VAR(k) pvar(f, a, k)
#define NONE(k) pnone(f, a, k)
#define FIN() fsm_finish(f, a)

static float frand(float lo, float hi) { return lo + (hi - lo) * (rand() / (float)RAND_MAX); }
static int irand(int lo, int hiExcl) { return hiExcl > lo ? lo + rand() % (hiExcl - lo) : lo; }
static void ev(Fsm *f, const char *e) { if (e && e[0]) fsm_event(f, e); }
static void setf(FVar *v, float x) { if (v) var_set_float(v, x); }
static void setb(FVar *v, bool b) { if (v) var_set_bool(v, b); }
static void seti(FVar *v, int i) { if (v) var_set_int(v, i); }
static void setgo(FVar *v, UObj *o) { if (v) v->go = o; }
static void setv(FVar *v, float x, float y, float z) { if (v) { v->v[0] = x; v->v[1] = y; v->v[2] = z; } }
static URb *rbody(UObj *o) { return (o && o->rb.has) ? &o->rb : NULL; }
static bool alive(UObj *o) { return o && !o->destroyed && o->used; }
static const char *aname(JVal *v) { return v && v->t == J_OBJ ? jstr(jget(v, "audio"), NULL) : NULL; }
static float norm360(float a) { a = fmodf(a, 360); if (a < 0) a += 360; return a; }

static void play_clip(const char *clip, float vol, float pmin, float pmax) {
    if (!clip) return;
    float p = (pmin == pmax) ? pmin : frand(pmin, pmax);
    if (p <= 0) p = 1;
    sfx_play_pitch(clip, vol, p);
}

// template (prefab) from an FsmGameObject param: {"prefab": guid} or a variable holding one
static int tmpl_of(Fsm *f, FsmAct *a, const char *key) {
    JVal *v = praw(a, key);
    if (!v || v->t != J_OBJ) return -1;
    JVal *p = jget(v, "prefab");
    if (p) return uo_template_by_key(p->s);
    JVal *vn = jget(v, "var");
    if (vn) { FVar *x = fsm_var(f, vn->s); if (x && x->obj && x->obj->t == J_OBJ && jget(x->obj, "prefab")) return uo_template_by_key(jget(x->obj, "prefab")->s); }
    return -1;
}
#ifdef __HOST__
static int tmpl_of_warn(Fsm *f, FsmAct *a, const char *key) {   // sweep (HK_CHECK): a prefab the room data lacks
    int t = tmpl_of(f, a, key); JVal *v = praw(a, key), *p = v && v->t == J_OBJ ? jget(v, "prefab") : NULL;
    if (t < 0 && p && getenv("HK_CHECK")) {
        static char seen[64][40]; static int ns; bool dup = false;
        for (int i = 0; i < ns; i++) if (!strcmp(seen[i], p->s)) dup = true;
        if (!dup && ns < 64) { snprintf(seen[ns++], 40, "%s", p->s); fprintf(stderr, "WARN missing prefab %s (%s/%s)\n", p->s, f->owner ? f->owner->name : "?", f->name); }
    }
    return t;
}
#define tmpl_of tmpl_of_warn
#endif

static int weighted(Fsm *f, JVal *w) {
    int n = w ? w->n : 0; if (n <= 0) return -1;
    float sum = 0; for (JVal *e = w->child; e; e = e->next) sum += jv_float(f, e, 1);
    float r = frand(0, sum), acc = 0; int i = 0;
    for (JVal *e = w->child; e; e = e->next, i++) { acc += jv_float(f, e, 1); if (r < acc) return i; }
    return n - 1;
}

// FsmColor parameter: [r,g,b,a] or {"var": name}
static bool pcolor(Fsm *f, FsmAct *a, const char *key, float *c) {
    JVal *v = praw(a, key);
    if (v && v->t == J_ARR && v->n >= 4) { for (int i = 0; i < 4; i++) c[i] = (float)jnum(jat(v, i), 1); return true; }
    if (v && v->t == J_OBJ && jget(v, "var")) { FVar *x = fsm_var_j(f, jget(v, "var")); if (x) { for (int i = 0; i < 4; i++) c[i] = x->v[i]; return true; } }
    if (v && v->t == J_OBJ && jget(v, "r")) { c[0] = (float)jnum(jget(v, "r"), 1); c[1] = (float)jnum(jget(v, "g"), 1); c[2] = (float)jnum(jget(v, "b"), 1); c[3] = (float)jnum(jget(v, "a"), 1); return true; }
    return false;
}
static void store_color(FVar *v, const float *c) { if (v) for (int i = 0; i < 4; i++) v->v[i] = c[i]; }

// TrackTriggerObjects.InsideCount (objects overlapping o's triggers) against CheckTrackTriggerCount's test
static bool track_test(Fsm *f, FsmAct *a, UObj *o) {
    int n = 0;
    for (int i = 0; i < o->nov; i++) if (alive(o->ov[i]) && uo_active(o->ov[i])) n++;
    int c = PI("count", 0);
    switch (PI("test", 0)) {
        case 0: return n == c; case 1: return n < c; case 2: return n > c; case 3: return n <= c; case 4: return n >= c;
    }
    return false;
}

// CallMethodProper parameters: {"var": name} or a typed value {"type", "f", "i", "b", "s"}
static const char *param_str(Fsm *f, JVal *p) {
    if (!p || p->t != J_OBJ) return NULL;
    JVal *vn = jget(p, "var"); if (vn && vn->t == J_STR) { FVar *v = fsm_var(f, vn->s); return v ? v->s : NULL; }
    return jstr(jget(p, "s"), NULL);
}
static int param_int(Fsm *f, JVal *p) {
    if (!p || p->t != J_OBJ) return 0;
    JVal *vn = jget(p, "var"); if (vn && vn->t == J_STR) { FVar *v = fsm_var(f, vn->s); return v ? (v->t == V_FLOAT ? (int)v->f : v->i) : 0; }
    int t = jint(jget(p, "type"), 0);
    return t == 1 ? (int)jnum(jget(p, "f"), 0) : jint(jget(p, "i"), 0);
}

static void cam_shake_event(const char *e) {
    if (!e) return;
    if (strstr(e, "Small") || strstr(e, "small")) game_shake(0.12f);
    else if (strstr(e, "Big") || strstr(e, "big")) game_shake(0.45f);
    else if (strstr(e, "Average") || strstr(e, "EnemyKill")) game_shake(0.25f);
    else game_shake(0.2f);
}

static void send_to_go(Fsm *f, UObj *go, const char *fsmName, const char *e, float delay, bool children) {
    if (!alive(go)) return;
    for (int i = 0; i < go->nfsm; i++) {
        Fsm *t = go->fsm[i];
        if (fsmName && fsmName[0] && strcmp(t->name, fsmName)) continue;
        if (delay > 0) fsm_event_delayed_to(f, t, e, delay); else fsm_event(t, e);
        if (fsmName && fsmName[0]) break;
    }
    if (go->native) go->native(go, e);
    if (children) for (UObj *c = go->child; c; c = c->next) send_to_go(f, c, fsmName, e, delay, true);
}

// FsmEventTarget: 0 Self, 1 GameObject, 2 GameObjectFSM, 3 FSMComponent, 4 BroadcastAll, 5 Host, 6 Parent, 7 Sub
static void send_target(Fsm *f, JVal *et, const char *e, float delay) {
    if (!e || !e[0]) return;
    uo_set_event_sender(f->owner);
    int tg = et && et->t == J_OBJ ? jint(jget(et, "target"), 0) : 0;
    if (tg == 4) {
        bool excl = jbool(jget(et, "exclude"), false);
        int n = uo_count();
        for (int i = 0; i < n; i++) {
            UObj *o = uo_obj(i);
            if (!o || o->destroyed || !uo_active(o)) continue;
            if (delay > 0) continue;
            for (int k = 0; k < o->nfsm; k++) { if (excl && o->fsm[k] == f) continue; fsm_event(o->fsm[k], e); }
            if (o->native) o->native(o, e);
        }
        if (delay > 0) fsm_broadcast_delayed(f, e, delay);     // one delayed broadcast (a per-FSM queue only had room for 8)
        return;
    }
    if (tg == 1 || tg == 2) {
        JVal *gv = jget(et, "go");
        UObj *go = (!gv || gv->t == J_NULL) ? f->owner : jv_go(f, gv);
        const char *fn = tg == 2 ? jv_str(f, jget(et, "fsmName"), NULL) : NULL;
        if (!go) {
            if (fn && !strcmp(fn, "CameraShake")) cam_shake_event(e);
            return;
        }
        send_to_go(f, go, fn, e, delay, jbool(jget(et, "children"), false));
        return;
    }
    if (delay > 0) fsm_event_delayed_to(f, f, e, delay); else fsm_event(f, e);
}

static Fsm *fsm_of(Fsm *f, FsmAct *a) {
    UObj *go = GO("gameObject"); if (!go) return NULL;
    const char *fn = PS("fsmName", NULL);
    return fsm_find_on(go, fn);
}

// transform helpers
static void world_rot_dir(UObj *o, float lx, float ly, float *dx, float *dy) {
    float r = uo_wrz(o) * (float)M_PI / 180, c = cosf(r), s = sinf(r);
    *dx = c * lx - s * ly; *dy = s * lx + c * ly;
}
static void set_world_rz(UObj *o, float z) {
    if (o->parent) { float pr = uo_wrz(o->parent); o->rz = z - pr; } else o->rz = z;
}
// first collider bounds (Collider2D.bounds)
static bool bounds(UObj *o, float *x0, float *y0, float *x1, float *y1) {
    if (!o || o->ncol <= 0) return false;
    return uo_col_aabb(o, &o->col[0], x0, y0, x1, y1);
}
static bool ray(float x, float y, float dx, float dy, float len) { float h; return w_raycast(x, y, dx, dy, len, &h); }

static float ease(int type, float t) {
    if (t <= 0) return 0; if (t >= 1) return 1;
    switch (type) {
        case 0: return t * t;                                    // easeInQuad
        case 1: return -t * (t - 2);                             // easeOutQuad
        case 2: return t < 0.5f ? 2 * t * t : -1 + (4 - 2 * t) * t;
        case 3: return t * t * t;
        case 4: { float u = t - 1; return u * u * u + 1; }
        case 5: return t < 0.5f ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
        case 12: return 1 - cosf(t * (float)M_PI / 2);           // easeInSine
        case 13: return sinf(t * (float)M_PI / 2);               // easeOutSine
        case 14: return -0.5f * (cosf((float)M_PI * t) - 1);     // easeInOutSine
        default: return t;                                       // linear (21) and the rest
    }
}

/* ---------------- collision / trigger callbacks (PlayMakerUnity2DProxy) ---------------- */
static void check_touching(Fsm *f, FsmAct *a, bool enterVariant);

// terrain collisions of the owner's rigidbody; kind 0 enter, 1 stay, 2 exit
void act_collision_notify(UObj *o, int kind) {
    for (int i = 0; i < o->nfsm; i++) {
        Fsm *f = o->fsm[i];
        if (!f->started || f->cur < 0) continue;
        int cur = f->cur; unsigned sw = f->nswitch; FState *st = &f->st[cur];
        for (int k = 0; k < st->nact; k++) {
            FsmAct *a = &st->act[k];
            if (!a->entered) continue;
            switch (a->type) {
                case A_CheckCollisionSide: if (kind == 1 || kind == 0) check_touching(f, a, false); break;   // stay delegate
                case A_CheckCollisionSideEnter: if (kind == 0) check_touching(f, a, true); break;
                case A_Collision2dEvent: {
                    int ct = PI("collision", 0);
                    const char *tag = PS("collideTag", NULL);
                    bool tagOk = NONE("collideTag") || !tag || !tag[0] || !strcmp(tag, "Untagged");
                    if (ct == kind && tagOk) ev(f, EV("sendEvent"));
                    break; }
                case A_Collision2dEventLayer: {
                    int ct = PI("collision", 0);
                    bool tagOk = NONE("collideTag") || !PS("collideTag", NULL) || !PS("collideTag", "")[0];
                    bool layOk = NONE("collideLayer") || PI("collideLayer", -1) == 8;
                    if (ct == kind && tagOk && layOk) ev(f, EV("sendEvent"));
                    break; }
            }
            if (f->nswitch != sw || !f->started) break;
        }
    }
}

// the Knight's body collided with the owner's solid collider (Collision2dEvent with collideTag Player)
void act_collision_hero(UObj *o, int kind) {
    for (int i = 0; i < o->nfsm; i++) {
        Fsm *f = o->fsm[i];
        if (!f->started || f->cur < 0) continue;
        int cur = f->cur; unsigned sw = f->nswitch; FState *st = &f->st[cur];
        for (int k = 0; k < st->nact; k++) {
            FsmAct *a = &st->act[k];
            if (!a->entered) continue;
            if (a->type == A_Collision2dEvent) {
                const char *tag = PS("collideTag", NULL);
                if (PI("collision", 0) == kind && !NONE("collideTag") && tag && !strcmp(tag, "Player")) { uo_set_event_sender(uo_hero()); ev(f, EV("sendEvent")); }
            } else if (a->type == A_Collision2dEventLayer) {
                if (PI("collision", 0) == kind && !NONE("collideLayer") && PI("collideLayer", -1) == 9) { uo_set_event_sender(uo_hero()); ev(f, EV("sendEvent")); }
            }
            if (f->nswitch != sw || !f->started) break;
        }
    }
}
// trigger overlaps of one of the owner's colliders with another object's collider; kind 0 enter, 1 stay, 2 exit
void act_trigger_notify(UObj *o, UObj *other, int kind) {
    for (int i = 0; i < o->nfsm; i++) {
        Fsm *f = o->fsm[i];
        if (!f->started || f->cur < 0) continue;
        int cur = f->cur; unsigned sw = f->nswitch; FState *st = &f->st[cur];
        for (int k = 0; k < st->nact; k++) {
            FsmAct *a = &st->act[k];
            if (!a->entered || !act_is_trigger(a->type)) continue;
            if (a->type == A_ReceivedDamage) {
                if (kind == 2) continue;
                const char *tg = PS("collideTag", NULL);
                if (tg && tg[0] && strcmp(other->tag, tg)) continue;
                if (other != uo_hero_slash()) {       // objects whose damages_enemy FSM deals damage
                    Fsm *df = fsm_find_on(other, PS("fsmName", ""));
                    FVar *dv = df ? fsm_var(df, "damageDealt") : NULL;
                    if (!dv || dv->i <= 0) continue;
                }
                setgo(VAR("storeGameObject"), other); ev(f, EV("sendEvent"));
                if (f->nswitch != sw || !f->started) break;
                continue;
            }
            if (PI("trigger", 0) != kind) continue;
            const char *tag = PS("collideTag", NULL);
            bool tagOk = NONE("collideTag") || !tag || !tag[0] || !strcmp(other->tag, tag) || ((a->type == A_Trigger2dEvent || a->type == A_SendTrigger2DEvent) && !strcmp(tag, "Untagged"));
            if (!tagOk) continue;
            if (a->type != A_Trigger2dEvent && a->type != A_SendTrigger2DEvent && !NONE("collideLayer") && PI("collideLayer", -1) != other->layer) continue;
            setgo(VAR("storeCollider"), other);
            if (a->type == A_SendTrigger2DEventByName) send_target(f, praw(a, "eventTarget"), PS("sendEvent", NULL), 0);
            else ev(f, EV("sendEvent"));
            if (f->nswitch != sw || !f->started) break;
        }
    }
}

static void check_touching(Fsm *f, FsmAct *a, bool enterVariant) {
    UObj *o = f->owner;
    float x0, y0, x1, y1;
    if (!bounds(o, &x0, &y0, &x1, &y1)) return;
    float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
    const float L = 0.08f;
    // Box2D keeps bodies a contact offset (0.01) apart, so corner rays don't graze the surface the body rests on
    { const float in = 0.03f; float ox0 = x0, oy0 = y0; x0 += in; x1 -= in; y0 += in; y1 -= in;
      (void)ox0; (void)oy0; }
    if (PB("otherLayer", false) && PI("otherLayerNumber", 8) != 8) return;   // only terrain colliders are modelled
    bool chkU = enterVariant || !NONE("topHit") || EV("topHitEvent");
    bool chkR = enterVariant || !NONE("rightHit") || EV("rightHitEvent");
    bool chkD = enterVariant || !NONE("bottomHit") || EV("bottomHitEvent");
    bool chkL = enterVariant || !NONE("leftHit") || EV("leftHitEvent");
    unsigned cur = f->nswitch;
    if (enterVariant) { setb(VAR("topHit"), false); setb(VAR("rightHit"), false); setb(VAR("bottomHit"), false); setb(VAR("leftHit"), false); }
    if (chkU) {
        if (!enterVariant) setb(VAR("topHit"), false);
        float px[3] = { x0, cx, x1 };
        for (int i = 0; i < 3; i++) if (ray(px[i], y1, 0, 1, L + 0.03f)) { setb(VAR("topHit"), true); ev(f, EV("topHitEvent")); break; }
        if (f->nswitch != cur) return;
    }
    if (chkR) {
        if (!enterVariant) setb(VAR("rightHit"), false);
        float py[3] = { y1, cy, y0 };
        for (int i = 0; i < 3; i++) if (ray(x1, py[i], 1, 0, L + 0.03f)) { setb(VAR("rightHit"), true); ev(f, EV("rightHitEvent")); break; }
        if (f->nswitch != cur) return;
    }
    if (chkD) {
        if (!enterVariant) setb(VAR("bottomHit"), false);
        float px[3] = { x1, cx, x0 };
        bool hitD = false;
        for (int i = 0; i < 3; i++) if (ray(px[i], y0, 0, -1, L + 0.03f)) { hitD = true; break; }
        // the physics step's own floor contact counts too (a body resting on a slope or a ledge corner can miss the
        // three rays, e.g. the Gruz Mother's corpse sliding on forever instead of landing and bursting)
        if (!hitD && o->rb.has && (o->rb.contacts & 1) && o->rb.vy <= 0.01f) hitD = true;
        if (hitD) { setb(VAR("bottomHit"), true); ev(f, EV("bottomHitEvent")); }
        if (f->nswitch != cur) return;
    }
    if (chkL) {
        if (!enterVariant) setb(VAR("leftHit"), false);
        float py[3] = { y0, cy, y1 };
        for (int i = 0; i < 3; i++) if (ray(x0, py[i], -1, 0, L + 0.03f)) { setb(VAR("leftHit"), true); ev(f, EV("leftHitEvent")); break; }
    }
}

/* ---------------- shared bodies ---------------- */
static void do_set_velocity(Fsm *f, FsmAct *a) {
    URb *rb = rbody(GO("gameObject")); if (!rb) return;
    float vx = rb->vx, vy = rb->vy, z;
    if (!NONE("vector")) pvec(f, a, "vector", &vx, &vy, &z);
    if (!NONE("x")) vx = PF("x", 0);
    if (!NONE("y")) vy = PF("y", 0);
    rb->vx = vx; rb->vy = vy;
}
static void do_velocity_angle(Fsm *f, FsmAct *a) {
    URb *rb = rbody(GO("gameObject")); if (!rb) return;
    float ang = PF("angle", 0) * (float)M_PI / 180, sp = PF("speed", 0);
    rb->vx = sp * cosf(ang); rb->vy = sp * sinf(ang);
}
static void do_add_force(Fsm *f, FsmAct *a) {
    URb *rb = rbody(GO("gameObject")); if (!rb) return;
    float fx = 0, fy = 0, z;
    if (!NONE("vector")) pvec(f, a, "vector", &fx, &fy, &z); else { fx = PF("x", 0); fy = PF("y", 0); }
    if (!NONE("vector3")) pvec(f, a, "vector3", &fx, &fy, &z);
    if (!NONE("x")) fx = PF("x", 0);
    if (!NONE("y")) fy = PF("y", 0);
    rb->vx += fx * HERO_FIXED_DT; rb->vy += fy * HERO_FIXED_DT;      // ForceMode2D.Force, mass 1
    if (praw(a, "maxSpeedX") && !NONE("maxSpeedX")) { float m = PF("maxSpeedX", 0); if (rb->vx > m) rb->vx = m; if (rb->vx < -m) rb->vx = -m; }
    if (praw(a, "maxSpeedY") && !NONE("maxSpeedY")) { float m = PF("maxSpeedY", 0); if (rb->vy > m) rb->vy = m; if (rb->vy < -m) rb->vy = -m; }
    if (praw(a, "maxSpeed") && !NONE("maxSpeed")) { float m = PF("maxSpeed", 0), s = sqrtf(rb->vx * rb->vx + rb->vy * rb->vy); if (s > m && s > 0) { rb->vx *= m / s; rb->vy *= m / s; } }
}
static void do_decel(Fsm *f, FsmAct *a, bool mult) {
    URb *rb = rbody(GO("gameObject")); if (!rb) return;
    float d = PF("deceleration", 0);
    float *c[2] = { &rb->vx, &rb->vy };
    for (int i = 0; i < 2; i++) {
        float v = *c[i];
        if (v < 0) { v = mult ? v * d : v + d; if (v > 0) v = 0; }
        else if (v > 0) { v = mult ? v * d : v - d; if (v < 0) v = 0; }
        *c[i] = v;
    }
}
static void clampv(URb *rb, float m) { if (rb->vx > m) rb->vx = m; if (rb->vx < -m) rb->vx = -m; if (rb->vy > m) rb->vy = m; if (rb->vy < -m) rb->vy = -m; }

static void do_idle_buzz(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
    float sx = a->f[0], sy = a->f[1], amax = PF("accelerationMax", 0), rr = PF("roamingRange", 0);
    float *ax = &a->f[2], *ay = &a->f[3], *wait = &a->f[4];
    float x = uo_wx(o), y = uo_wy(o), wmin = PF("waitMin", 0), wmax = PF("waitMax", 0);
    float vx = rb->vx, vy = rb->vy;
    if (y < sy - rr) { if (vy < 0) { *ay = amax / 2000; vy /= 1.125f; *wait = frand(wmin, wmax); } }
    else if (y > sy + rr && vy > 0) { *ay = -amax / 2000; vy /= 1.125f; *wait = frand(wmin, wmax); }
    if (x < sx - rr) { if (vx < 0) { *ax = amax / 2000; vx /= 1.125f; *wait = frand(wmin, wmax); } }
    else if (x > sx + rr && vx > 0) { *ax = -amax / 2000; vx /= 1.125f; *wait = frand(wmin, wmax); }
    if (*wait <= 1e-6f) {
        *ay = y < sy - rr ? frand(0, amax) : y > sy + rr ? frand(-amax, 0) : frand(-amax, amax);
        *ax = x < sx - rr ? frand(0, amax) : x > sx + rr ? frand(-amax, 0) : frand(-amax, amax);
        *ay /= 2000; *ax /= 2000; *wait = frand(wmin, wmax);
    }
    if (*wait > 1e-6f) *wait -= HERO_FIXED_DT;
    vx += *ax; vy += *ay;
    rb->vx = vx; rb->vy = vy; clampv(rb, PF("speedMax", 0));
}
static void do_distance_fly(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float x = uo_wx(o), y = uo_wy(o), tx = uo_wx(t), ty = uo_wy(t), acc = PF("acceleration", 0);
    float d = sqrtf((x - tx) * (x - tx) + (y - ty) * (y - ty));
    bool th = PB("targetsHeight", false);
    if (d > PF("distance", 0)) { rb->vx += x < tx ? acc : -acc; if (!th) rb->vy += y < ty ? acc : -acc; }
    else { rb->vx += x < tx ? -acc : acc; if (!th) rb->vy += y < ty ? -acc : acc; }
    if (th) { float h = PF("height", 0); if (y < ty + h) rb->vy += acc; if (y > ty + h) rb->vy -= acc; }
    clampv(rb, PF("speedMax", 0));
}
static void do_chase(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float spread = PF("targetSpread", 0);
    if (spread > 0) {
        if (a->f[2] >= a->f[3]) { a->f[0] = frand(-spread, spread); a->f[1] = frand(-spread, spread); a->f[2] = 0; a->f[3] = frand(PF("spreadResetTimeMin", 0), PF("spreadResetTimeMax", 0)); }
        else a->f[2] += HERO_FIXED_DT;
    }
    float acc = PF("acceleration", 0);
    rb->vx += uo_wx(o) < uo_wx(t) + a->f[0] ? acc : -acc;
    rb->vy += uo_wy(o) < uo_wy(t) + a->f[1] ? acc : -acc;
    clampv(rb, PF("speedMax", 0));
}
static void do_fire_at(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float px = 0, py = 0, pz;
    pvec(f, a, "position", &px, &py, &pz);
    float ang = atan2f(uo_wy(t) + py - uo_wy(o), uo_wx(t) + px - uo_wx(o)) * 180 / (float)M_PI;
    if (!NONE("spread")) { float s = PF("spread", 0); ang += frand(-s, s); }
    float sp = PF("speed", 0);
    rb->vx = sp * cosf(ang * (float)M_PI / 180); rb->vy = sp * sinf(ang * (float)M_PI / 180);
}

static void do_face_object(Fsm *f, FsmAct *a) {
    UObj *A = GO("objectA"), *B = GO("objectB");
    if (!alive(A) || !alive(B)) { FIN(); return; }
    float xs = a->f[0], cur = A->sx, want;
    bool fr = PB("spriteFacesRight", false);
    if (uo_wx(A) < uo_wx(B)) want = fr ? xs : -xs; else want = fr ? -xs : xs;
    if (fabsf(cur - want) > 1e-5f) {
        A->sx = want;
        if (PB("resetFrame", false)) uo_anim_play_frame(A, 0);
        if (PB("playNewAnimation", false)) uo_anim_play(A, PS("newAnimationClip", NULL), false);
    }
}
static void do_face_direction(Fsm *f, FsmAct *a, float dt) {
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
    float xs = a->f[0];
    if (a->f[1] <= 0 || !PB("pauseBetweenTurns", false)) {
        bool fr = PB("spriteFacesRight", false);
        float want = rb->vx > 0 ? (fr ? xs : -xs) : (fr ? -xs : xs);
        if (o->sx != want) {
            a->f[1] = PF("pauseTime", 0); o->sx = want;
            if (PB("playNewAnimation", false)) { uo_anim_play(o, PS("newAnimationClip", NULL), false); uo_anim_play_frame(o, 0); }
        }
    } else a->f[1] -= dt;
}

static void do_translate(Fsm *f, FsmAct *a, float dt) {
    UObj *o = GO("gameObject"); if (!o) return;
    float x = 0, y = 0, z;
    if (!NONE("vector")) pvec(f, a, "vector", &x, &y, &z); else { x = PF("x", 0); y = PF("y", 0); }
    if (!NONE("x")) x = PF("x", 0);
    if (!NONE("y")) y = PF("y", 0);
    if (PB("perSecond", true)) { x *= dt; y *= dt; }
    if (PI("space", 1) == 1) { float dx, dy; world_rot_dir(o, x, y, &dx, &dy); x = dx; y = dy; }
    uo_set_world_pos(o, uo_wx(o) + x, uo_wy(o) + y);
    if (a->type == A_TranslateV2) {        // the City of Tears lifts stop at their ends
        if (!NONE("yMin") && uo_wy(o) < PF("yMin", 0)) uo_set_world_pos(o, uo_wx(o), PF("yMin", 0));
        if (!NONE("yMax") && uo_wy(o) > PF("yMax", 0)) uo_set_world_pos(o, uo_wx(o), PF("yMax", 0));
    }
}
static void do_set_position(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); if (!o) return;
    bool world = PI("space", 1) == 0;
    float x = world ? uo_wx(o) : o->lx, y = world ? uo_wy(o) : o->ly, z = o->lz;
    if (!NONE("vector")) pvec(f, a, "vector", &x, &y, &z);
    if (!NONE("x")) x = PF("x", 0);
    if (!NONE("y")) y = PF("y", 0);
    if (!NONE("z")) z = PF("z", 0);
    if (world) uo_set_world_pos(o, x, y); else { o->lx = x; o->ly = y; }
    o->lz = z;
}
static void do_set_scale(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); if (!o) return;
    float x = o->sx, y = o->sy, z = 1;
    if (!NONE("vector")) pvec(f, a, "vector", &x, &y, &z);
    if (!NONE("x")) x = PF("x", 0);
    if (!NONE("y")) y = PF("y", 0);
    o->sx = x; o->sy = y;
}
static void do_set_rotation(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); if (!o) return;
    bool self = PI("space", 0) == 1;
    float z = self ? o->rz : uo_wrz(o), vx, vy, vz;
    if (!NONE("vector") && pvec(f, a, "vector", &vx, &vy, &vz)) z = vz;
    if (!NONE("zAngle")) z = PF("zAngle", 0);
    if (self) o->rz = z; else set_world_rz(o, z);
}
static void do_flip(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); if (!o) return;
    if (PB("flipHorizontally", false)) o->sx = -o->sx;
    if (PB("flipVertically", false)) o->sy = -o->sy;
}
static void do_get_position(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); if (!o) return;
    bool world = PI("space", 0) == 0;
    float x = world ? uo_wx(o) : o->lx, y = world ? uo_wy(o) : o->ly;
    setv(VAR("vector"), x, y, o->lz); setf(VAR("x"), x); setf(VAR("y"), y); setf(VAR("z"), o->lz);
}
static void do_get_velocity(Fsm *f, FsmAct *a) {
    URb *rb = rbody(GO("gameObject")); if (!rb) return;
    setv(VAR("vector"), rb->vx, rb->vy, 0); setf(VAR("x"), rb->vx); setf(VAR("y"), rb->vy);
}

static void do_activate(Fsm *f, FsmAct *a, UObj *o, bool act, bool rec) {
    if (!o) return;
    uo_set_active(o, act);
    if (rec) for (UObj *c = o->child; c; c = c->next) do_activate(f, a, c, act, true);
}

// prefabs the runtime implements natively (see build_objs.NATIVE_PREFABS / the HUD prefabs)
static const char *prefab_guid(Fsm *f, FsmAct *a, const char *key) {
    JVal *v = praw(a, key);
    if (!v || v->t != J_OBJ) return NULL;
    JVal *p = jget(v, "prefab");
    return p && p->t == J_STR ? p->s : NULL;
}
static UObj *native_spawn(const char *guid, float x, float y) {
    if (!guid) return NULL;
    if (!strncmp(guid, "b76d47fb", 8) || !strncmp(guid, "f5d6f4fe", 8)) return prompt_create(x, y);       // Arrow Prompt (New)
    if (!strncmp(guid, "d4ba7079", 8) || !strncmp(guid, "c5f862c8", 8)) {                                   // Area Title (Extra)
        UObj *at = uo_global("AreaTitle"); if (at) uo_set_active(at, true); return at;
    }
    return NULL;
}

static UObj *spawn_at(int ti, Fsm *f, FsmAct *a, bool poolEuler, UObj **outSp) {
    UObj *sp = GO("spawnPoint");
    float x = 0, y = 0, z = 0, rx, ry, rz = 0;
    if (alive(sp)) {
        x = uo_wx(sp); y = uo_wy(sp);
        float px, py, pz; if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x += px; y += py; }
        if (!NONE("rotation") && pvec(f, a, "rotation", &rx, &ry, &rz)) {} else rz = uo_wrz(sp);
    } else {
        float px, py, pz; if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x = px; y = py; }
        if (!NONE("rotation") && pvec(f, a, "rotation", &rx, &ry, &rz)) {}
    }
    (void)poolEuler; (void)z;
    if (outSp) *outSp = sp;
    if (ti < 0) return native_spawn(prefab_guid(f, a, "gameObject"), x, y);
    return uo_spawn(ti, x, y, rz);
}

static void fling_from(Fsm *f, FsmAct *a, int ti, bool pool) {
    UObj *sp = GO("spawnPoint");
    float x = 0, y = 0, px, py, pz;
    if (alive(sp)) { x = uo_wx(sp); y = uo_wy(sp); if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x += px; y += py; } }
    else if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x = px; y = py; }
    int n = irand(PI("spawnMin", 0), PI("spawnMax", 0) + 1);
    for (int i = 0; i < n; i++) {
        UObj *g = uo_spawn(ti, x, y, 0);
        if (!g) break;
        float gx = x, gy = y;
        if (pool) {
            if (praw(a, "originVariationX")) gx += frand(-PF("originVariationX", 0), PF("originVariationX", 0));
            if (praw(a, "originVariationY")) gy += frand(-PF("originVariationY", 0), PF("originVariationY", 0));
        } else if (praw(a, "originVariation")) { float v = PF("originVariation", 0); gx += frand(-v, v); gy += frand(-v, v); }
        uo_set_world_pos(g, gx, gy);
        float s = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180;
        if (g->rb.has) { g->rb.vx = s * cosf(an); g->rb.vy = s * sinf(an); }
        if (pool && !NONE("FSM")) { Fsm *t = fsm_find_on(g, PS("FSM", NULL)); if (t) fsm_event(t, PS("FSMEvent", NULL)); }
    }
}

static UObj *find_path(UObj *o, const char *path) {
    char buf[96]; strncpy(buf, path, 95); buf[95] = 0;
    char *sv, *t = strtok_r(buf, "/", &sv);
    while (t && o) { o = uo_find_child(o, t); t = strtok_r(NULL, "/", &sv); }
    return o;
}
static UObj *find_desc(UObj *o, const char *name, const char *tag) {
    for (UObj *c = o->child; c; c = c->next) {
        if (c->destroyed) continue;
        if ((!name || !name[0] || !strcmp(c->name, name)) && (!tag || !tag[0] || !strcmp(tag, "Untagged") || !strcmp(c->tag, tag))) return c;
        UObj *r = find_desc(c, name, tag); if (r) return r;
    }
    return NULL;
}
// AlertRange.Find(root, childName)
static UObj *alert_find(UObj *root, const char *child) {
    if (!root) return NULL;
    if (child && child[0]) {
        UObj *c = find_desc(root, child, NULL);
        return c && uo_comp(c, C_ALERTRANGE) ? c : NULL;
    }
    if (uo_comp(root, C_ALERTRANGE)) return root;
    for (UObj *c = root->child; c; c = c->next) { UObj *r = alert_find(c, NULL); if (r) return r; }
    return NULL;
}

/* ---------------- tk2d ---------------- */
static void tk_delegates(Fsm *f, FsmAct *a, UObj *o) {
    const char *te = EV("animationTriggerEvent"), *ce = EV("animationCompleteEvent");
    uo_anim_set_callbacks(o, f, ce, te, ce != NULL, te != NULL);
}

/* ---------------- tweens ---------------- */
static void tween_step(Fsm *f, FsmAct *a, float dt, bool scale) {
    UObj *o = GO("gameObject"); if (!o) return;
    float time = NONE("time") ? 1 : PF("time", 1);
    if (!NONE("speed")) { float sp = PF("speed", 0); float d = sqrtf(a->f[2] * a->f[2] + a->f[3] * a->f[3]); time = sp > 0 ? d / sp : 1; }
    a->t0 += dt;
    if (a->t0 < 0) return;
    float t = time > 0 ? a->t0 / time : 1;
    int loop = PI("loopType", 0);
    bool done = false;
    if (t >= 1) { if (loop == 0) { t = 1; done = true; } else { a->t0 = 0; if (loop == 2) a->b1 = !a->b1; t = 1; } }
    float e = ease(PI("easeType", 21), t);
    if (a->b1) e = 1 - e;
    if (scale) { o->sx = a->f[0] * (1 + (a->f[2] - 1) * e); o->sy = a->f[1] * (1 + (a->f[3] - 1) * e); }
    else {
        float dx = a->f[2] * e, dy = a->f[3] * e;
        if (PI("space", 0) == 1) { float rx, ry; world_rot_dir(o, dx, dy, &rx, &ry); dx = rx; dy = ry; }
        uo_set_world_pos(o, a->f[0] + dx, a->f[1] + dy);
    }
    if (loop != 0 && t >= 1 && a->b1 == false && loop == 1) {   // restart from the start position
        if (!scale) uo_set_world_pos(o, a->f[0], a->f[1]); else { o->sx = a->f[0]; o->sy = a->f[1]; }
    }
    if (done) { FIN(); ev(f, EV("finishEvent")); }
}

/* ---------------- lifecycle ---------------- */


/* StaticVariableList (SetStaticVariable / GetStaticVariable / CheckStaticBool) */
typedef struct { char name[40]; float f; int i; bool b; char s[64]; } StaticVar;
static StaticVar svars[32]; static int nsvars;
static StaticVar *svar(const char *n, bool make) {
    if (!n) return NULL;
    for (int i = 0; i < nsvars; i++) if (!strcmp(svars[i].name, n)) return &svars[i];
    if (!make || nsvars >= 32) return NULL;
    StaticVar *v = &svars[nsvars++]; memset(v, 0, sizeof *v); snprintf(v->name, sizeof v->name, "%s", n); return v;
}
/* ---------------- C# methods called from FSMs (CallMethodProper / SendMessage) ---------------- */
typedef struct { int i; float f; bool b; const char *s; UObj *go; bool set; } MArg;
static MArg marg_val(Fsm *f, JVal *v) {      // a literal or {"var": name}
    MArg m = { 0 };
    if (!v) return m;
    m.set = true;
    if (v->t == J_OBJ && jget(v, "var")) {
        FVar *x = fsm_var_j(f, jget(v, "var"));
        if (x) { m.i = x->t == V_FLOAT ? (int)x->f : x->i; m.f = x->t == V_FLOAT ? x->f : (float)x->i; m.b = x->t == V_BOOL ? x->b : x->i != 0; m.s = x->s; m.go = x->go; }
        return m;
    }
    if (v->t == J_OBJ) { m.go = jv_go(f, v); return m; }
    if (v->t == J_STR) { m.s = v->s; return m; }
    m.f = (float)jnum(v, 0); m.i = (int)m.f; m.b = m.i != 0;
    return m;
}
static MArg marg_fc(Fsm *f, JVal *fc) { return fc ? marg_val(f, jget(fc, "v")) : (MArg){ 0 }; }
static MArg marg_cp(Fsm *f, JVal *p) {        // CallMethodProper parameter: {"var"} or {"type","f","i","b","s"}
    MArg m = { 0 };
    if (!p || p->t != J_OBJ) return m;
    if (jget(p, "var")) return marg_val(f, p);
    m.set = true; m.f = (float)jnum(jget(p, "f"), 0); m.i = jint(jget(p, "i"), 0); m.b = jint(jget(p, "b"), 0) != 0; m.s = jstr(jget(p, "s"), NULL);
    JVal *tv = jget(p, "type");
    int t = tv && tv->t == J_STR ? atoi(tv->s) : jint(tv, 0);     // the export writes the type as a string ("1" = int)
    if (t == 0) m.i = (int)m.f;         // float
    if (t == 1) m.f = (float)m.i;       // int
    if (t == 2) { m.i = m.b; m.f = m.b; }
    return m;
}
static void sets(FVar *v, const char *s) { if (v) snprintf(v->s, sizeof v->s, "%s", s ? s : ""); }
static void call_method(Fsm *f, UObj *target, const char *b, const char *m, MArg a0, MArg a1, FVar *res) {
    (void)target; (void)a1;
    if (!b || !m) return;
    { float r = 0; if (comp2_call(target, b, m, a0.f, &r)) {
        if (res) { if (res->t == V_BOOL) setb(res, r != 0); else if (res->t == V_INT) seti(res, (int)r); else setf(res, r); }
        return; } }
    if (!strcmp(b, "HeroController")) {
        if (!strcmp(m, "Bounce")) hero_bounce();
        else if (!strcmp(m, "RecoilLeft")) hero_recoil_left();
        else if (!strcmp(m, "RecoilRight")) hero_recoil_right();
        else if (!strcmp(m, "RecoilDown")) hero_recoil_down();
        else if (!strcmp(m, "GetState") || !strcmp(m, "GetCState")) setb(res, hero_get_state(a0.s));
        else if (!strcmp(m, "SetCState")) hero_set_cstate(a0.s, a1.b);
        else if (!strcmp(m, "CanTalk") || !strcmp(m, "CanInspect") || !strcmp(m, "CanInteract")) setb(res, hero_can_talk());
        else if (!strcmp(m, "CanCast")) setb(res, hero_can_cast());
        else if (!strcmp(m, "CanFocus")) setb(res, hero_can_focus());
        else if (!strcmp(m, "CanNailArt")) setb(res, hero_can_nail_art());
        else if (!strcmp(m, "CanQuickMap")) setb(res, hero_can_quick_map());
        else if (!strcmp(m, "CanSuperDash")) setb(res, hero_can_superdash());
        else if (!strcmp(m, "CanDreamNail")) setb(res, hero_can_dream_nail());
        else if (!strcmp(m, "CanInput")) setb(res, hero_can_input());
        else if (!strcmp(m, "SetBenchRespawn")) game_set_bench_respawn();
        else if (!strcmp(m, "AffectedByGravity")) hero_affected_by_gravity(a0.set ? a0.b : true);
        else if (!strcmp(m, "RelinquishControl") || !strcmp(m, "RelinquishControlNotVelocity")) hero_relinquish_control();
        else if (!strcmp(m, "RegainControl")) hero_regain_control();
        else if (!strcmp(m, "StopAnimationControl")) hero_stop_anim_control();
        else if (!strcmp(m, "StartAnimationControl") || !strcmp(m, "StartAnimationControlLeavingScene")) hero_start_anim_control();
        else if (!strcmp(m, "FaceLeft")) hero_face_left();
        else if (!strcmp(m, "FaceRight")) hero_face_right();
        else if (!strcmp(m, "FlipSprite")) hero_flip_sprite();
        else if (!strcmp(m, "TakeGeo")) game_take_geo(a0.i);
        else if (!strcmp(m, "AddGeo") || !strcmp(m, "AddGeoQuietly") || !strcmp(m, "AddGeoToCounter")) game_add_geo(a0.i);
        else if (!strcmp(m, "AddHealth")) game_add_health(a0.i);
        else if (!strcmp(m, "MaxHealth")) game_max_health();
        else if (!strcmp(m, "AddToMaxHealth")) game_add_max_health(a0.i);
        else if (!strcmp(m, "CharmUpdate")) game_charm_update();
        else if (!strcmp(m, "AddMPCharge") || !strcmp(m, "AddMPChargeSpa") || !strcmp(m, "TryAddMPChargeSpa")) {
            extern int game_soul(void); int before = game_soul(); game_add_soul(a0.i); setb(res, game_soul() > before); }   // TryAddMPChargeSpa: false once full
        else if (!strcmp(m, "SetMPCharge")) hero_set_mp(a0.i);
        else if (!strcmp(m, "TakeMP") || !strcmp(m, "TakeMPQuick")) hero_take_mp(a0.i);
        else if (!strcmp(m, "StartMPDrain")) hero_start_mp_drain(a0.f);
        else if (!strcmp(m, "StopMPDrain")) hero_stop_mp_drain();
        else if (!strcmp(m, "QuakeInvuln")) hero_quake_invuln();
        else if (!strcmp(m, "ResetAirMoves")) hero_reset_air_moves();
        else if (!strcmp(m, "SetDamageMode") || !strcmp(m, "SetDamageModeFSM")) hero_set_damage_mode(a0.i);
        else if (!strcmp(m, "ResetHardLandingTimer")) hero_reset_hard_landing();
        else if (!strcmp(m, "CancelAttack")) hero_cancel_attack();
        else if (!strcmp(m, "IsSwimming")) hero_set_cstate("swimming", true);
        else if (!strcmp(m, "NotSwimming")) hero_set_cstate("swimming", false);
        else if (!strcmp(m, "SetStartWithJump")) { extern void hero_set_start_with_jump(void); hero_set_start_with_jump(); }
        else if (!strcmp(m, "FindGroundPoint")) {     // the start point's vector didn't survive the export; the scripts SetPosition him there first
            extern void hero_find_ground_point(float *x, float *y); UObj *h = uo_hero(); float gx = h ? uo_wx(h) : 0, gy = h ? uo_wy(h) : 0; hero_find_ground_point(&gx, &gy); setv(res, gx, gy, 0); }
        else if (!strcmp(m, "SetBackOnGround")) hero_set_cstate("onGround", true);
        else if (!strcmp(m, "SetHazardRespawn")) { }
        else if (!strcmp(m, "EnterWithoutInput")) { }
    } else if (!strcmp(b, "GameManager")) {
        if (!strcmp(m, "AwardAchievement") || !strcmp(m, "QueueAchievement")) { extern void ach_award(const char *); ach_award(a0.s); }
        else if (!strcmp(m, "CheckAllAchievements")) { extern void ach_check_all(void); ach_check_all(); }
        else if (!strcmp(m, "GetSceneNameString")) sets(res, uo_room_name());
        else if (!strcmp(m, "GetCurrentMapZone")) sets(res, game_map_zone());
        else if (!strcmp(m, "IsGameplayScene")) setb(res, true);
        else if (!strcmp(m, "ChangeToScene")) { if (a0.s && a0.s[0]) game_begin_transition(a0.s, a1.s); }
        else if (!strcmp(m, "SaveGame")) game_save_now();
        else if (!strcmp(m, "UnequipCharm")) { char k[40]; snprintf(k, sizeof k, "equippedCharm_%d", a0.i); pd_set_bool(k, false); game_charm_update(); }
        else if (!strcmp(m, "UpdateGameMap")) setb(res, map_update());
        // the Hunter reads these to tell whether the Journal is complete; unset, 0 >= 0 read as "complete" and he
        // handed over the Hunter's Mark on the second visit (journal_count.c, tools/gen_journal_count.py)
        else if (!strcmp(m, "CountJournalEntries")) { extern void journal_count_entries(void); journal_count_entries(); }
    }
}


/* ---------------- batch 3: more movement / utility actions ---------------- */
static void do_idle_buzz_v(Fsm *f, FsmAct *a, bool v3) {      // IdleBuzzV2 / V3 (separate X / Y roaming ranges)
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
    float sx = a->f[0], sy = a->f[1], amax = PF("accelerationMax", 0), amin = v3 ? PF("accelerationMin", 0) : 0, rx = PF("roamingRangeX", 0), ry = PF("roamingRangeY", 0);
    float *ax = &a->f[2], *ay = &a->f[3], *wait = &a->f[4];
    float x = uo_wx(o), y = uo_wy(o), wmin = PF("waitMin", 0), wmax = PF("waitMax", 0), vx = rb->vx, vy = rb->vy;
    if (y < sy - ry) { if (vy < 0) { *ay = amax / 2000; vy /= 1.125f; *wait = frand(wmin, wmax); } }
    else if (y > sy + ry && vy > 0) { *ay = -amax / 2000; vy /= 1.125f; *wait = frand(wmin, wmax); }
    if (x < sx - rx) { if (vx < 0) { *ax = amax / 2000; vx /= 1.125f; *wait = frand(wmin, wmax); } }
    else if (x > sx + rx && vx > 0) { *ax = -amax / 2000; vx /= 1.125f; *wait = frand(wmin, wmax); }
    if (*wait <= 1e-6f) {
        *ay = y < sy - ry ? frand(v3 ? amin : 0, amax) : y > sy + ry ? frand(-amax, v3 ? amin : 0) : frand(-amax, amax);
        *ax = x < sx - rx ? frand(v3 ? amin : 0, amax) : x > sx + rx ? frand(-amax, v3 ? amin : 0) : frand(-amax, amax);
        *ay /= 2000; *ax /= 2000; *wait = frand(wmin, wmax);
    }
    if (*wait > 1e-6f) *wait -= HERO_FIXED_DT;
    vx += *ax; vy += *ay; rb->vx = vx; rb->vy = vy; clampv(rb, PF("speedMax", 0));
}
static void do_chase_ground(Fsm *f, FsmAct *a, bool away) {     // ChaseObjectGround / RunAway
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float tr = away ? 0 : PF("turnRange", 0), acc = PF("acceleration", 0), m = PF("speedMax", 0);
    if (!away && !(uo_wx(o) < uo_wx(t) - tr) && !(uo_wx(o) > uo_wx(t) + tr)) return;
    bool right = uo_wx(o) < uo_wx(t); if (away) right = !right;
    rb->vx += right ? acc : -acc;
    if (PB("animateTurnAndRun", false)) {
        bool turning = a->b2;
        if (right) { if (rb->vx < 0 && !turning) { uo_anim_play(o, PS("turnAnimation", NULL), false); a->b2 = true; } if (rb->vx > 0 && turning) { uo_anim_play(o, PS("runAnimation", NULL), false); a->b2 = false; } }
        else { if (rb->vx > 0 && !turning) { uo_anim_play(o, PS("turnAnimation", NULL), false); a->b2 = true; } if (rb->vx < 0 && turning) { uo_anim_play(o, PS("runAnimation", NULL), false); a->b2 = false; } }
    }
    if (rb->vx > m) rb->vx = m; if (rb->vx < -m) rb->vx = -m;
}
static void do_distance_fly2(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float x = uo_wx(o), y = uo_wy(o), tx = uo_wx(t), ty = uo_wy(t), acc = PF("acceleration", 0), d = sqrtf((x - tx) * (x - tx) + (y - ty) * (y - ty));
    bool th = PB("targetsHeight", false), sl = PB("stayLeft", false), sr = PB("stayRight", false), far = d > PF("distance", 0);
    if (sl && x > tx + 1) rb->vx -= acc; else if (sr && x < tx - 1) rb->vx += acc;
    else if (far) rb->vx += x < tx ? acc : -acc; else rb->vx += x < tx ? -acc : acc;
    if (!th) { if (far) rb->vy += y < ty ? acc : -acc; else rb->vy += y < ty ? -acc : acc; }
    else { float h = PF("height", 0); if (y < ty + h) rb->vy += acc; if (y > ty + h) rb->vy -= acc; }
    clampv(rb, PF("speedMax", 0));
}
static void do_distance_fly_smooth(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float ox = 0, oy = 0, oz; if (!NONE("offset")) pvec(f, a, "offset", &ox, &oy, &oz);
    float tx = uo_wx(t) + ox, ty = uo_wy(t) + oy, dx = tx - uo_wx(o), dy = ty - uo_wy(o), d = sqrtf(dx * dx + dy * dy);
    float dist = PF("distance", 0), rad = PF("targetRadius", 0);
    if (!(d > dist - rad) || !(d < dist + rad)) {
        float l = d > 1 ? d : 1, fx = dx / l * PF("accelerationForce", 0), fy = dy / l * PF("accelerationForce", 0);
        if (d < dist) { fx = -fx; fy = -fy; }
        rb->vx += fx * HERO_FIXED_DT; rb->vy += fy * HERO_FIXED_DT;      // AddForce (mass 1)
        float m = PF("speedMax", 0), sp = sqrtf(rb->vx * rb->vx + rb->vy * rb->vy); if (sp > m && sp > 0) { rb->vx *= m / sp; rb->vy *= m / sp; }
    } else { float k = PF("deceleration", 0.9f); rb->vx *= k; rb->vy *= k; }
}
static void do_distance_walk(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"), *t = GO("target"); URb *rb = rbody(o); if (!rb || !alive(t)) return;
    float d = fabsf(uo_wx(o) - uo_wx(t)), dist = PF("distance", 0), rng = PF("range", 0), sp = PF("speed", 0), vx = rb->vx;
    bool *mr = &a->b1; float *ct = &a->t3;
    bool left = uo_wx(o) < uo_wx(t);
    if (d > dist + rng) { if (left) { if (!*mr && *ct <= 0) { vx = sp; *mr = true; *ct = 0.6f; } } else if (*mr && *ct <= 0) { vx = -sp; *mr = false; *ct = 0.6f; } }
    else if (d < dist - rng) { if (left) { if (*mr && *ct <= 0) { vx = -sp; *mr = false; *ct = 0.6f; } } else if (!*mr && *ct <= 0) { vx = sp; *mr = true; *ct = 0.6f; } }
    if (rb->vx > -0.1f && rb->vx < 0.1f) { if (rand() & 1) { vx = sp; *mr = true; } else { vx = -sp; *mr = false; } }
    rb->vx = vx;
    if (PB("changeAnimation", false)) {
        bool fr = PB("spriteFacesRight", false), fwd = (fr && *mr) || (!fr && !*mr);
        if (o->sx <= 0) fwd = !fwd;
        uo_anim_play(o, PS(fwd ? "forwardAnimation" : "backAnimation", NULL), false);
    }
}
static void do_add_force_angle(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
    float an = PF("angle", 0) * (float)M_PI / 180, sp = PF("speed", 0);
    rb->vx += sp * cosf(an) * HERO_FIXED_DT; rb->vy += sp * sinf(an) * HERO_FIXED_DT;
    if (!NONE("maxSpeedX")) { float m = PF("maxSpeedX", 0); if (rb->vx > m) rb->vx = m; if (rb->vx < -m) rb->vx = -m; }
    if (!NONE("maxSpeedY")) { float m = PF("maxSpeedY", 0); if (rb->vy > m) rb->vy = m; if (rb->vy < -m) rb->vy = -m; }
    if (!NONE("maxSpeed")) { float m = PF("maxSpeed", 0), v = sqrtf(rb->vx * rb->vx + rb->vy * rb->vy); if (v > m && v > 0) { rb->vx *= m / v; rb->vy *= m / v; } }
}
static void do_face_angle_v2(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
    float z = atan2f(rb->vy, rb->vx) * 180 / (float)M_PI + PF("angleOffset", 0);
    if (PB("worldSpace", false) && o->parent) o->rz = z - uo_wrz(o->parent); else o->rz = z;
}
static void do_set_vel_if_false(Fsm *f, FsmAct *a) {
    UObj *o = GO("gameObject"); URb *rb = rbody(o); if (!rb || PB("checkBool", false)) return;
    float vx = rb->vx, vy = rb->vy, z; if (!NONE("vector")) pvec(f, a, "vector", &vx, &vy, &z);
    if (!NONE("x")) vx = PF("x", 0); if (!NONE("y")) vy = PF("y", 0);
    rb->vx = vx; rb->vy = vy;
}

void act_enter(Fsm *f, FsmAct *a) {
    UObj *o;
    switch (a->type) {
    case A_UNKNOWN: FIN(); break;
    /* --- batch 3 --- */
    case A_PreBuildTK2DSprites: case A_AnimatorPlay: case A_CreatePoolObjects: case A_BehaviourListAdd: case A_BehaviourListRemove:
    case A_PlayParticleEmitterInState: o = GO("gameObject"); if (o) particles_obj_play(o, true); FIN(); break;
    case A_EnemyPusherIgnore: case A_ShowGodfinderIcon: case A_CameraRepositionToHero:
    case A_ListenForRsDown: case A_ListenForRsUp: case A_PreSpawnCorpse: case A_Tk2dSpriteTweenColor:
        FIN(); break;
    case A_CallStaticMethod: {
        const char *cl = PS("className", ""), *m = PS("methodName", "");
        if (!strcmp(cl, "EnemyDeathEffects") && !strcmp(m, "RecordKillForJournal")) { /* journal kills: counted by HealthManager */ }
        FIN(); break; }
    case A_SendEnemyMessage: { o = GO("Target"); const char *e = PS("EventString", NULL);
        if (o && e) { if (!strcmp(e, "GO LEFT")) walker_go(o, -1); else if (!strcmp(e, "GO RIGHT")) walker_go(o, 1); }
        FIN(); break; }
    case A_IdleBuzzV2: case A_IdleBuzzV3: {
        o = GO("gameObject"); if (o) { a->f[0] = uo_wx(o); a->f[1] = uo_wy(o); }
        float x, y, z; if (!NONE("manualStartPos") && pvec(f, a, "manualStartPos", &x, &y, &z)) { a->f[0] = x; a->f[1] = y; }
        do_idle_buzz_v(f, a, a->type == A_IdleBuzzV3); break; }
    case A_ChaseObjectGround: a->b2 = false; do_chase_ground(f, a, false); break;
    case A_RunAway: a->b2 = false; do_chase_ground(f, a, true); break;
    case A_DistanceFlyV2: do_distance_fly2(f, a); break;
    case A_DistanceFlySmooth: do_distance_fly_smooth(f, a); break;
    case A_DistanceWalk: a->b1 = false; a->t3 = 0; do_distance_walk(f, a); break;
    case A_AddForce2dAsAngle: do_add_force_angle(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_FaceAngleV2: do_face_angle_v2(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetVelocity2dIfFalse: do_set_vel_if_false(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetIsDead: { o = GO("target"); if (o) hm_set_dead(o, PB("setValue", false)); FIN(); break; }
    case A_GetIsDead: { o = GO("target"); setb(VAR("storeValue"), o && hm_is_dead(o)); FIN(); break; }
    case A_GetHP: case A_GetHPEveryFrame: { o = GO("target"); if (o && uo_comp(o, C_HEALTH)) seti(VAR("storeValue"), hm_hp(o)); if (a->type == A_GetHP) FIN(); break; }
    case A_SetHealthManagerReset: { o = GO("target"); if (o) hm_set_death_reset(o, PB("reset", false)); FIN(); break; }
    case A_SetBoxColliderTrigger: { o = GO("gameObject"); if (o) for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { o->col[i].trig = PB("trigger", false); break; } FIN(); break; }
    case A_SetCollider2dIsTrigger: { o = GO("gameObject"); if (o) for (int i = 0; i < o->ncol; i++) { o->col[i].trig = PB("isTrigger", false); if (!PB("setAllColliders", false)) break; } FIN(); break; }
    case A_SetRigidbodySimulated2D: { o = GO("gameObject"); if (o && o->rb.has) o->rb.sim = PB("isSimulated", true); FIN(); break; }
    case A_SetLayer: { o = GO("gameObject"); if (o) o->layer = PI("layer", o->layer); FIN(); break; }
    case A_SetBoxCollider2DSizeVector: { o = GO("gameObject1"); float x, y, z;
        if (o) for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) {
            if (!NONE("size") && pvec(f, a, "size", &x, &y, &z)) { o->col[i].w = x; o->col[i].h = y; }
            if (!NONE("offset") && pvec(f, a, "offset", &x, &y, &z)) { o->col[i].ox = x; o->col[i].oy = y; }
            break; }
        FIN(); break; }
    case A_Tk2dPlayAnimationV2: { o = GO("gameObject"); const char *c = PS("clipName", NULL);
        if (o && o->an.has && c && !(PB("doNotResetCurrentClip", false) && uo_anim_is(o, c))) uo_anim_play(o, c, true);
        FIN(); break; }
    case A_RandomFloatEither: setf(VAR("storeResult"), rand() % 100 < 50 ? PF("value1", 0) : PF("value2", 1)); FIN(); break;
    case A_RandomlyFlipFloat: { FVar *v = VAR("storeResult"); if (v && (rand() & 1)) setf(v, -v->f); FIN(); break; }
    case A_ReflectAngle: { float n = PF("angle", 0); if (PB("reflectHorizontally", false)) n = 180 - n; if (PB("reflectVertically", false)) n = -n;
        while (n > 360) n -= 360; while (n < -360) n += 360; setf(VAR("storeResult"), n); FIN(); break; }
    case A_KeepFloatPositive: case A_FloatSignTest: case A_GetYDistance: case A_GetMidPoint: case A_TransformPoint: case A_GetColliderRange:
    case A_SetColorRGBA: case A_GameObjectCompareTag: case A_Vector3Lerp: case A_GetAngleBetweenPoints: case A_SetFsmColor: case A_GetRandomObject:
        act_update(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    case A_SendRandomEventV3: {
        JVal *evs = praw(a, "events"), *tr = praw(a, "trackingInts"), *mx = praw(a, "eventMax"), *tm = praw(a, "trackingIntsMissed"), *mm = praw(a, "missedMax");
        int n = evs ? evs->n : 0; FIN(); if (!n) break;
        #define TV(arr, i) (arr ? fsm_var(f, jstr(jget(jat(arr, i), "var"), "")) : NULL)
        for (int loops = 0; loops <= 100; loops++) {
            int i = weighted(f, praw(a, "weights")); if (i < 0) break;
            int forced = -1;
            for (int k = 0; tm && k < tm->n; k++) { FVar *m = TV(tm, k); if (m && m->i >= jv_int(f, jat(mm, k), 9999)) forced = k; }
            if (forced >= 0) {
                for (int k = 0; tr && k < tr->n; k++) { FVar *t = TV(tr, k), *m = TV(tm, k); if (t) t->i = 0; if (m) m->i++; }
                FVar *m = TV(tm, forced), *t = TV(tr, forced); if (m) m->i = 0; if (t) t->i = 1;
                ev(f, jstr(jget(jat(evs, forced), "event"), NULL)); break;
            }
            FVar *ti = TV(tr, i);
            if (!ti || ti->i < jv_int(f, jat(mx, i), 9999)) {
                int v = (ti ? ti->i : 0) + 1;
                for (int k = 0; tr && k < tr->n; k++) { FVar *t = TV(tr, k), *m = TV(tm, k); if (t) t->i = 0; if (m) m->i++; }
                if (ti) ti->i = v; FVar *m = TV(tm, i); if (m) m->i = 0;
                ev(f, jstr(jget(jat(evs, i), "event"), NULL)); break;
            }
            if (loops == 100) ev(f, jstr(jget(jat(evs, 0), "event"), NULL));
        }
        #undef TV
        break; }
    case A_SpawnRandomObjectsOverTimeV2: a->t0 = 0; break;
    case A_SpawnFromPool: {
        UObj *pool = GO("pool"); FIN(); if (!pool) break;
        int n = irand(PI("spawnMin", 1), PI("spawnMax", 1) + 1);
        for (int i = 0; i < n; i++) {
            int cnt = 0; for (UObj *c = pool->child; c; c = c->next) cnt++;
            if (!cnt) break;
            int pick = rand() % cnt; UObj *c = pool->child; while (pick-- && c) c = c->next; if (!c) break;
            uo_set_active(c, true);
            float sp = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180;
            if (c->rb.has) { c->rb.vx = sp * cosf(an); c->rb.vy = sp * sinf(an); }
            float x = uo_wx(c), y = uo_wy(c), px, py, pz; if (!NONE("adjustPosition") && pvec(f, a, "adjustPosition", &px, &py, &pz)) { x += px; y += py; }
            uo_set_parent(c, NULL, false); uo_set_world_pos(c, x, y);
        }
        break; }
    case A_FlingFlashingGeo: { int ti = tmpl_of(f, a, "gameObject"); if (ti >= 0) fling_from(f, a, ti, true); FIN(); break; }
    case A_GetChildNum: { o = GO("gameObject"); UObj *r = NULL; int cnt = 0; if (o) for (UObj *c = o->child; c; c = c->next) cnt++;
        if (cnt) { int k = PI("childIndex", 0) % cnt; r = o->child; while (k-- && r) r = r->next; }
        FVar *v = VAR("store"); if (v) v->go = r; FIN(); break; }
    case A_SetDreamNailConvo: { o = GO("target"); JVal *c = o ? uo_comp_fields(o, "EnemyDreamnailReaction") : NULL; (void)c; FIN(); break; }
    case A_Vector3Add: { FVar *v = VAR("vector3Variable"); float x, y, z; if (v && pvec(f, a, "addVector", &x, &y, &z)) { v->v[0] += x; v->v[1] += y; v->v[2] += z; } if (!PB("everyFrame", false)) FIN(); break; }
    case A_SelectRandomVector3: { JVal *arr = praw(a, "vector3Array"); int i = weighted(f, praw(a, "weights")); FVar *v = VAR("storeVector3");
        if (arr && i >= 0 && v) { JVal *e = jat(arr, i); float x = 0, y = 0, z = 0;
            if (e && e->t == J_ARR) { x = (float)jnum(jat(e, 0), 0); y = (float)jnum(jat(e, 1), 0); z = (float)jnum(jat(e, 2), 0); }
            else if (e && e->t == J_OBJ && jget(e, "var")) { FVar *s2 = fsm_var_j(f, jget(e, "var")); if (s2) { x = s2->v[0]; y = s2->v[1]; z = s2->v[2]; } }
            else if (e && e->t == J_OBJ) { x = (float)jnum(jget(e, "x"), 0); y = (float)jnum(jget(e, "y"), 0); z = (float)jnum(jget(e, "z"), 0); }
            setv(v, x, y, z); }
        FIN(); break; }
    case A_GetPersistentBool: { o = GO("target"); bool val = false; UComp *pc = o ? uo_comp(o, C_PBI) : NULL;
        if (pc) { char k[96]; extern void comps_pbi_key(UObj *, UComp *, char *, int); comps_pbi_key(o, pc, k, sizeof k); val = pd_bool(k); }
        setb(VAR("storeValue"), val); FIN(); break; }
    case A_SendMessageByTag: { const char *tg = PS("tag", ""), *m = PS("message", ""); UObj *t = uo_find_tag(tg);
        MArg z = { 0 }; if (t && m[0]) call_method(f, t, "", m, z, z, NULL); FIN(); break; }
    case A_GetFsmString: { Fsm *t = fsm_of(f, a); FVar *src = t ? fsm_var(t, PS("variableName", "")) : NULL, *dst = VAR("storeValue");
        if (src && dst) snprintf(dst->s, FVAR_STR, "%s", src->s); if (!PB("everyFrame", false)) FIN(); break; }
    case A_TranslateContinuous: break;
    case A_EndGGBossScene: { extern void comps_end_boss_scene(void); comps_end_boss_scene(); FIN(); break; }   // BossSceneController.EndBossScene
    case A_MoveLiftChain: case A_StopLiftChain: {
        o = GO("target"); if (!o) o = f->owner; float r;
        comp2_call(o, "LiftChain", a->type == A_StopLiftChain ? "Stop" : (PB("goUp", false) ? "GoUp" : "GoDown"), 0, &r);
        FIN(); break; }
    case A_EnableFSM: { o = GO("gameObject"); Fsm *t = o ? fsm_find_on(o, PS("fsmName", "")) : NULL; if (t) { bool en = PB("enable", true); if (en && !t->enabled) { t->enabled = true; if (!t->started) fsm_start(t); } else t->enabled = en; } FIN(); break; }
    case A_ShowBossChallengeUI: {      // Hall of Gods: the statue's challenge prompt (native, gg_challenge in dialogue.c)
        UObj *st = f->owner; UComp *bc = NULL; while (st && !(bc = uo_comp_by_cls(st, "BossStatue"))) st = st->parent;
        if (!bc) { FIN(); break; }
        const char *pdk = jstr(jget(bc->f, "statueStatePD"), "");
        // the dream (alt) version when its toggle is on
        if (pd_bool("ggAltStatue") && jstr(jget(bc->f, "dreamStatueStatePD"), NULL)) pdk = jstr(jget(bc->f, "dreamStatueStatePD"), pdk);
        char scene[48] = ""; FILE *tf = fopen("romfs:/text/gg_statues.txt", "rb");
        if (tf) { char ln[128], k[64], sc[48]; while (fgets(ln, sizeof ln, tf)) if (sscanf(ln, "%63s %47s", k, sc) == 2 && !strcmp(k, pdk)) { snprintf(scene, sizeof scene, "%s", sc); break; } fclose(tf); }
        if (!scene[0]) { FIN(); break; }
        StaticVar *sv = svar("bossSceneToLoad", true); if (sv) snprintf(sv->s, sizeof sv->s, "%s", scene);
        UObj *rg = NULL; JVal *rgv = jget(bc->f, "dreamReturnGate"); JVal *oi = rgv && rgv->t == J_OBJ ? jget(rgv, "o") : NULL;
        if (oi && st->treeArr && jint(oi, -1) >= 0 && jint(oi, -1) < st->treeN) rg = st->treeArr[jint(oi, -1)];
        pd_set_str("bossReturnEntryGate", rg ? rg->name : "");
        pd_set_str("dreamReturnScene", uo_room_name());
        extern void gg_challenge_open(Fsm *f, FsmAct *a, const char *nameSheet, const char *nameKey, const char *descSheet, const char *descKey, const char *levelEvent);
        gg_challenge_open(f, a, PS("bossNameSheet", "CP3"), PS("bossNameKey", ""), PS("descriptionSheet", "CP3"), PS("descriptionKey", ""), EV("levelSelectedEvent"));
        break; }
    case A_ShowBossDoorChallengeUI: {  // Godhome pantheon doors (BossSequenceDoor): which tier from romfs/text/gg_doors.txt (door x)
        UObj *d = f->owner; UComp *dc = NULL; while (d && !(dc = uo_comp_by_cls(d, "BossSequenceDoor"))) d = d->parent;
        if (!dc) { FIN(); break; }
        extern int gg_door_tier(UObj *d); int tier = gg_door_tier(d);
        if (!tier) { FIN(); break; }
        UObj *rg = NULL; JVal *rgv = jget(dc->f, "dreamReturnGate"); JVal *oi = rgv && rgv->t == J_OBJ ? jget(rgv, "o") : NULL;
        if (oi && d->treeArr && jint(oi, -1) >= 0 && jint(oi, -1) < d->treeN) rg = d->treeArr[jint(oi, -1)];
        pd_set_str("bossReturnEntryGate", rg ? rg->name : "");
        pd_set_str("dreamReturnScene", uo_room_name());
        char tk[40], dk[40]; snprintf(tk, sizeof tk, "UI_CHALLENGE_TITLE_MAIN_%d", tier); snprintf(dk, sizeof dk, "UI_CHALLENGE_DESC_%d", tier);
        extern void gg_challenge_open(Fsm *f, FsmAct *a, const char *nameSheet, const char *nameKey, const char *descSheet, const char *descKey, const char *levelEvent);
        extern void gg_challenge_set_tier(int tier);
        const char *ce = EV("challengeEvent");
        gg_challenge_open(f, a, "CP3", tk, "CP3", dk, ce && ce[0] ? ce : "DREAM");
        gg_challenge_set_tier(tier);
        break; }
    case A_AudioPlayRandomSingle: { o = GO("gameObject"); uo_play_at(o, aname(praw(a, "audioClip")), 1, 1); FIN(); break; }

    /* --- flow --- */
    case A_Wait: case A_WaitRandom:
        a->t1 = a->type == A_Wait ? PF("time", 1) : frand(PF("timeMin", 0), PF("timeMax", 0));
        a->t0 = 0;
        if (a->t1 <= 0) { FIN(); ev(f, EV("finishEvent")); }
        break;
    case A_NextFrameEvent: break;
    case A_SendEventByName: case A_SendEvent: case A_SendEventByNameV2: {
        float d = PF("delay", 0);
        const char *e = a->type == A_SendEvent ? EV("sendEvent") : PS("sendEvent", NULL);
        if (d < 0.001f) { send_target(f, praw(a, "eventTarget"), e, 0); if (!PB("everyFrame", false)) FIN(); }
        else { send_target(f, praw(a, "eventTarget"), e, d); a->t0 = d; }
        // the dialogue manager answers BOX DOWN DREAM with DONE once its dream box has gone (Godhome doors, dream warps)
        if (e && !strcmp(e, "BOX DOWN DREAM")) fsm_event_delayed(f, "DONE", 1.0f);
        break; }
    case A_SendRandomEvent: {
        JVal *evs = praw(a, "events");
        if (evs && evs->n > 0) {
            int i = weighted(f, praw(a, "weights"));
            if (i >= 0) {
                JVal *e = jat(evs, i); const char *en = e ? jstr(jget(e, "event"), NULL) : NULL;
                float d = PF("delay", 0);
                if (d < 0.001f) { FIN(); ev(f, en); }
                else { fsm_event_delayed_to(f, f, en, d); a->t0 = d; }
                break;
            }
        }
        FIN(); break; }
    case A_SendRandomEventV2: {
        JVal *evs = praw(a, "events"), *tr = praw(a, "trackingInts"), *mx = praw(a, "eventMax");
        int n = evs ? evs->n : 0;
        for (int guard = 0; n > 0 && guard < 64; guard++) {
            int i = weighted(f, praw(a, "weights"));
            if (i < 0) break;
            FVar *ti = tr ? fsm_var(f, jstr(jget(jat(tr, i), "var"), "")) : NULL;
            int cur = ti ? ti->i : 0, max = jv_int(f, jat(mx, i), 9999);
            if (cur < max) {
                int v = cur + 1;
                for (JVal *t = tr ? tr->child : NULL; t; t = t->next) { FVar *x = fsm_var(f, jstr(jget(t, "var"), "")); if (x) x->i = 0; }
                if (ti) ti->i = v;
                FIN(); ev(f, jstr(jget(jat(evs, i), "event"), NULL));
                return;
            }
        }
        FIN(); break; }
    /* --- logic --- */
    case A_BoolTest: case A_BoolAllTrue: case A_BoolNoneTrue: case A_FloatCompare: case A_IntCompare: case A_StringCompare:
    case A_FloatSwitch: case A_FloatInRange: case A_FloatTestToBool: case A_GameObjectIsNull: case A_BoolFlipEveryFrame:
    case A_SetFloatValue: case A_SetIntValue: case A_SetBoolValue: case A_SetStringValue: case A_SetGameObject:
    case A_SetVector2XY: case A_SetVector3XYZ: case A_GetVector3XYZ: case A_FloatAdd: case A_FloatSubtract:
    case A_FloatMultiply: case A_FloatDivide: case A_FloatClamp: case A_FloatOperator: case A_IntAdd: case A_IntOperator:
    case A_DistanceBetweenPoints2D: case A_GetDistance: case A_GetAngleToTarget2D: case A_CheckTargetDirection:
    case A_GetPosition: case A_GetVelocity2d: case A_GetScale: case A_GetRotation: case A_FaceObject:
    case A_CheckAlertRange: case A_CheckAlertRangeByName: case A_CheckCanSeeHero: case A_GetCanSeeHero:
    case A_SetFsmBool: case A_SetFsmInt: case A_SetFsmFloat: case A_SetFsmString: case A_SetFsmGameObject:
    case A_GetFsmBool: case A_GetFsmFloat: case A_ActivateGameObject: case A_Tk2dSpriteSetColor: case A_Tk2dSpriteGetId:
    case A_SetFloatValueV2: case A_FloatAddV2: case A_FloatMultiplyV2: case A_SetIsFixedAngle2d: {
        if (a->type == A_FaceObject) {
            UObj *A = GO("objectA");
            if (!alive(A) || !A->an.has) { FIN(); break; }
            a->f[0] = fabsf(A->sx);
        }
        if (a->type == A_CheckAlertRangeByName) a->o0 = alert_find(f->owner, PS("childName", ""));
        if (a->type == A_CheckCanSeeHero) a->o0 = uo_comp(f->owner, C_LOS) ? f->owner : NULL;
        act_update(f, a, 0);
        bool every = PB("everyFrame", false) || (a->type == A_GetDistance && !praw(a, "everyFrame"));
        if (a->type == A_Tk2dSpriteSetColor || a->type == A_Tk2dSpriteGetId) every = PB("everyframe", false);
        if (a->type == A_FloatAddV2 && PB("fixedUpdate", false)) { /* add happens in fixed */ }
        if (!every) FIN();
        break; }
    case A_BoolFlip: { FVar *v = VAR("boolVariable"); if (v) v->b = !v->b; FIN(); break; }
    case A_RandomFloat: setf(VAR("storeResult"), frand(PF("min", 0), PF("max", 1))); FIN(); break;
    case A_RandomInt: { int lo = PI("min", 0), hi = PI("max", 0); seti(VAR("storeResult"), PB("inclusiveMax", false) ? irand(lo, hi + 1) : irand(lo, hi)); FIN(); break; }
    case A_RandomBool: setb(VAR("storeResult"), rand() % 2); FIN(); break;
    /* --- objects --- */
    case A_GetOwner: setgo(VAR("storeGameObject"), f->owner); FIN(); break;
    case A_GetHero: setgo(VAR("storeResult"), uo_hero()); FIN(); break;
    case A_FindChild: o = GO("gameObject"); setgo(VAR("storeResult"), o ? find_path(o, PS("childName", "")) : NULL); FIN(); break;
    case A_GetChild: o = GO("gameObject"); if (o) setgo(VAR("storeResult"), find_desc(o, PS("childName", NULL), PS("withTag", NULL))); FIN(); break;
    case A_FindGameObject: {
        const char *n = PS("objectName", NULL), *tag = PS("withTag", NULL); UObj *r = NULL;
        if (n && n[0]) r = uo_find(n);
        else if (tag && tag[0] && strcmp(tag, "Untagged")) r = uo_find_tag(tag);
        setgo(VAR("store"), r); FIN(); break; }
    case A_GetParent: o = GO("gameObject"); setgo(VAR("storeResult"), o ? o->parent : NULL); FIN(); break;
    case A_GetChildCount: { o = GO("gameObject"); int n = 0; if (o) for (UObj *c = o->child; c; c = c->next) if (!c->destroyed) n++; seti(VAR("storeResult"), n); FIN(); break; }
    case A_GetRandomChild: { o = GO("gameObject"); if (o) { int n = 0; for (UObj *c = o->child; c; c = c->next) if (!c->destroyed) n++;
            if (n) { int k = rand() % n; for (UObj *c = o->child; c; c = c->next) if (!c->destroyed && k-- == 0) { setgo(VAR("storeResult"), c); break; } } }
        FIN(); break; }
    case A_SetParent: o = GO("gameObject"); if (o) uo_set_parent(o, GO("parent"), PB("resetLocalPosition", false)); FIN(); break;
    case A_GetComponent: setgo(VAR("storeComponent"), GO("gameObject")); FIN(); break;
    case A_FindAlertRange: setgo(VAR("storeResult"), alert_find(GO("target"), PS("childName", ""))); FIN(); break;
    case A_ActivateAllChildren: o = GO("gameObject"); if (o) for (UObj *c = o->child; c; c = c->next) uo_set_active(c, PB("activate", true)); FIN(); break;
    case A_CreateObject: case A_SpawnObjectFromGlobalPool: {
        int ti = tmpl_of(f, a, "gameObject");
        UObj *g = spawn_at(ti, f, a, a->type == A_SpawnObjectFromGlobalPool, NULL);
#ifdef __HOST__
        if (getenv("HK_SPAWNDBG")) fprintf(stderr, "[spawn] %s ti=%d -> %s\n", f->name ? f->name : "?", ti, g ? g->name : "NULL");
#endif
        setgo(VAR("storeObject"), g); FIN(); break; }
    case A_SpawnObjectFromGlobalPoolOverTimeV2: a->t0 = 0; break;
    case A_SpawnRandomObjects: { int ti = tmpl_of(f, a, "gameObject"); if (ti >= 0) fling_from(f, a, ti, false); FIN(); break; }
    case A_FlingObjectsFromGlobalPool: { int ti = tmpl_of(f, a, "gameObject"); if (ti >= 0) fling_from(f, a, ti, true); FIN(); break; }
    case A_FlingObjects: {
        o = GO("containerObject");
        if (o) for (UObj *c = o->child; c; c = c->next) {
            if (!c->rb.has) continue;
            float s = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180;
            c->rb.vx = s * cosf(an); c->rb.vy = s * sinf(an);
            float ax, ay, az;
            if (!NONE("adjustPosition") && pvec(f, a, "adjustPosition", &ax, &ay, &az)) {
                if (PB("randomisePosition", false)) uo_set_world_pos(c, uo_wx(c) + frand(-ax, ax), uo_wy(c) + frand(-ay, ay));
                else uo_set_world_pos(c, uo_wx(c) + ax, uo_wy(c) + ay);
            }
        }
        FIN(); break; }
    case A_FlingObject: {
        o = GO("flungObject"); URb *rb = rbody(o);
        if (rb) { float s = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180; rb->vx = s * cosf(an); rb->vy = s * sinf(an); }
        FIN(); break; }
    case A_DestroyObject: o = GO("gameObject"); if (alive(o)) { float d = PF("delay", 0); if (d <= 0) uo_destroy(o); else uo_destroy_after(o, d); } FIN(); break;
    case A_DestroySelf: case A_RecycleSelf: FIN(); uo_destroy(f->owner); break;
    case A_SetMeshRenderer: o = GO("gameObject"); if (o) o->mr = PB("active", false); FIN(); break;
    case A_SetCollider: case A_SetPolygonCollider: {
        o = GO("gameObject");
        if (o) for (int i = 0; i < o->ncol; i++) {
            bool match = a->type == A_SetCollider ? o->col[i].t == COL_BOX : o->col[i].t == COL_POLY;
            if (match) { o->col[i].en = PB("active", false); break; }
        }
        FIN(); break; }
    /* --- transform --- */
    case A_SetPosition: if (!PB("everyFrame", false) && !PB("lateUpdate", false)) { do_set_position(f, a); FIN(); } break;
    case A_SetRotation: if (!PB("everyFrame", false) && !PB("lateUpdate", false)) { do_set_rotation(f, a); FIN(); } break;
    case A_SetScale: do_set_scale(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_FlipScale: do_flip(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetRandomRotation: o = GO("gameObject"); if (o && PB("z", true)) o->rz = (float)(rand() % 360); FIN(); break;
    case A_Translate: if (!PB("everyFrame", true) && !PB("lateUpdate", false) && !PB("fixedUpdate", false)) { do_translate(f, a, 1.0f / 60); FIN(); } break;
    case A_TranslateV2:
        if (PB("alwaysOnStart", false)) do_translate(f, a, 1.0f / 60);
        if (!PB("everyFrame", true) && !PB("lateUpdate", false) && !PB("fixedUpdate", false)) { do_translate(f, a, 1.0f / 60); FIN(); }
        break;
    case A_RandomWait:
        a->t1 = frand(PF("min", 0), PF("max", 1)); a->t0 = 0;
        if (a->t1 <= 0) { FIN(); ev(f, EV("finishEvent")); }
        break;
    case A_SetBattleScene: o = GO("target"); if (o) hm_set_battle_scene(o, GO("battleScene")); FIN(); break;
    case A_GetGrandparent: o = GO("gameObject"); setgo(VAR("storeResult"), o && o->parent ? o->parent->parent : NULL); FIN(); break;
    case A_SetSpecialDeath: o = GO("target"); if (o && !NONE("hasSpecialDeath")) hm_set_special(o, PB("hasSpecialDeath", false)); FIN(); break;
    case A_SetDamageOverride: o = GO("target"); if (o) hm_set_damage_override(o, PB("damageOverride", false)); FIN(); break;
    case A_FloatDivideV2: { FVar *v = VAR("floatVariable"); float d = PF("divideBy", 1); if (v && d != 0) var_set_float(v, v->f / d); if (!PB("everyFrame", false)) FIN(); break; }
    case A_CheckGeoCap:     // the PC game has no cap on loose geo (only low graphics tiers do)
        setb(VAR("StoreIsUnderCap"), true); setb(VAR("StoreIsAtCap"), false); ev(f, EV("IsUnderCapEvent"));
        if (!PB("EveryFrame", false)) FIN();
        break;
    case A_AnyKey: break;
    case A_Vector3Compare: act_update(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    case A_SendEventToRegisterV2: { const char *e = PS("EventName", NULL); FIN(); if (e && e[0]) { extern void game_global_event(const char *); game_global_event(e); comps_event_register_send(e); } break; }
    case A_LerpTk2dSpriteColor:
        o = GO("Target"); a->o0 = o; if (!o) { FIN(); break; }
        a->f[0] = o->tintR; a->f[1] = o->tintG; a->f[2] = o->tintB; a->f[3] = o->tintA; a->t0 = 0;
        act_update(f, a, 0); break;
    case A_LineCast2d: a->i0 = 0; act_update(f, a, 0); if (PI("repeatInterval", 1) == 0) FIN(); break;
    case A_GetNextChild: {      // walks the children one per entry, then sends finishedEvent and starts over
        o = GO("gameObject"); FIN(); if (!o) break;
        if (a->o0 != o) { a->o0 = o; a->i1 = 0; }
        int k = 0; UObj *c = o->child; while (c && k < a->i1) { if (!c->destroyed) k++; c = c->next; }
        while (c && c->destroyed) c = c->next;
        if (!c) { a->i1 = 0; ev(f, EV("finishedEvent")); break; }
        setgo(VAR("storeNextChild"), c); a->i1++;
        ev(f, EV("loopEvent")); break; }
    case A_FinishFSM: FIN(); fsm_stop(f); break;
    case A_SetSpawnJarContents: { o = GO("storedObject"); extern void comps_spawnjar_set(UObj *, int, int);
        if (o) comps_spawnjar_set(o, tmpl_of(f, a, "enemyPrefab"), PI("enemyHealth", 1)); FIN(); break; }
    case A_Tk2dSpriteSetId: {   // a sprite (no animator) showing another frame of its collection: Bone Gate tutorial signs etc.
        o = GO("gameObject"); FIN(); if (!o) break;
        const char *nm = PS("ORSpriteName", ""); extern bool uo_sprite_set(UObj *o, int id, const char *name);
        uo_sprite_set(o, PI("spriteID", 0), nm); break; }
    case A_ObjectJitter: o = GO("gameObject"); if (o) { a->f[0] = uo_wx(o); a->f[1] = uo_wy(o); } break;
    case A_FaceDirection:
        o = GO("gameObject"); if (!o) { FIN(); break; }
        a->f[0] = fabsf(o->sx);
        do_face_direction(f, a, 0);
        if (!PB("everyFrame", false)) FIN();
        break;
    /* --- physics --- */
    case A_SetVelocity2d: do_set_velocity(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetVelocityAsAngle: do_velocity_angle(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_AddForce2dV2: do_add_force(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetGravity2dScale: {
        o = GO("gameObject");
        // the Knight's Rigidbody2D is the hero physics' gravity: the spell scripts zero it while casting and put it back
        // after (with only the stand-in's copy changed, he hovered after an air cast until a nail hit reset it)
        if (o && o == uo_hero()) { extern void hero_set_gravity_scale(float); hero_set_gravity_scale(PF("gravityScale", 1)); }
        URb *rb = rbody(o); if (rb) rb->g = PF("gravityScale", 1); FIN(); break; }
    case A_Decelerate: do_decel(f, a, false); break;
    case A_DecelerateV2: do_decel(f, a, true); break;
    case A_IdleBuzz: o = GO("gameObject"); if (o) { a->f[0] = uo_wx(o); a->f[1] = uo_wy(o); } do_idle_buzz(f, a); break;
    case A_DistanceFly: do_distance_fly(f, a); break;
    case A_ChaseObject: do_chase(f, a); break;
    case A_FireAtTarget: do_fire_at(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_AddTorque2d: FIN(); break;
    case A_RayCast2d: a->i0 = 0; act_update(f, a, 0); if (PI("repeatInterval", 1) == 0) FIN(); break;
    case A_CheckCollisionSide: case A_CheckCollisionSideEnter: case A_Collision2dEvent:
    case A_Trigger2dEvent: case A_Trigger2dEventLayer: case A_SendTrigger2DEventByName: case A_SendTrigger2DEvent: break;   // proxy delegates
    case A_iTweenFadeTo: case A_iTweenFadeFrom: {
        o = GO("gameObject"); a->o0 = o; if (!o) { FIN(); break; }
        a->f[0] = o->tintA; a->f[1] = PF("alpha", 0); a->t0 = -(NONE("delay") ? 0 : PF("delay", 0));
        if (a->type == A_iTweenFadeFrom) { float t = a->f[0]; a->f[0] = a->f[1]; a->f[1] = t; }
        break; }
    case A_GetFsmGameObject: { Fsm *t = fsm_of(f, a); FVar *v = t ? fsm_var(t, PS("variableName", "")) : NULL; setgo(VAR("storeValue"), v ? v->go : NULL); if (!PB("everyFrame", false)) FIN(); break; }
    case A_FlingObjectsFromGlobalPoolVel: {
        int ti = tmpl_of(f, a, "gameObject"); if (ti < 0) { FIN(); break; }
        UObj *sp = GO("spawnPoint"); float x = 0, y = 0, px, py, pz;
        if (alive(sp)) { x = uo_wx(sp); y = uo_wy(sp); if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x += px; y += py; } }
        else if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x = px; y = py; }
        int n = irand(PI("spawnMin", 0), PI("spawnMax", 0) + 1);
        for (int i = 0; i < n; i++) {
            UObj *g = uo_spawn(ti, x, y, 0); if (!g) break;
            float gx = x, gy = y;
            if (praw(a, "originVariationX")) gx += frand(-PF("originVariationX", 0), PF("originVariationX", 0));
            if (praw(a, "originVariationY")) gy += frand(-PF("originVariationY", 0), PF("originVariationY", 0));
            uo_set_world_pos(g, gx, gy);
            if (g->rb.has) { g->rb.vx = frand(PF("speedMinX", 0), PF("speedMaxX", 0)); g->rb.vy = frand(PF("speedMinY", 0), PF("speedMaxY", 0)); }
        }
        FIN(); break; }
    case A_GetCollision2dInfo: FIN(); break;
    /* --- animation --- */
    case A_Tk2dPlayAnimation: o = GO("gameObject"); if (o && o->an.has) uo_anim_play(o, PS("clipName", NULL), false); FIN(); break;
    case A_Tk2dPlayAnimationWithEvents: o = GO("gameObject");
        if (o && o->an.has) { uo_anim_play(o, PS("clipName", NULL), false); tk_delegates(f, a, o); }
        if (!EV("animationTriggerEvent") && !EV("animationCompleteEvent")) FIN();   // nothing to wait for
        break;
    case A_Tk2dWatchAnimationEvents: o = GO("gameObject"); a->o0 = o; if (o && (o->an.has || o == uo_hero())) tk_delegates(f, a, o); break;
    case A_Tk2dPlayFrame: o = GO("gameObject"); if (o && o->an.has) uo_anim_play_frame(o, PI("frame", 0)); FIN(); break;
    /* --- walker --- */
    case A_StopWalker: case A_StartWalker: case A_SetWalkerFacing: {
        o = GO("target"); a->o0 = (o && uo_comp(o, C_WALKER)) ? o : NULL;
        if (a->o0) act_update(f, a, 0);
        if (!PB("everyFrame", false)) FIN();
        break; }
    case A_WalkLeftRight: {
        o = GO("gameObject"); a->o0 = o; if (!o) break;
        bool fl = PB("spriteFacesLeft", false), sl = PB("startLeft", false), sr = PB("startRight", false);
        a->b0 = false;
        if (o->sx < 0) { if (!fl && sr) a->b0 = true; if (fl && sl) a->b0 = true; }
        else { if (fl && sr) a->b0 = true; if (!fl && sl) a->b0 = true; }
        if (!sl && !sr && !PB("keepDirection", false) && frand(0, 100) <= 50) a->b0 = true;
        setb(VAR("startLeft"), false); setb(VAR("startRight"), false);
        a->i0 = 0;
        if (o->an.has) uo_anim_play(o, PS("walkAnimName", NULL), false);
        break; }
    /* --- health --- */
    case A_SetInvincible: o = GO("target");
        if (o && uo_comp(o, C_HEALTH)) { if (!NONE("Invincible")) hm_set_invincible(o, PB("Invincible", false)); if (!NONE("InvincibleFromDirection")) hm_set_invincible_dir(o, PI("InvincibleFromDirection", 0)); }
        FIN(); break;
    case A_SetHP: o = GO("target"); if (o && !NONE("hp")) hm_set_hp(o, PI("hp", 0)); FIN(); break;
    case A_InstaDeath: o = GO("target"); if (o && uo_comp(o, C_HEALTH) && !hm_is_dead(o)) hm_die(o, PF("direction", 0) >= 0 ? 0.0f : 180.0f, ATK_GENERIC); FIN(); break;
    case A_SetDamageHeroAmount: o = GO("target"); if (o && !NONE("damageDealt")) damagehero_set(o, PI("damageDealt", 1)); FIN(); break;
    case A_SetGeoDrop: o = GO("target"); if (o) hm_set_geo(o, PI("smallGeo", 0), PI("mediumGeo", 0), PI("largeGeo", 0)); FIN(); break;
    /* --- player data / game --- */
    case A_PlayerDataBoolTest: { bool v = pd_bool(PS("boolName", NULL)); FIN(); ev(f, v ? EV("isTrue") : EV("isFalse")); break; }
    case A_SetPlayerDataBool: pd_set_bool(PS("boolName", NULL), PB("value", false)); FIN(); break;
    case A_GetPlayerDataInt: seti(VAR("storeValue"), pd_int(PS("intName", NULL))); FIN(); break;
    case A_SetPlayerDataInt: pd_set_int(PS("intName", NULL), PI("value", 0)); FIN(); break;
    case A_WaitForFinishedEnteringScene: FIN(); ev(f, EV("sendEvent")); break;
    case A_CheckSceneName: { const char *n = PS("sceneName", ""); bool eq = !strcmp(n, uo_room_name()); FIN(); ev(f, eq ? EV("equalEvent") : EV("notEqualEvent")); break; }
    case A_GGCheckIsBossRushMode: FIN(); ev(f, EV("falseEvent")); break;
    case A_GGCheckIfBossScene: FIN(); ev(f, !strncmp(uo_room_name(), "GG_", 3) ? EV("bossSceneEvent") : EV("regularSceneEvent")); break;   // Godhome arenas are boss scenes
    case A_GGCheckIfFirstBossScene: { extern bool gg_pantheon_first_scene(void); FIN(); ev(f, gg_pantheon_first_scene() ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_CheckGGBossLevel: FIN(); ev(f, !strncmp(uo_room_name(), "GG_", 3) ? EV("level1") : EV("notGG")); break;          // Attuned
    case A_GGWaitForBossSceneTransitionIn: a->t0 = 0.5f; break;
    case A_CheckIsChineseBuild: FIN(); ev(f, EV("falseEvent")); break;
    case A_SendMessage: {
        JVal *fc = praw(a, "functionCall"); const char *fn = fc ? jstr(jget(fc, "function"), "") : "";
        o = GO("gameObject");
        MArg a0 = marg_fc(f, fc), none = { 0 };
        if (!strcmp(fn, "FreezeMoment")) game_freeze(0.05f);
        else if (!strcmp(fn, "flashInfected") || !strcmp(fn, "flashInfectedLong") || !strcmp(fn, "flashWhiteLong") || !strcmp(fn, "flashFocusHeal") ||
                 !strcmp(fn, "flashHealBlue") || !strcmp(fn, "flashSporeQuick") || !strcmp(fn, "flashDungQuick") || !strcmp(fn, "FlashingSuperDash")) { if (o) o->flash = 0.12f; }
        else if (o && o == uo_hero()) call_method(f, o, "HeroController", fn, a0, none, NULL);
        else if (o && o == uo_global("GameManager")) call_method(f, o, "GameManager", fn, a0, none, NULL);
        FIN(); break; }
    case A_CallMethodProper: {
        const char *b = PS("behaviour", ""), *m = PS("methodName", "");
        JVal *pars = praw(a, "parameters");
        JVal *sr = praw(a, "storeResult"); JVal *vn = sr && sr->t == J_OBJ ? jget(sr, "var") : NULL;
        FVar *res = vn ? fsm_var_j(f, vn) : NULL;
        if (!strcmp(b, "DialogueBox")) {
            UObj *go = GO("gameObject");
            int yn = go && go == uo_global("DialogueTextYN");
            dlg_call(yn, m, param_str(f, jat(pars, 0)), param_str(f, jat(pars, 1)));
        } else if (!strcmp(b, "CharmIconList") && !strcmp(m, "GetSprite")) {   // charm icon as a sprite handle (icons.spr)
            int id = param_int(f, jat(pars, 0));
            char nm[40]; snprintf(nm, sizeof nm, "charm_%d", id);
            const SpriteSet *ic = game_icons();
            if (res && ic) { res->b = true; res->i = spr_named(ic, nm); res->f = 1; }
        } else call_method(f, GO("gameObject"), b, m, marg_cp(f, jat(pars, 0)), marg_cp(f, jat(pars, 1)), res);
        FIN(); break; }
    /* --- audio --- */
    case A_AudioPlayerOneShotSingle: case A_AudioPlayerOneShot:
        a->t0 = 0;
        if (PF("delay", 0) == 0) { act_update(f, a, -1); FIN(); }
        break;
    case A_AudioPlaySimple: play_clip(aname(praw(a, "oneShotClip")), NONE("volume") ? 1 : PF("volume", 1), 1, 1); FIN(); break;
    case A_AudioPlay: case A_AudioPlayV2: {
        const char *c = aname(praw(a, "oneShotClip"));
        UObj *src = GO("gameObject");
        if (!c && src && src->as.has) {          // AudioSource.Play: the source's own clip
            c = src->as.clip;
            if (!NONE("volume")) src->as.vol = PF("volume", 1);
            uo_source_play(src);          // a looping source keeps playing (uo_update_loops) until AudioStop
            a->t0 = src->as.loop ? 1e9f : sfx_length(c);
            break;
        }
        if (!c) { FIN(); break; }
        uo_play_at(src, c, NONE("volume") ? 1 : PF("volume", 1), 1);
        a->t0 = sfx_length(c);
        if (!EV("finishedEvent")) { /* keep running until the clip ends, like the original */ }
        break; }
    case A_AudioPlayRandom: {
        JVal *cl = praw(a, "audioClips"); int i = weighted(f, praw(a, "weights"));
        if (cl && i >= 0) play_clip(aname(jat(cl, i)), 1, PF("pitchMin", 1), PF("pitchMax", 1));
        FIN(); break; }
    case A_AudioStop: uo_source_stop(GO("gameObject")); FIN(); break;
    case A_SetAudioClip: { UObj *src = GO("gameObject"); const char *c = aname(praw(a, "audioClip")); if (src && c) { src->as.has = true; src->as.clip = c; if (src->as.vol <= 0) src->as.vol = 1; if (src->as.pitch <= 0) src->as.pitch = 1; } FIN(); break; }
    case A_SetAudioPitch: { UObj *src = GO("gameObject"); if (src && !NONE("pitch")) src->as.pitch = PF("pitch", 1); FIN(); break; }
    /* --- tweens --- */
    case A_iTweenMoveBy: case A_iTweenScaleBy: {
        o = GO("gameObject"); if (!o) { FIN(); break; }
        float vx = 0, vy = 0, vz = 0; pvec(f, a, "vector", &vx, &vy, &vz);
        if (a->type == A_iTweenMoveBy) { a->f[0] = uo_wx(o); a->f[1] = uo_wy(o); a->f[2] = vx; a->f[3] = vy; }
        else { a->f[0] = o->sx; a->f[1] = o->sy; a->f[2] = vx; a->f[3] = vy; }
        a->t0 = -(NONE("delay") ? 0 : PF("delay", 0)); a->b1 = false;
        break; }
    case A_EaseFloat: case A_EaseColor:
        a->t0 = -(NONE("delay") ? 0 : PF("delay", 0));
        if (a->type == A_EaseColor) {
            float c0[4] = { 1, 1, 1, 1 }, c1[4] = { 1, 1, 1, 1 };
            pcolor(f, a, "fromValue", c0); pcolor(f, a, "toValue", c1);
            a->f[0] = c0[0]; a->f[1] = c0[1]; a->f[2] = c0[2]; a->f[3] = c0[3]; a->f[4] = c1[0]; a->f[5] = c1[1]; a->t1 = c1[2]; a->t2 = c1[3];
            store_color(VAR("colorVariable"), PB("reverse", false) ? c1 : c0);
            break;
        }
        a->f[0] = PF("fromValue", 0); a->f[1] = PF("toValue", 0);
        if (a->type == A_EaseFloat) setf(VAR("floatVariable"), PB("reverse", false) ? a->f[1] : a->f[0]);
        break;
    case A_SpawnBlood: FIN(); break;
    case A_SpawnBloodTime: a->t0 = 0; break;
    /* --- batch 2 --- */
    case A_FaceAngle: act_fixed(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetRecoilSpeed: o = GO("target"); if (o) recoil_set_speed(o, PF("newRecoilSpeed", 0)); FIN(); break;
    case A_SetRecoilFreeze: o = GO("target"); if (o) recoil_set_freeze(o, PB("freeze", false)); FIN(); break;
    case A_RayCast2dV2: a->i0 = 0; act_update(f, a, 0); if (PI("repeatInterval", 1) == 0) FIN(); break;
    case A_ConstrainMovement: o = GO("gameObject"); if (o) { a->f[0] = uo_wx(o); a->f[1] = uo_wy(o); } break;
    case A_CheckSendEventLimit: if (alive(GO("gameObject"))) send_target(f, praw(a, "target"), EV("trueEvent"), 0); FIN(); break;
    case A_TakeDamage: {
        UObj *t = GO("Target");
        if (t) {
            float mult = NONE("Multiplier") ? 1 : PF("Multiplier", 1);
            comps_hit_taker(t, (int)(PI("DamageDealt", 0) * mult + 0.5f), PF("Direction", 0), PI("AttackType", 1), NONE("MagnitudeMultiplier") ? 1 : PF("MagnitudeMultiplier", 1), f->owner);
        }
        FIN(); break; }
    case A_AccelerateVelocity: break;
    case A_LookAt2dGameObjectSmooth: act_update(f, a, 0); break;
    case A_BoundsBoxCollider: case A_GetVelocityAsAngle: case A_BoolTestMulti: case A_GetXDistance: case A_GameObjectIsVisible:
    case A_Tk2dSpriteSetScale: case A_GetVector2XY: case A_BoolAnyTrue: case A_IntSwitch: case A_SetVector3Value:
    case A_Vector3Multiply: case A_Vector3AddXYZ: case A_ConvertIntToFloat: case A_ConvertFloatToInt: case A_BoxColliderOffset:
    case A_GetFsmInt: case A_GetSpeed2d: case A_GetLayer: case A_GetTag: case A_GetName: {
        act_update(f, a, 0);
        bool every = a->type == A_GetXDistance ? (!praw(a, "everyFrame") || PB("everyFrame", true)) : a->type == A_Tk2dSpriteSetScale ? PB("everyframe", false) : PB("everyFrame", false);
        if (!every) FIN();
        break; }
    case A_PreventInvincibleEffect: FIN(); break;
    case A_ReceivedDamage: case A_Collision2dEventLayer: break;
    case A_SpawnRandomObjectsV2: { int ti = tmpl_of(f, a, "gameObject"); if (ti >= 0) fling_from(f, a, ti, true); FIN(); break; }
    case A_FlingObjectsFromGlobalPoolTime: a->t0 = 0; break;
    case A_ProjectileSquash: act_fixed(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetPositionToObject: {
        o = GO("gameObject"); UObj *t = GO("targetObject");
        if (o && alive(t)) { float x = uo_wx(t), y = uo_wy(t); if (!NONE("xOffset")) x += PF("xOffset", 0); if (!NONE("yOffset")) y += PF("yOffset", 0); uo_set_world_pos(o, x, y); }
        FIN(); break; }
    case A_RandomFloatV2: setf(VAR("storeResult"), frand(PF("min", 0), PF("max", 1))); if (!PB("everyFrame", false)) FIN(); break;
    case A_IntAddV2: { FVar *v = VAR("intVariable"); if (v) v->i += PI("add", 0); if (!PB("everyFrame", false)) FIN(); break; }
    case A_AddForce2d: do_add_force(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetIsKinematic2d: { URb *rb = rbody(GO("gameObject")); if (rb) { rb->kin = PB("isKinematic", false); rb->asleep = false; rb->restT = 0; } FIN(); break; }
    case A_SetCircleCollider: o = GO("gameObject"); if (o) for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_CIRCLE) { o->col[i].en = PB("active", false); break; } FIN(); break;
    case A_CompareNames: {
        const char *n = PS("name", ""); bool hit = false;
        JVal *arr = praw(a, "strings");
        if (arr && arr->t == J_OBJ && jget(arr, "var")) { FVar *v = fsm_var_j(f, jget(arr, "var")); JVal *ss = v && v->obj ? jget(v->obj, "strings") : NULL;
            for (JVal *e = ss ? ss->child : NULL; e; e = e->next) if (e->t == J_STR && n && !strcmp(e->s, n)) hit = true; }
        else for (JVal *e = arr && arr->t == J_ARR ? arr->child : NULL; e; e = e->next) { const char *x = jv_str(f, e, ""); if (n && x && !strcmp(x, n)) hit = true; }
        send_target(f, praw(a, "target"), hit ? EV("trueEvent") : EV("falseEvent"), 0);
        FIN(); break; }
    case A_DetachChildren: o = GO("gameObject"); if (o) while (o->child) uo_set_parent(o->child, NULL, false); FIN(); break;
    case A_iTweenRotateBy: {     // iTween counts the amount in turns (x 360): Xero's nails spin
        o = GO("gameObject"); if (!o) { FIN(); break; }
        float vx = 0, vy = 0, vz = 0; pvec(f, a, "vector", &vx, &vy, &vz);
        a->f[0] = o->rz; a->f[2] = o->rz + 360 * vz;
        a->t0 = -(NONE("delay") ? 0 : PF("delay", 0));
        break; }
    case A_GameObjectHasChildren: {
        o = GO("gameObject"); if (o) { bool has = false; for (UObj *c = o->child; c; c = c->next) if (!c->destroyed) { has = true; break; }
            setb(VAR("storeResult"), has); ev(f, EV(has ? "trueEvent" : "falseEvent")); }
        if (!PB("everyFrame", false)) FIN();
        break; }
    case A_GameObjectCompare: {
        bool eq = GO("gameObjectVariable") == GO("compareTo"); setb(VAR("storeResult"), eq);
        ev(f, EV(eq ? "equalEvent" : "notEqualEvent")); if (!PB("everyFrame", false)) FIN();
        break; }
    case A_SwitchOnPlatform: FIN(); ev(f, EV("Standalone")); break;    // the PC version
    case A_iTweenScaleTo: case A_iTweenRotateTo: {
        o = GO("gameObject"); if (!o) { FIN(); break; }
        float vx = 0, vy = 0, vz = 0;
        pvec(f, a, a->type == A_iTweenScaleTo ? "vectorScale" : "vectorRotation", &vx, &vy, &vz);
        if (a->type == A_iTweenScaleTo) { a->f[0] = o->sx; a->f[1] = o->sy; a->f[2] = vx; a->f[3] = vy; }
        else { a->f[0] = o->rz; a->f[2] = vz; float d = fmodf(a->f[2] - a->f[0], 360); if (d > 180) d -= 360; if (d < -180) d += 360; a->f[2] = a->f[0] + d; }
        a->t0 = -(NONE("delay") ? 0 : PF("delay", 0));
        break; }
    case A_StringSwitch: {
        const char *v = PS("stringVariable", ""); JVal *cmp = praw(a, "compareTo"), *se = praw(a, "sendEvent"); int i = 0;
        for (JVal *e = cmp ? cmp->child : NULL; e; e = e->next, i++) { const char *x = jv_str(f, e, ""); if (v && x && !strcmp(v, x)) { JVal *q = jat(se, i); ev(f, q ? jstr(jget(q, "event"), NULL) : NULL); break; } }
        if (!PB("everyFrame", false)) FIN();
        break; }
    case A_SetTag: o = GO("gameObject"); if (o) { const char *t = PS("tag", "Untagged"); strncpy(o->tag, t ? t : "", 23); } FIN(); break;
    case A_IncrementPlayerDataInt: pd_set_int(PS("intName", NULL), pd_int(PS("intName", NULL)) + 1); FIN(); break;
    case A_Rotate: if (!PB("everyFrame", false) && !PB("lateUpdate", false) && !PB("fixedUpdate", false)) { act_late(f, a, 0); FIN(); } break;
    case A_GotoPreviousState: FIN(); if (f->prev >= 0) { f->pending = f->prev; } break;
    case A_BuildString: {
        char buf[FVAR_STR] = ""; JVal *parts = praw(a, "stringParts"); const char *sep = PS("separator", "");
        for (JVal *e = parts ? parts->child : NULL; e; e = e->next) { const char *x = jv_str(f, e, ""); strncat(buf, x ? x : "", FVAR_STR - 1 - strlen(buf)); if ((e->next || PB("addToEnd", false)) && sep) strncat(buf, sep, FVAR_STR - 1 - strlen(buf)); }
        FVar *v = VAR("storeResult"); if (v) strcpy(v->s, buf);
        if (!PB("everyFrame", false)) FIN();
        break; }
    case A_SelectRandomString: {
        JVal *ss = praw(a, "strings"); int i = weighted(f, praw(a, "weights")); FVar *v = VAR("storeString");
        if (v && ss && i >= 0) { const char *x = jv_str(f, jat(ss, i), ""); strncpy(v->s, x ? x : "", FVAR_STR - 1); }
        FIN(); break; }
    case A_SetAngularVelocity2d: FIN(); break;
    case A_PlayerDataBoolAllTrue: {
        JVal *ss = praw(a, "stringVariables"); bool all = true;
        for (JVal *e = ss ? ss->child : NULL; e; e = e->next) if (!pd_bool(jv_str(f, e, ""))) all = false;
        setb(VAR("storeResult"), all); FIN(); ev(f, all ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_GetPlayerDataString: { FVar *v = VAR("storeValue"); if (v) snprintf(v->s, sizeof v->s, "%s", pd_str(PS("stringName", NULL))); FIN(); break; }
    /* --- NPCs / UI (batch 3) --- */
    case A_ShowPromptMarker: {
        UObj *sp = GO("spawnPoint"); FVar *st = VAR("storeObject");
        if (sp) { UObj *m = prompt_show(f->owner, st ? st->go : NULL, PS("labelName", "Listen"), uo_wx(sp), uo_wy(sp)); setgo(st, m); }
        FIN(); break; }
    case A_HidePromptMarker: { FVar *st = VAR("storedObject"); if (st && st->go) { prompt_hide(st->go); st->go = NULL; } FIN(); break; }
    case A_ListenForUp: case A_ListenForDown: case A_ListenForLeft: case A_ListenForRight: case A_ListenForJump:
    case A_ListenForAttack: case A_ListenForCast: case A_ListenForDash: case A_ListenForMenuSubmit: case A_ListenForMenuCancel:
    case A_ListenForDreamNail: case A_ListenForQuickMap: case A_ListenForInventory: case A_ListenForSuperdash:
        act_update(f, a, 0);
        if (PB("stateEntryOnly", false)) FIN();
        break;
    case A_ListenForPromptContinue: case A_GetButtonDown: break;
    case A_AddTrackTrigger: FIN(); break;     // overlaps are tracked for every trigger object (uo triggers_step)
    case A_CheckTrackTriggerCount: {
        o = GO("target");
        if (o && track_test(f, a, o)) { FIN(); ev(f, EV("successEvent")); break; }
        if (!PB("everyFrame", true)) FIN();
        break; }
    case A_PlayerDataBoolTrueAndFalse: { bool t = pd_bool(PS("trueBool", NULL)) && !pd_bool(PS("falseBool", NULL)); FIN(); ev(f, t ? EV("isTrue") : EV("isFalse")); break; }
    case A_GetPlayerDataBool: setb(VAR("storeValue"), pd_bool(PS("boolName", NULL))); FIN(); break;
    case A_GetPlayerDataFloat: setf(VAR("storeValue"), (float)pd_int(PS("floatName", NULL))); FIN(); break;
    case A_SetPlayerDataFloat: pd_set_int(PS("floatName", NULL), (int)PF("value", 0)); FIN(); break;
    case A_SetPlayerDataString: pd_set_str(PS("stringName", NULL), PS("value", "")); FIN(); break;
    case A_PlayerDataIntAdd: { const char *n = PS("intName", NULL); pd_set_int(n, pd_int(n) + PI("amount", 0)); FIN(); break; }
    case A_ForceHeroFootstepSound: P.forceFootstep = PI("forceFootstepSound", 0); FIN(); break;
    case A_IntCompareToBool: act_update(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    case A_SendEventToRegister: { const char *e = PS("eventName", NULL); FIN(); if (e && e[0]) { extern void game_global_event(const char *); game_global_event(e); comps_event_register_send(e); } break; }
    case A_GetLanguageString: case A_GetLanguageStringProcessed: {
        FVar *v = VAR("storeValue");
        const char *t = lang_get_or_key(PS("convName", ""), PS("sheetName", ""));
        if (v) { snprintf(v->s, sizeof v->s, "%s", t); for (char *c = v->s; *c; c++) if (*c == '\f') *c = ' '; }
        FIN(); break; }
    case A_ConvertIntToString: { FVar *v = VAR("stringVariable"); FVar *iv = VAR("intVariable"); if (v) snprintf(v->s, sizeof v->s, "%d", iv ? iv->i : PI("intVariable", 0)); if (!PB("everyFrame", false)) FIN(); break; }
    case A_SetSpriteRenderer: o = GO("gameObject"); if (o) { if (o->sr.has) o->sr.en = PB("active", true); else o->mr = PB("active", true); } FIN(); break;
    case A_SetSpriteRendererSprite: {     // a Sprite asset ({"sprite": guid}) or a variable holding one
        o = GO("gameObject"); JVal *sp = praw(a, "sprite");
        if (o && sp && sp->t == J_OBJ) {
            JVal *g = jget(sp, "sprite"), *vn = jget(sp, "var");
            if (g && g->t == J_STR) { o->sr.has = true; o->sr.set = 0; o->sr.frame = uo_sprite_frame(g->s); if (!o->sr.a) o->sr.r = o->sr.g = o->sr.b = o->sr.a = 1; }
            else if (vn) { FVar *v = fsm_var(f, vn->s); if (v && v->b) { o->sr.has = true; o->sr.frame = v->i; o->sr.set = (int)v->f; if (!o->sr.a) o->sr.r = o->sr.g = o->sr.b = o->sr.a = 1; } }
        }
        FIN(); break; }
    case A_SetTextMeshProText: case A_SetTextMeshText: { o = GO("gameObject"); if (o) uo_set_text(o, PS("textString", "")); FIN(); break; }
    case A_SetTextMeshProGameText: { o = GO("gameObject"); if (o) uo_set_text(o, lang_get_or_key(PS("convName", ""), PS("sheetName", ""))); FIN(); break; }
    case A_GetMeshRendererBounds: { o = GO("gameObject"); float w = 0;
        if (o && o->tmp.has && o->tmp.text) { const Font *ft = o->tmp.font ? &A.fontMenu : &A.fontText; w = font_width(ft, o->tmp.text) * o->tmp.size / 7.2f / 14.2f; }
        setf(VAR("width"), w); setf(VAR("height"), 0.6f); FIN(); break; }
    case A_CreateUIMsgGetItem: {          // the big "item get" message (UI Msg Get Item prefab)
        UObj *m = msg_create_getitem(VAR("sprite"));
        setgo(VAR("storeObject"), m); FIN(); break; }
    case A_GetPosition2D: { o = GO("gameObject"); if (o) { setf(VAR("x"), uo_wx(o)); setf(VAR("y"), uo_wy(o)); FVar *v = VAR("vector"); setv(v, uo_wx(o), uo_wy(o), 0); } if (!PB("everyFrame", false)) FIN(); break; }
    case A_Tk2dPlayAnimationDelay: FIN(); break;
    case A_ConvertStringToInt: { FVar *sv = VAR("stringVariable"); seti(VAR("intVariable"), sv ? atoi(sv->s) : atoi(PS("stringVariable", "0"))); if (!PB("everyFrame", false)) FIN(); break; }
    case A_IntTestToBool: { int x = PI("int1", 0), y = PI("int2", 0);
        if (!NONE("equalBool")) setb(VAR("equalBool"), x == y);
        if (!NONE("lessThanBool")) setb(VAR("lessThanBool"), x < y);
        if (!NONE("greaterThanBool")) setb(VAR("greaterThanBool"), x > y);
        if (!PB("everyFrame", false)) FIN(); break; }
    case A_FadeGroupUp: o = GO("target"); if (o) fadegroup_set(o, true, false); FIN(); break;
    case A_FadeGroupDown: o = GO("target"); if (o) fadegroup_set(o, false, PB("fast", false)); FIN(); break;
    case A_SetFloatToHighest: { float x = PF("value1", 0), y = PF("value2", 0); setf(VAR("floatVariable"), x > y ? x : y); if (!PB("everyFrame", false)) FIN(); break; }
    case A_DestroyAllChildren: { o = GO("gameObject"); if (o) for (UObj *c = o->child; c; c = c->next) { if (PB("disable", false)) uo_set_active(c, false); else uo_destroy(c); } FIN(); break; }
    case A_WaitForBossLoad: FIN(); ev(f, EV("sendEvent")); break;
    case A_SetBoxCollider2DSize: { o = GO("gameObject1"); if (!o) o = GO("gameObject");
        if (o) for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) {
            float x, y, z; if (pvec(f, a, "size", &x, &y, &z)) { o->col[i].w = x; o->col[i].h = y; }
            if (!NONE("width")) o->col[i].w = PF("width", o->col[i].w); if (!NONE("height")) o->col[i].h = PF("height", o->col[i].h); break; }
        FIN(); break; }
    case A_GetLastEvent: { FVar *v = VAR("storeEvent"); if (v) snprintf(v->s, sizeof v->s, "%s", f->lastEvent ? f->lastEvent : ""); FIN(); break; }
    case A_SetFsmVector3: { Fsm *t = fsm_of(f, a); FVar *v = t ? fsm_var(t, PS("variableName", "")) : NULL; float x, y, z; if (v && pvec(f, a, "setValue", &x, &y, &z)) { v->v[0] = x; v->v[1] = y; v->v[2] = z; } FIN(); break; }
    case A_SetTextMeshProAlignment: FIN(); break;
    case A_SendEventByScale: { o = GO("gameObject"); bool neg = o && uo_wsx(o) < 0; FIN(); ev(f, neg ? EV("negativeEvent") : EV("positiveEvent")); break; }
    case A_iTweenMoveTo: {
        o = GO("gameObject"); if (!o) { FIN(); break; }
        float tx = 0, ty = 0, tz = 0; UObj *tr = GO("transformPosition");
        if (tr) { tx = uo_wx(tr); ty = uo_wy(tr); } else pvec(f, a, "vectorPosition", &tx, &ty, &tz);
        a->b1 = PI("space", 0) == 1;
        a->f[0] = a->b1 ? o->lx : uo_wx(o); a->f[1] = a->b1 ? o->ly : uo_wy(o); a->f[2] = tx; a->f[3] = ty;
        a->t0 = -(NONE("delay") ? 0 : PF("delay", 0));
        break; }
    case A_LoadLevel: FIN(); game_load_level(PS("levelName", "")); break;
    case A_BeginSceneTransition: FIN(); game_begin_transition(PS("sceneName", ""), PS("entryGateName", "left1")); break;
    case A_SetMaterialColor: case A_SetTextMeshProColor: case A_SetSpriteRendererColor: case A_SetColorValue: case A_GetMaterialColor:
    case A_GetColorRGBA: case A_Tk2dSpriteGetColor:
        act_update(f, a, 0);
        if (!PB("everyFrame", false) && !PB("everyframe", false)) FIN();
        break;
    case A_FadeColorFader: { extern void comps_color_fade_tree(UObj *o, bool up, bool children);
        o = GO("target"); if (o) comps_color_fade_tree(o, PI("fadeType", 0) == 0, PB("useChildren", true)); FIN(); break; }
    case A_SetGameObjectActive: o = GO("gameObject"); if (o) uo_set_active(o, PB("active", true)); FIN(); break;
    case A_PlayParticleEmitter: {        // emit > 0: that many at once; otherwise Play (children too) unless already playing
        o = GO("gameObject"); int n = PI("emit", 0);
        if (o) { if (n > 0) particles_obj_emit(o, n); else particles_obj_play(o, true); }
        FIN(); break; }
    case A_StopParticleEmitter: o = GO("gameObject"); if (o) particles_obj_play(o, false); FIN(); break;
    case A_SetParticleEmission: o = GO("gameObject"); if (o) particles_obj_emission(o, PB("emission", true)); FIN(); break;
    case A_SetParticleEmissionRate: o = GO("gameObject"); if (o) particles_obj_rate(o, PF("emissionRate", 10)); FIN(); break;
    case A_TransitionToAudioSnapshot: { JVal *s = praw(a, "snapshot");      // build_objs.py: {snapshot, mixer} names
        if (s && s->t == J_OBJ && jget(s, "var")) { FVar *v = VAR("snapshot"); s = v ? v->obj : NULL; }
        if (s && s->t == J_OBJ && jget(s, "snapshot")) game_audio_snapshot(jstr(jget(s, "mixer"), ""), jstr(jget(s, "snapshot"), ""), PF("transitionTime", 0));
        FIN(); break; }
    case A_SetParticleEmissionSpeed: case A_VibrationPlayerPlay: case A_PlayVibration:
    case A_PlayVibrationV2: case A_SetProperty: case A_DebugLog: case A_SetTrailRenderer:
    case A_GetPreInstantiatedGameObject: case A_FadeAudio: case A_SetAudioVolume: case A_GetVelocity: case A_SetVelocity:
        FIN(); break;
    case A_ApplyMusicCue: { JVal *mc = praw(a, "musicCue"); const char *g = mc && mc->t == J_OBJ ? jstr(jget(mc, "asset"), NULL) : NULL;
        if (g) game_music_cue(g, PF("delayTime", 0), PF("transitionTime", 0)); FIN(); break; }
    case A_GetCurrentLanguageAsString: { FVar *v = VAR("stringVariable"); if (v) snprintf(v->s, sizeof v->s, "EN"); FIN(); break; }
    case A_SetName: o = GO("gameObject"); if (o) snprintf(o->name, sizeof o->name, "%s", PS("name", o->name)); FIN(); break;
    case A_iTweenShakePosition: a->t0 = 0; a->t1 = NONE("time") ? 1 : PF("time", 1); o = GO("gameObject");
        if (o) { a->f[0] = o->lx; a->f[1] = o->ly; } a->o0 = o; break;
    case A_GetEventSender: setgo(VAR("sentByGameObject"), uo_event_sender()); FIN(); break;
    case A_GetAxis: act_update(f, a, 0); if (!PB("everyFrame", true)) FIN(); break;
    case A_GetAngleToTarget: break;                       // OnLateUpdate
    case A_ChaseObjectV2: case A_DecelerateXY: act_fixed(f, a, 0); break;
    case A_TimeLimitSet: setf(VAR("storeValue"), uo_time + PF("timeDelay", 0)); FIN(); break;
    case A_TimeLimitCheck: { FVar *v = VAR("storedValue"); FIN(); ev(f, uo_time >= (v ? v->f : 0) ? EV("aboveEvent") : EV("belowEvent")); break; }
    case A_DistanceBetweenPoints: { float x1, y1, z1, x2, y2, z2; pvec(f, a, "point1", &x1, &y1, &z1); pvec(f, a, "point2", &x2, &y2, &z2);
        float dx = x2 - x1, dy = y2 - y1, dz = PB("ignoreZ", false) ? 0 : z2 - z1; setf(VAR("distanceResult"), sqrtf(dx * dx + dy * dy + dz * dz)); FIN(); break; }
    case A_CheckCurrentMapZone: { const char *z = game_map_zone(); bool eq = z && !strcmp(z, PS("mapZone", ""));
        FIN(); ev(f, eq ? EV("equalEvent") : EV("notEqualEvent")); break; }
    case A_AudioPlayInState: FIN(); break;
    case A_SetFloatToSmallest: { float x = PF("value1", 0), y = PF("value2", 0); setf(VAR("floatVariable"), x < y ? x : y); if (!PB("everyFrame", false)) FIN(); break; }
    case A_GGCheckIfBossSequence: { extern bool gg_pantheon_active(void); FIN(); ev(f, gg_pantheon_active() ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_HasComponent: { o = GO("gameObject"); bool has = o && uo_has_component_named(o, PS("component", "")); setb(VAR("store"), has);
        FIN(); ev(f, has ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_ColorInterpolate: { a->t0 = 0; JVal *cs = praw(a, "colors"); int n = cs ? cs->n : 0;
        FVar *sv = VAR("storeColor");
        if (n < 2) { if (n == 1 && sv) { float c[4]; JVal *e = jat(cs, 0); if (e && e->t == J_ARR) { for (int i = 0; i < 4; i++) c[i] = (float)jnum(jat(e, i), 1); store_color(sv, c); } } FIN(); break; }
        act_update(f, a, 0); break; }
    case A_RotateTo: break;
    case A_SpawnRandomObjectsOverTime: a->t0 = 0; break;
    case A_VibrationPlayerStop: case A_CreateGameObjectPool: case A_GGSetCanTransition: case A_SetInterpolate: case A_SetExtrapolate:
    case A_SendDreamImpact: comps_dream_impact(GO("target")); FIN(); break;
    case A_SendEventToGameObjectOptimized: o = GO("target"); if (o) { uo_set_event_sender(f->owner); uo_send_event(o, PS("sendEvent", NULL)); } if (!PB("everyFrame", false)) FIN(); break;
    case A_WaitForHeroInPosition: { extern bool uo_hero_ready(void);   // finishes once the Knight's stand-in holds his position in this room
        if (uo_hero_ready() && PB("skipIfAlreadyPositioned", false)) { FIN(); ev(f, EV("sendEvent")); }
        break; }
    case A_ScaleTo: {
        o = GO("gameObject"); if (!o) { FIN(); break; }
        float vx = 1, vy = 1, vz = 1;
        a->b0 = praw(a, "vectorScale") != NULL;         // iTween variant
        if (a->b0) pvec(f, a, "vectorScale", &vx, &vy, &vz); else pvec(f, a, "target", &vx, &vy, &vz);
        a->f[0] = o->sx; a->f[1] = o->sy; a->f[2] = vx; a->f[3] = vy; a->t0 = 0;
        break; }
    case A_GetTagCount: { int n = 0; const char *tg = PS("tag", ""); for (int i = 0; i < uo_count(); i++) { UObj *q = uo_obj(i); if (q && !q->destroyed && uo_active(q) && !strcmp(q->tag, tg)) n++; }
        seti(VAR("storeResult"), n); FIN(); break; }
    case A_GetNailDamage: seti(VAR("storeValue"), pd_int("nailDamage")); FIN(); break;
    case A_BeginRecoil: { o = GO("target"); if (o) comps_recoil_by_direction(o, PF("attackDirection", 0), PF("attackMagnitude", 1)); FIN(); break; }
    case A_Tk2dStopAnimation: o = GO("gameObject"); if (o) { if (o == uo_hero()) hero_anim_stop(); else o->an.playing = false; } FIN(); break;
    case A_SetStaticVariable: { StaticVar *v = svar(PS("variableName", NULL), true); JVal *sv = praw(a, "setValue");
        if (v && sv) { MArg m = marg_cp(f, sv); v->f = m.f; v->i = m.i; v->b = m.b; snprintf(v->s, sizeof v->s, "%s", m.s ? m.s : ""); } FIN(); break; }
    case A_GetStaticVariable: { StaticVar *v = svar(PS("variableName", NULL), false); FVar *t = VAR("storeValue");
        if (v && t) { t->f = v->f; t->i = v->i; t->b = v->b; snprintf(t->s, sizeof t->s, "%s", v->s); if (t->t == V_BOOL) t->b = v->b; } FIN(); break; }
    case A_CheckStaticBool: { StaticVar *v = svar(PS("variableName", NULL), false); FIN(); if (v && v->b) ev(f, EV("trueEvent")); else ev(f, EV("falseEvent")); break; }
    case A_IntClamp: { FVar *v = VAR("intVariable"); if (v) { int mn = PI("minValue", 0), mx = PI("maxValue", 0); if (v->i < mn) v->i = mn; if (v->i > mx) v->i = mx; } if (!PB("everyFrame", false)) FIN(); break; }
    case A_ListenForQuickCast: break;          // OnUpdate only
    case A_GetConstantsValue: { FVar *v = VAR("storeValue"); const char *n = PS("variableName", "");
        if (v && !strcmp(n, "GG_RETURN_SCENE")) snprintf(v->s, sizeof v->s, "GG_Workshop");
        else if (v && !strcmp(n, "HERO_DEFAULT_GRAVITY")) v->f = 0.79f;
        else if (v && !strcmp(n, "GG_DOOR_ENTRANCE_SCENE")) snprintf(v->s, sizeof v->s, "GG_Boss_Door_Entrance");
        FIN(); break; }
    case A_CheckCanDreamWarpInScene: FIN(); ev(f, EV("trueEvent")); break;
    case A_SetGameObjectSelf: setgo(VAR("variable"), GO("gameObject")); FIN(); break;
    case A_SendMessageV2: { JVal *fc = praw(a, "functionCall"); const char *fn = fc ? jstr(jget(fc, "function"), "") : ""; o = GO("gameObject");
        MArg a0 = marg_fc(f, fc), none = { 0 };
        if (o && o == uo_hero()) call_method(f, o, "HeroController", fn, a0, none, NULL);
        else if (o && o == uo_global("GameManager")) call_method(f, o, "GameManager", fn, a0, none, NULL);
        if (!PB("everyFrame", false)) FIN(); break; }
    case A_SpawnObjectFromGlobalPoolOverTime: a->t0 = 0; break;
    case A_FindClosest: act_update(f, a, 0); if (!PB("everyFrame", false)) FIN(); break;
    default: FIN(); break;
    }
}

void act_update(Fsm *f, FsmAct *a, float dt) {
    UObj *o;
    switch (a->type) {
    /* --- batch 3 --- */
    case A_KeepFloatPositive: { FVar *v = VAR("floatVariable"); if (v && v->f < 0) v->f = -v->f; break; }
    case A_FloatSignTest: ev(f, PF("floatValue", 0) < 0 ? EV("isNegative") : EV("isPositive")); break;
    case A_GetYDistance: { UObj *s2 = GO("gameObject"), *t = GO("target"); if (alive(s2) && alive(t)) setf(VAR("storeResult"), fabsf(uo_wy(s2) - uo_wy(t))); break; }
    case A_GetMidPoint: { UObj *s2 = GO("gameObject"), *t = GO("target"); if (alive(s2) && alive(t)) setv(VAR("midPoint"), (uo_wx(s2) + uo_wx(t)) / 2, (uo_wy(s2) + uo_wy(t)) / 2, 0); break; }
    case A_TransformPoint: { o = GO("gameObject"); float x = 0, y = 0, z = 0; pvec(f, a, "localPosition", &x, &y, &z);
        if (o) { float an = uo_wrz(o) * (float)M_PI / 180, lx = x * uo_wsx(o), ly = y * uo_wsy(o); setv(VAR("storeResult"), uo_wx(o) + lx * cosf(an) - ly * sinf(an), uo_wy(o) + lx * sinf(an) + ly * cosf(an), z); } break; }
    case A_GetColliderRange: { o = GO("gameObject"); if (o) for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { float x0, y0, x1, y1; if (uo_col_aabb(o, &o->col[i], &x0, &y0, &x1, &y1)) { setf(VAR("minX"), x0); setf(VAR("maxX"), x1); setf(VAR("minY"), y0); setf(VAR("maxY"), y1); } break; } break; }
    case A_SetColorRGBA: { FVar *v = VAR("colorVariable"); if (!v) break;
        if (!NONE("red")) v->v[0] = PF("red", 0); if (!NONE("green")) v->v[1] = PF("green", 0); if (!NONE("blue")) v->v[2] = PF("blue", 0); if (!NONE("alpha")) v->v[3] = PF("alpha", 1); break; }
    case A_GameObjectCompareTag: { UObj *g = GO("gameObject"); bool r = g && !strcmp(g->tag, PS("tag", "Untagged")); setb(VAR("storeResult"), r); ev(f, r ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_Vector3Lerp: { float x0, y0, z0, x1, y1, z1, t = PF("amount", 0); if (pvec(f, a, "fromVector", &x0, &y0, &z0) && pvec(f, a, "toVector", &x1, &y1, &z1)) setv(VAR("storeResult"), x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, z0 + (z1 - z0) * t); break; }
    case A_GetAngleBetweenPoints: { float x0, y0, z0, x1, y1, z1; if (pvec(f, a, "point1", &x0, &y0, &z0) && pvec(f, a, "point2", &x1, &y1, &z1)) setf(VAR("storeAngle"), atan2f(y1 - y0, x1 - x0) * 180 / (float)M_PI); break; }
    case A_SetFsmColor: { Fsm *t = fsm_of(f, a); FVar *v = t ? fsm_var(t, PS("variableName", "")) : NULL; float c[4]; if (v && pcolor(f, a, "setValue", c)) store_color(v, c); break; }
    case A_GetRandomObject: { const char *tg = PS("withTag", "Untagged"); UObj *cand[64]; int n = 0;
        for (int i = 0; i < uo_count() && n < 64; i++) { UObj *q = uo_obj(i); if (q && !q->destroyed && q->used && uo_active(q) && !strcmp(q->tag, tg)) cand[n++] = q; }
        FVar *v = VAR("storeResult"); if (v) v->go = n ? cand[rand() % n] : NULL; break; }
    case A_GetHPEveryFrame: { o = GO("target"); if (o && uo_comp(o, C_HEALTH)) seti(VAR("storeValue"), hm_hp(o)); break; }
    case A_DistanceWalk: if (a->t3 > 0) a->t3 -= dt; break;
    case A_TranslateContinuous: { o = GO("gameObject"); if (o) { o->lx += PF("x", 0) * dt; o->ly += PF("y", 0) * dt; } break; }
    case A_SpawnRandomObjectsOverTimeV2: {
        a->t0 += dt; if (a->t0 < PF("frequency", 1)) break; a->t0 = 0;
        int ti = tmpl_of(f, a, "gameObject"); if (ti < 0) break;
        float x = 0, y = 0; UObj *sp = GO("spawnPoint"); float px, py, pz; bool hp = !NONE("position") && pvec(f, a, "position", &px, &py, &pz);
        if (sp) { x = uo_wx(sp); y = uo_wy(sp); if (hp) { x += px; y += py; } } else if (hp) { x = px; y = py; }
        int n = irand(PI("spawnMin", 1), PI("spawnMax", 1) + 1); float ovx = PF("originVariationX", 0), ovy = PF("originVariationY", 0);
        for (int i = 0; i < n; i++) {
            UObj *c = uo_spawn(ti, x + frand(-ovx, ovx), y + frand(-ovy, ovy), 0); if (!c) continue;
            float s2 = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180;
            if (c->rb.has) { c->rb.vx = s2 * cosf(an); c->rb.vy = s2 * sinf(an); }
            float sc = frand(PF("scaleMin", 1), PF("scaleMax", 1)); if (sc != 1) { c->sx = sc; c->sy = sc; }
        }
        break; }

    case A_Wait: case A_WaitRandom: case A_RandomWait:
        a->t0 += dt;
        if (a->t0 >= a->t1) { FIN(); ev(f, EV("finishEvent")); }
        break;
    case A_FloatDivideV2: if (!PB("fixedUpdate", false)) { FVar *v = VAR("floatVariable"); float d = PF("divideBy", 1); if (v && d != 0) var_set_float(v, v->f / d); } break;
    case A_AnyKey: { extern bool game_any_key_down(void); if (game_any_key_down()) ev(f, EV("sendEvent")); break; }
    case A_Vector3Compare: {
        float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0, tol = PF("tolerance", 0);
        pvec(f, a, "vector3Variable1", &ax, &ay, &az); pvec(f, a, "vector3Variable2", &bx, &by, &bz);
        ev(f, EV(fabsf(ax - bx) <= tol && fabsf(ay - by) <= tol && fabsf(az - bz) <= tol ? "equal" : "notEqual"));
        break; }
    case A_LerpTk2dSpriteColor: {
        o = a->o0; if (!alive(o)) { FIN(); break; }
        a->t0 += dt; float lt = PF("LerpTime", 1), k = lt > 0 ? a->t0 / lt : 1; if (k > 1) k = 1;
        float c[4] = { 1, 1, 1, 1 }; JVal *cv = praw(a, "TargetColor");
        if (cv && cv->t == J_ARR) for (int i = 0; i < 4 && i < cv->n; i++) c[i] = jnum(jat(cv, i), 1);
        else if (cv && cv->t == J_OBJ && jget(cv, "var")) { FVar *v = fsm_var_j(f, jget(cv, "var")); if (v) for (int i = 0; i < 4; i++) c[i] = v->v[i]; }
        o->tintR = a->f[0] + (c[0] - a->f[0]) * k; o->tintG = a->f[1] + (c[1] - a->f[1]) * k;
        o->tintB = a->f[2] + (c[2] - a->f[2]) * k; o->tintA = a->f[3] + (c[3] - a->f[3]) * k;
        if (k >= 1) FIN();
        break; }
    case A_LineCast2d: {
        if (dt != 0) { if (--a->i0 != 0) break; }
        a->i0 = PI("repeatInterval", 1);
        UObj *from = GO("fromGameObject"), *to = GO("toGameObject");
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0, z;
        pvec(f, a, "fromPosition", &x0, &y0, &z); if (from) { x0 += uo_wx(from); y0 += uo_wy(from); }
        pvec(f, a, "toPosition", &x1, &y1, &z); if (to) { x1 += uo_wx(to); y1 += uo_wy(to); }
        JVal *lm = praw(a, "layerMask"); bool terr = !lm || lm->n == 0;      // only terrain (8) is modelled
        for (JVal *e = lm ? lm->child : NULL; e; e = e->next) if (jv_int(f, e, -1) == 8) terr = true;
        if (PB("invertMask", false)) terr = !terr;
        float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy), h = 0;
        bool hit = terr && len > 1e-5f && w_raycast(x0, y0, dx / len, dy / len, len, &h);
        setb(VAR("storeDidHit"), hit);
        if (hit) { setv(VAR("storeHitPoint"), x0 + dx / len * h, y0 + dy / len * h, 0); setf(VAR("storeHitDistance"), h / len); ev(f, EV("hitEvent")); }
        break; }
    case A_NextFrameEvent: FIN(); ev(f, EV("sendEvent")); break;
    case A_WaitForHeroInPosition: { extern bool uo_hero_ready(void); if (uo_hero_ready()) { FIN(); ev(f, EV("sendEvent")); } break; }
    case A_GGWaitForBossSceneTransitionIn: a->t0 -= dt; if (a->t0 <= 0) { FIN(); ev(f, EV("finishEvent")); } break;
    case A_SendEventByName: case A_SendEvent: case A_SendEventByNameV2:
        if (!PB("everyFrame", false)) { if (a->t0 > 0) { a->t0 -= dt; if (a->t0 <= 0) FIN(); } }
        else send_target(f, praw(a, "eventTarget"), a->type == A_SendEvent ? EV("sendEvent") : PS("sendEvent", NULL), 0);
        break;
    case A_SendRandomEvent: a->t0 -= dt; if (a->t0 <= 0) FIN(); break;
    case A_BoolTest: { bool v = PB("boolVariable", false); ev(f, v ? EV("isTrue") : EV("isFalse")); break; }
    case A_BoolAllTrue: case A_BoolNoneTrue: {
        JVal *bv = praw(a, "boolVariables"); bool all = true, none = true;
        for (JVal *e = bv ? bv->child : NULL; e; e = e->next) { bool b = jv_bool(f, e, false); if (b) none = false; else all = false; }
        bool r = a->type == A_BoolAllTrue ? all : none;
        if (r) ev(f, EV("sendEvent"));
        setb(VAR("storeResult"), r);
        break; }
    case A_BoolFlipEveryFrame: { FVar *v = VAR("boolVariable"); if (v) v->b = !v->b; break; }
    case A_FloatCompare: {
        float a1 = PF("float1", 0), a2 = PF("float2", 0), tol = PF("tolerance", 0);
        if (fabsf(a1 - a2) <= tol) ev(f, EV("equal")); else if (a1 < a2) ev(f, EV("lessThan")); else if (a1 > a2) ev(f, EV("greaterThan"));
        break; }
    case A_IntCompare: {
        int a1 = PI("integer1", 0), a2 = PI("integer2", 0);
        if (a1 == a2) ev(f, EV("equal")); else if (a1 < a2) ev(f, EV("lessThan")); else ev(f, EV("greaterThan"));
        break; }
    case A_StringCompare: {
        const char *s1 = PS("stringVariable", ""), *s2 = PS("compareTo", ""); bool eq = !strcmp(s1 ? s1 : "", s2 ? s2 : "");
        setb(VAR("storeResult"), eq); ev(f, eq ? EV("equalEvent") : EV("notEqualEvent"));
        break; }
    case A_FloatSwitch: {
        if (NONE("floatVariable")) break;
        float v = PF("floatVariable", 0); JVal *lt = praw(a, "lessThan"), *se = praw(a, "sendEvent"); int i = 0;
        for (JVal *e = lt ? lt->child : NULL; e; e = e->next, i++) if (v < jv_float(f, e, 0)) { JVal *x = jat(se, i); ev(f, x ? jstr(jget(x, "event"), NULL) : NULL); break; }
        break; }
    case A_FloatInRange: {
        if (NONE("floatVariable")) break;
        float v = PF("floatVariable", 0); bool in = v <= PF("upperValue", 0) && v >= PF("lowerValue", 0);
        setb(VAR("boolVariable"), in); ev(f, in ? EV("trueEvent") : EV("falseEvent"));
        break; }
    case A_FloatTestToBool: {
        float a1 = PF("float1", 0), a2 = PF("float2", 0);
        setb(VAR("equalBool"), fabsf(a1 - a2) <= PF("tolerance", 0)); setb(VAR("lessThanBool"), a1 < a2); setb(VAR("greaterThanBool"), a1 > a2);
        break; }
    case A_GameObjectIsNull: { bool n = !alive(GO("gameObject")); setb(VAR("storeResult"), n); ev(f, n ? EV("isNull") : EV("isNotNull")); break; }
    case A_SetFloatValue: setf(VAR("floatVariable"), PF("floatValue", 0)); break;
    case A_SetFloatValueV2: if (NONE("activeBool") || PB("activeBool", false)) setf(VAR("floatVariable"), PF("floatValue", 0)); break;
    case A_SetIntValue: seti(VAR("intVariable"), PI("intValue", 0)); break;
    case A_SetBoolValue: setb(VAR("boolVariable"), PB("boolValue", false)); break;
    case A_SetStringValue: { FVar *v = VAR("stringVariable"); const char *s = PS("stringValue", ""); if (v) { strncpy(v->s, s ? s : "", FVAR_STR - 1); v->s[FVAR_STR - 1] = 0; } break; }
    case A_SetGameObject: {      // a prefab reference is kept as the reference (SpawnObjectFromGlobalPool spawns it later)
        FVar *v = VAR("variable"); JVal *src = praw(a, "gameObject");
        if (v) { if (src && src->t == J_OBJ && jget(src, "prefab")) { v->go = NULL; v->obj = src; }
                 else { v->go = GO("gameObject"); if (v->obj && v->obj->t == J_OBJ && jget(v->obj, "prefab")) v->obj = NULL; } }
        break; }
    case A_SetVector2XY: {
        FVar *v = VAR("vector2Variable"); if (!v) break;
        float x = v->v[0], y = v->v[1], z;
        if (!NONE("vector2Value")) pvec(f, a, "vector2Value", &x, &y, &z);
        if (!NONE("x")) x = PF("x", 0);
        if (!NONE("y")) y = PF("y", 0);
        v->v[0] = x; v->v[1] = y; break; }
    case A_SetVector3XYZ: {
        FVar *v = VAR("vector3Variable"); if (!v) break;
        float x = v->v[0], y = v->v[1], z = v->v[2];
        if (!NONE("vector3Value")) pvec(f, a, "vector3Value", &x, &y, &z);
        if (!NONE("x")) x = PF("x", 0);
        if (!NONE("y")) y = PF("y", 0);
        if (!NONE("z")) z = PF("z", 0);
        setv(v, x, y, z); break; }
    case A_GetVector3XYZ: { FVar *v = VAR("vector3Variable"); if (!v) break; setf(VAR("storeX"), v->v[0]); setf(VAR("storeY"), v->v[1]); setf(VAR("storeZ"), v->v[2]); break; }
    case A_FloatAdd: case A_FloatSubtract: {
        FVar *v = VAR("floatVariable"); if (!v) break;
        float d = a->type == A_FloatAdd ? PF("add", 0) : -PF("subtract", 0);
        if (PB("perSecond", false)) d *= dt;
        var_set_float(v, (v->t == V_INT ? v->i : v->f) + d); break; }
    case A_FloatAddV2:
        if (PB("fixedUpdate", false)) break;
        if (NONE("activeBool") || PB("activeBool", false)) { FVar *v = VAR("floatVariable"); if (v) v->f += PB("perSecond", false) ? PF("add", 0) * dt : PF("add", 0); }
        break;
    case A_FloatMultiply: case A_FloatMultiplyV2: {
        if (a->type == A_FloatMultiplyV2 && PB("fixedUpdate", false) && dt != 0) break;
        FVar *v = VAR("floatVariable"); if (v) v->f *= PF("multiplyBy", 1); break; }
    case A_FloatDivide: { FVar *v = VAR("floatVariable"); float d = PF("divideBy", 1); if (v && d != 0) v->f /= d; break; }
    case A_FloatClamp: { FVar *v = VAR("floatVariable"); if (v) { float lo = PF("minValue", 0), hi = PF("maxValue", 0); if (v->f < lo) v->f = lo; if (v->f > hi) v->f = hi; } break; }
    case A_FloatOperator: case A_IntOperator: {
        bool isf = a->type == A_FloatOperator;
        float x = isf ? PF("float1", 0) : PI("integer1", 0), y = isf ? PF("float2", 0) : PI("integer2", 0), r = 0;
        switch (PI("operation", 0)) {
            case 0: r = x + y; break; case 1: r = x - y; break; case 2: r = x * y; break;
            case 3: r = y != 0 ? (isf ? x / y : (float)((int)x / (int)y)) : 0; break;
            case 4: r = fminf(x, y); break; case 5: r = fmaxf(x, y); break;
        }
        if (isf) setf(VAR("storeResult"), r); else seti(VAR("storeResult"), (int)r);
        break; }
    case A_IntAdd: { FVar *v = VAR("intVariable"); if (v) v->i += PI("add", 0); break; }
    case A_DistanceBetweenPoints2D: {
        float x1 = 0, y1 = 0, x2 = 0, y2 = 0, z; pvec(f, a, "point1", &x1, &y1, &z); pvec(f, a, "point2", &x2, &y2, &z);
        setf(VAR("distanceResult"), sqrtf((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2))); break; }
    case A_GetDistance: {
        UObj *s = GO("gameObject"), *t = GO("target");
        if (alive(s) && alive(t)) { float dx = uo_wx(s) - uo_wx(t), dy = uo_wy(s) - uo_wy(t); setf(VAR("storeResult"), sqrtf(dx * dx + dy * dy)); }
        break; }
    case A_GetAngleToTarget2D: {
        UObj *s = GO("gameObject"), *t = GO("target"); if (!alive(s) || !alive(t)) break;
        float ang = atan2f(uo_wy(t) + PF("offsetY", 0) - uo_wy(s), uo_wx(t) + PF("offsetX", 0) - uo_wx(s)) * 180 / (float)M_PI;
        while (ang < 0) ang += 360;
        setf(VAR("storeAngle"), ang);
        if (dt != 0 && !PB("everyFrame", false)) FIN();
        break; }
    case A_CheckTargetDirection: {
        UObj *s = GO("gameObject"), *t = GO("target"); if (!alive(s) || !alive(t)) break;
        float x = uo_wx(s), y = uo_wy(s), tx = uo_wx(t), ty = uo_wy(t);
        if (x < tx) ev(f, EV("rightEvent")); setb(VAR("rightBool"), x < tx);
        if (x > tx) ev(f, EV("leftEvent")); setb(VAR("leftBool"), x > tx);
        if (y < ty) ev(f, EV("aboveEvent")); setb(VAR("aboveBool"), y < ty);
        if (y > ty) ev(f, EV("belowEvent")); setb(VAR("belowBool"), y > ty);
        break; }
    case A_GetPosition: do_get_position(f, a); break;
    case A_GetVelocity2d: do_get_velocity(f, a); break;
    case A_GetScale: { o = GO("gameObject"); if (!o) break; bool w = PI("space", 0) == 0; float sx = w ? uo_wsx(o) : o->sx, sy = w ? uo_wsy(o) : o->sy;
        setv(VAR("vector"), sx, sy, 1); setf(VAR("xScale"), sx); setf(VAR("yScale"), sy); setf(VAR("zScale"), 1); break; }
    case A_GetRotation: { o = GO("gameObject"); if (!o) break; float z = norm360(PI("space", 0) == 1 ? o->rz : uo_wrz(o));
        setv(VAR("vector"), 0, 0, z); setf(VAR("xAngle"), 0); setf(VAR("yAngle"), 0); setf(VAR("zAngle"), z); break; }
    case A_FaceObject: do_face_object(f, a); break;
    case A_FaceDirection: do_face_direction(f, a, dt); break;
    case A_CheckAlertRange: { UObj *r = GO("alertRange"); setb(VAR("storeResult"), r && alert_in_range(r)); break; }
    case A_CheckAlertRangeByName: setb(VAR("storeResult"), a->o0 && alert_in_range(a->o0)); break;
    case A_CheckCanSeeHero: setb(VAR("storeResult"), a->o0 && los_can_see(a->o0)); break;
    case A_GetCanSeeHero: { UObj *l = GO("lineOfSightDetector"); setb(VAR("storeResult"), l && los_can_see(l)); break; }
    case A_SetFsmBool: case A_SetFsmInt: case A_SetFsmFloat: case A_SetFsmString: case A_SetFsmGameObject: {
        {   // "health_manager(_enemy)" was an FSM in older builds; the HealthManager component took its variables over
            const char *fn = PS("fsmName", ""); UObj *go = GO("gameObject");
            if (go && fn && !strncmp(fn, "health_manager", 14) && uo_comp(go, C_HEALTH) && !fsm_find_on(go, fn)) {
                const char *vn = PS("variableName", "");
                if (a->type == A_SetFsmGameObject && !strcmp(vn, "Battle Scene")) hm_set_battle_scene(go, GO("setValue"));
                else if (a->type == A_SetFsmInt && !strcmp(vn, "HP")) hm_set_hp(go, PI("setValue", 0));
                break;
            }
        }
        Fsm *t = fsm_of(f, a); if (!t) break;
        FVar *v = fsm_var(t, PS("variableName", "")); if (!v) break;
        switch (a->type) {
            case A_SetFsmBool: v->b = PB("setValue", false); break;
            case A_SetFsmInt: var_set_int(v, PI("setValue", 0)); break;
            case A_SetFsmFloat: var_set_float(v, PF("setValue", 0)); break;
            case A_SetFsmString: { const char *s = PS("setValue", ""); strncpy(v->s, s ? s : "", FVAR_STR - 1); break; }
            case A_SetFsmGameObject: v->go = GO("setValue"); break;
        }
        break; }
    case A_GetFsmBool: case A_GetFsmFloat: {
        UObj *go = GO("gameObject");
        if (go && go == uo_hero_slash() && !strcmp(PS("variableName", ""), "direction")) { setf(VAR("storeValue"), P.slashAngle); break; }
        Fsm *t = fsm_of(f, a); if (!t) break;
        FVar *v = fsm_var(t, PS("variableName", "")); if (!v) break;
        if (a->type == A_GetFsmBool) setb(VAR("storeValue"), v->b); else setf(VAR("storeValue"), v->t == V_INT ? v->i : v->f);
        break; }
    case A_ActivateGameObject: {
        o = GO("gameObject"); if (!o) break;
        bool act = PB("activate", true);
        if (PB("recursive", true)) do_activate(f, a, o, act, true); else uo_set_active(o, act);
        a->o0 = o; break; }
    case A_Tk2dSpriteSetColor: {
        o = GO("gameObject"); if (!o) break;
        JVal *c = praw(a, "color");
        if (c && c->t == J_ARR && c->n >= 4) { o->tintR = jnum(jat(c, 0), 1); o->tintG = jnum(jat(c, 1), 1); o->tintB = jnum(jat(c, 2), 1); o->tintA = jnum(jat(c, 3), 1); }
        else if (c && c->t == J_OBJ && jget(c, "var")) { FVar *v = fsm_var_j(f, jget(c, "var")); if (v) { o->tintR = v->v[0]; o->tintG = v->v[1]; o->tintB = v->v[2]; o->tintA = v->v[3]; } }
        break; }
    case A_Tk2dSpriteGetId: o = GO("gameObject"); if (o) seti(VAR("spriteID"), uo_sprite_id(o)); break;
    case A_SetIsFixedAngle2d: break;
    case A_SetPosition: if (!PB("lateUpdate", false)) do_set_position(f, a); break;
    case A_SetRotation: if (!PB("lateUpdate", false)) do_set_rotation(f, a); break;
    case A_SetScale: if (!PB("lateUpdate", false)) do_set_scale(f, a); break;
    case A_FlipScale: if (!PB("lateUpdate", false)) do_flip(f, a); break;
    case A_Translate: case A_TranslateV2: if (!PB("lateUpdate", false) && !PB("fixedUpdate", false)) do_translate(f, a, dt); break;
    case A_RayCast2d: {
        if (dt != 0) { if (--a->i0 != 0) break; }
        a->i0 = PI("repeatInterval", 1);
        float dist = PF("distance", 100);
        if (fabsf(dist) < 1e-6f) break;
        UObj *from = GO("fromGameObject");
        float x = 0, y = 0, z, dx = 0, dy = 0;
        pvec(f, a, "fromPosition", &x, &y, &z);
        if (from) { x += uo_wx(from); y += uo_wy(from); }
        pvec(f, a, "direction", &dx, &dy, &z);
        if (from && PI("space", 1) == 1) { float rx, ry; world_rot_dir(from, dx, dy, &rx, &ry); dx = rx; dy = ry; }
        float len = sqrtf(dx * dx + dy * dy); if (len > 0) { dx /= len; dy /= len; }
        // layer mask: only terrain (8) is modelled
        JVal *lm = praw(a, "layerMask"); bool terr = !lm || lm->n == 0;
        for (JVal *e = lm ? lm->child : NULL; e; e = e->next) if (jv_int(f, e, -1) == 8) terr = true;
        if (PB("invertMask", false)) terr = !terr;
        float h = 0; bool hit = terr && w_raycast(x, y, dx, dy, dist > 0 ? dist : 1000, &h);
        setb(VAR("storeDidHit"), hit);
        if (hit) {
            setv(VAR("storeHitPoint"), x + dx * h, y + dy * h, 0); setf(VAR("storeHitDistance"), h);
            setf(VAR("storeHitFraction"), dist > 0 ? h / dist : 0);
            ev(f, EV("hitEvent"));
        }
        break; }
    case A_CheckCollisionSide:
        if (PB("topHit", false) || PB("bottomHit", false) || PB("rightHit", false) || PB("leftHit", false)) check_touching(f, a, false);
        break;
    case A_Tk2dWatchAnimationEvents:
        // nothing to watch (no animator, inactive, or the clip already ended): the completion fires right away
        // (the Knight's clips run in the hero code: his completion arrives through the delegate, as with WithEvents;
        // finishing here at once cut the fireball recoil short)
        if (a->o0 && a->o0 == uo_hero()) { extern bool hero_anim_done(void); if (hero_anim_done()) { FIN(); ev(f, EV("animationCompleteEvent")); } }
        else if (!a->o0 || !a->o0->an.has || !uo_active(a->o0) || !a->o0->an.playing) { FIN(); ev(f, EV("animationCompleteEvent")); }
        break;
    case A_StopWalker: if (a->o0) walker_stop(a->o0, 1); break;
    case A_StartWalker:
        if (a->o0) { if (NONE("walkRight")) walker_start(a->o0); else walker_go(a->o0, PB("walkRight", false) ? 1 : -1); walker_clear_turn_cooldown(a->o0); }
        break;
    case A_SetWalkerFacing:
        if (a->o0) { if (PB("randomStartDir", false)) walker_change_facing(a->o0, rand() % 2 ? 1 : -1); else if (!NONE("walkRight")) walker_change_facing(a->o0, PB("walkRight", false) ? 1 : -1); }
        break;
    case A_SpawnObjectFromGlobalPoolOverTimeV2: {
        a->t0 += dt; if (a->t0 < PF("frequency", 1)) break;
        a->t0 = 0;
        int ti = tmpl_of(f, a, "gameObject"); if (ti < 0) break;
        UObj *g = spawn_at(ti, f, a, true, NULL);
        if (g) { float s = frand(PF("scaleMin", 1), PF("scaleMax", 1)); if (s != 1) { g->sx = s; g->sy = s; } }
        break; }
    case A_AudioPlayerOneShotSingle: case A_AudioPlayerOneShot: {
        float d = PF("delay", 0);
        if (dt >= 0) { if (d <= 0) break; if (a->t0 < d) { a->t0 += dt; break; } }
        UObj *sp = GO("spawnPoint"); (void)sp;
        float vol = PF("volume", 1), pmin = PF("pitchMin", 1), pmax = PF("pitchMax", 1);
        if (a->type == A_AudioPlayerOneShotSingle) play_clip(aname(praw(a, "audioClip")), vol, pmin, pmax);
        else { JVal *cl = praw(a, "audioClips"); int i = weighted(f, praw(a, "weights")); if (cl && i >= 0) play_clip(aname(jat(cl, i)), vol, pmin, pmax); }
        if (dt >= 0) FIN();
        break; }
    case A_AudioPlay: case A_AudioPlayV2:
        a->t0 -= dt; if (a->t0 <= 0) { FIN(); ev(f, EV("finishedEvent")); }
        break;
    case A_iTweenMoveBy: tween_step(f, a, dt, false); break;
    case A_iTweenScaleBy: tween_step(f, a, dt, true); break;
    case A_EaseFloat: case A_EaseColor: {
        a->t0 += dt; if (a->t0 < 0) break;
        float time = NONE("time") ? 1 : PF("time", 1);
        if (!NONE("speed") && a->type == A_EaseFloat) { float sp = PF("speed", 0); time = sp > 0 ? fabsf(a->f[1] - a->f[0]) / sp : 1; }
        float t = time > 0 ? a->t0 / time : 1; if (t > 1) t = 1;
        float e = ease(PI("easeType", 21), t); if (PB("reverse", false)) e = 1 - e;
        if (a->type == A_EaseFloat) setf(VAR("floatVariable"), a->f[0] + (a->f[1] - a->f[0]) * e);
        else { float to[4] = { a->f[4], a->f[5], a->t1, a->t2 }, c[4]; for (int i = 0; i < 4; i++) c[i] = a->f[i] + (to[i] - a->f[i]) * e; store_color(VAR("colorVariable"), c); }
        if (t >= 1) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_SpawnBloodTime: break;
    /* --- batch 2 --- */
    case A_RayCast2dV2: {
        if (dt != 0) { if (--a->i0 != 0) break; }
        a->i0 = PI("repeatInterval", 1);
        float dist = PF("distance", 100); if (dist == 0) break;
        UObj *from = GO("fromGameObject");
        float x = 0, y = 0, z, dx = 0, dy = 0;
        pvec(f, a, "fromPosition", &x, &y, &z);
        if (from) { x += uo_wx(from); y += uo_wy(from); }
        pvec(f, a, "direction", &dx, &dy, &z);
        if (from && PI("space", 1) == 1) { float rx, ry; world_rot_dir(from, dx, dy, &rx, &ry); dx = rx; dy = ry; }
        float len = sqrtf(dx * dx + dy * dy); if (len > 0) { dx /= len; dy /= len; }
        JVal *lm = praw(a, "layerMask"); bool terr = !lm || lm->n == 0;
        for (JVal *e = lm ? lm->child : NULL; e; e = e->next) if (jv_int(f, e, -1) == 8) terr = true;
        if (PB("invertMask", false)) terr = !terr;
        float h = 0; bool hit = terr && w_raycast(x, y, dx, dy, dist > 0 ? dist : 1000, &h);
        setb(VAR("storeDidHit"), hit);
        if (hit) { setv(VAR("storeHitPoint"), x + dx * h, y + dy * h, 0); setf(VAR("storeHitDistance"), dist > 0 ? h / dist : 0); setf(VAR("storeDistance"), h); ev(f, EV("hitEvent")); }
        break; }
    case A_ConstrainMovement: {
        o = GO("gameObject"); if (!o) break;
        float x = uo_wx(o), y = uo_wy(o), cx = PF("xConstrain", 0), cy = PF("yConstrain", 0);
        if (x > a->f[0] + cx) x = a->f[0] + cx; else if (x < a->f[0] - cx) x = a->f[0] - cx;
        if (y > a->f[1] + cy) y = a->f[1] + cy; else if (y < a->f[1] - cy) y = a->f[1] - cy;
        uo_set_world_pos(o, x, y); a->f[0] = x; a->f[1] = y; break; }
    case A_LookAt2dGameObjectSmooth: {
        o = GO("gameObject"); UObj *t = GO("targetObject"); if (!o || !alive(t)) break;
        float ang = atan2f(uo_wy(t) - uo_wy(o), uo_wx(t) - uo_wx(o)) * 180 / (float)M_PI; while (ang < 0) ang += 360;
        float want = ang - PF("rotationOffset", 0), sp = PF("speed", 0), z = norm360(uo_wrz(o));
        if (z < want) { z += sp; if (z > want) z = want; set_world_rz(o, z); }
        z = norm360(uo_wrz(o));
        if (z > want) { z -= sp; if (z < want) z = want; set_world_rz(o, z); }
        break; }
    case A_BoundsBoxCollider: {
        o = GO("gameObject1"); if (!o) break;
        for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { float x0, y0, x1, y1; uo_col_aabb(o, &o->col[i], &x0, &y0, &x1, &y1);
            setv(VAR("scaleVector2"), x1 - x0, y1 - y0, 0); setf(VAR("scaleX"), x1 - x0); setf(VAR("scaleY"), y1 - y0); break; }
        break; }
    case A_BoxColliderOffset: {
        o = GO("gameObject1"); if (!o) break;
        for (int i = 0; i < o->ncol; i++) if (o->col[i].t == COL_BOX) { setv(VAR("offsetVector2"), o->col[i].ox, o->col[i].oy, 0); setf(VAR("offsetX"), o->col[i].ox); setf(VAR("offsetY"), o->col[i].oy); break; }
        break; }
    case A_GetVelocityAsAngle: { URb *rb = rbody(GO("gameObject")); if (!rb) break; float n = atan2f(rb->vx, -rb->vy) * 180 / (float)M_PI - 90; if (n < 0) n += 360; setf(VAR("storeAngle"), n); break; }
    case A_GetSpeed2d: { URb *rb = rbody(GO("gameObject")); if (rb) setf(VAR("storeResult"), sqrtf(rb->vx * rb->vx + rb->vy * rb->vy)); break; }
    case A_BoolTestMulti: {
        JVal *bv = praw(a, "boolVariables"), *bs = praw(a, "boolStates");
        if (!bv || !bs || bv->n == 0 || bv->n != bs->n) break;
        bool ok = true; JVal *y = bs->child;
        for (JVal *x = bv->child; x && y; x = x->next, y = y->next) if (jv_bool(f, x, false) != jv_bool(f, y, false)) { ok = false; break; }
        setb(VAR("storeResult"), ok); ev(f, ok ? EV("trueEvent") : EV("falseEvent"));
        break; }
    case A_BoolAnyTrue: {
        JVal *bv = praw(a, "boolVariables"); bool any = false;
        for (JVal *e = bv ? bv->child : NULL; e; e = e->next) if (jv_bool(f, e, false)) any = true;
        setb(VAR("storeResult"), any); if (any) ev(f, EV("sendEvent"));
        break; }
    case A_IntSwitch: {
        int v = PI("intVariable", 0); JVal *cmp = praw(a, "compareTo"), *se = praw(a, "sendEvent"); int i = 0;
        for (JVal *e = cmp ? cmp->child : NULL; e; e = e->next, i++) if (jv_int(f, e, 0) == v) { JVal *q = jat(se, i); ev(f, q ? jstr(jget(q, "event"), NULL) : NULL); break; }
        break; }
    case A_GetXDistance: { UObj *s = GO("gameObject"), *t = GO("target"); if (alive(s) && alive(t)) setf(VAR("storeResult"), fabsf(uo_wx(s) - uo_wx(t))); break; }
    case A_GameObjectIsVisible: {
        o = GO("gameObject"); if (!o) break;
        float dx = uo_wx(o) - uo_hero_x(), dy = uo_wy(o) - uo_hero_y(); bool vis = fabsf(dx) < 15 && fabsf(dy) < 9;
        setb(VAR("storeResult"), vis); ev(f, vis ? EV("trueEvent") : EV("falseEvent")); break; }
    case A_Tk2dSpriteSetScale: break;
    case A_GetVector2XY: { FVar *v = VAR("vector2Variable"); if (v) { setf(VAR("storeX"), v->v[0]); setf(VAR("storeY"), v->v[1]); } break; }
    case A_SetVector3Value: { FVar *v = VAR("vector3Variable"); float x, y, z; if (v && pvec(f, a, "vector3Value", &x, &y, &z)) setv(v, x, y, z); break; }
    case A_Vector3Multiply: { FVar *v = VAR("vector3Variable"); float m = PF("multiplyBy", 1); if (v) setv(v, v->v[0] * m, v->v[1] * m, v->v[2] * m); break; }
    case A_Vector3AddXYZ: { FVar *v = VAR("vector3Variable"); if (!v) break; float k = PB("perSecond", false) ? dt : 1;
        setv(v, v->v[0] + PF("addX", 0) * k, v->v[1] + PF("addY", 0) * k, v->v[2] + PF("addZ", 0) * k); break; }
    case A_ConvertIntToFloat: setf(VAR("floatVariable"), (float)PI("intVariable", 0)); break;
    case A_ConvertFloatToInt: { float x = PF("floatVariable", 0); int r = PI("rounding", 0); seti(VAR("intVariable"), r == 1 ? (int)floorf(x + 0.5f) : r == 2 ? (int)ceilf(x) : r == 3 ? (int)floorf(x) : (int)x); break; }
    case A_GetFsmInt: {
        { const char *fn = PS("fsmName", ""); UObj *go = GO("gameObject");
          if (go && fn && !strncmp(fn, "health_manager", 14) && uo_comp(go, C_HEALTH) && !fsm_find_on(go, fn)) {
              if (!strcmp(PS("variableName", ""), "HP")) seti(VAR("storeValue"), hm_get_hp(go)); break; } }
        Fsm *t = fsm_of(f, a); FVar *v = t ? fsm_var(t, PS("variableName", "")) : NULL; if (v) seti(VAR("storeValue"), v->t == V_FLOAT ? (int)v->f : v->i); break; }
    case A_GetLayer: o = GO("gameObject"); if (o) seti(VAR("storeResult"), o->layer); break;
    case A_GetTag: { o = GO("gameObject"); FVar *v = VAR("storeResult"); if (o && v) strncpy(v->s, o->tag, FVAR_STR - 1); break; }
    case A_GetName: { o = GO("gameObject"); FVar *v = VAR("storeName"); if (o && v) strncpy(v->s, o->name, FVAR_STR - 1); break; }
    case A_FlingObjectsFromGlobalPoolTime: {
        a->t0 += dt; if (a->t0 < PF("frequency", 1)) break;
        a->t0 = 0; int ti = tmpl_of(f, a, "gameObject"); if (ti >= 0) fling_from(f, a, ti, true); break; }
    case A_GameObjectHasChildren: { o = GO("gameObject"); if (o) { bool has = false; for (UObj *c = o->child; c; c = c->next) if (!c->destroyed) { has = true; break; }
        setb(VAR("storeResult"), has); ev(f, EV(has ? "trueEvent" : "falseEvent")); } break; }
    case A_GameObjectCompare: { bool eq = GO("gameObjectVariable") == GO("compareTo"); setb(VAR("storeResult"), eq); ev(f, EV(eq ? "equalEvent" : "notEqualEvent")); break; }
    case A_iTweenScaleTo: case A_iTweenRotateTo: case A_iTweenRotateBy: {
        o = GO("gameObject"); if (!o) break;
        a->t0 += dt; if (a->t0 < 0) break;
        float time = NONE("time") ? 1 : PF("time", 1), t = time > 0 ? a->t0 / time : 1; if (t > 1) t = 1;
        float e = ease(PI("easeType", 21), t);
        if (a->type == A_iTweenScaleTo) { o->sx = a->f[0] + (a->f[2] - a->f[0]) * e; o->sy = a->f[1] + (a->f[3] - a->f[1]) * e; }
        else o->rz = a->f[0] + (a->f[2] - a->f[0]) * e;
        if (t >= 1) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_Rotate: if (!PB("lateUpdate", false) && !PB("fixedUpdate", false)) act_late(f, a, -dt); break;
    case A_ListenForUp: case A_ListenForDown: case A_ListenForLeft: case A_ListenForRight: case A_ListenForJump:
    case A_ListenForAttack: case A_ListenForCast: case A_ListenForDash: case A_ListenForMenuSubmit: case A_ListenForMenuCancel:
    case A_ListenForDreamNail: case A_ListenForQuickMap: case A_ListenForInventory: case A_ListenForSuperdash: {
        if (game_is_paused()) break;
        bool pr = false, held = false, rel = false;
        switch (a->type) {
            case A_ListenForUp: pr = gin.upP; held = gin.upH; break;
            case A_ListenForDown: pr = gin.downP; held = gin.downH; break;
            case A_ListenForLeft: pr = gin.leftP; held = gin.leftH; break;
            case A_ListenForRight: pr = gin.rightP; held = gin.rightH; break;
            case A_ListenForJump: pr = gin.jumpP; held = gin.jumpH; break;
            case A_ListenForAttack: pr = gin.attackP; held = gin.attackH; break;
            case A_ListenForCast: pr = gin.castP; held = gin.castH; break;
            case A_ListenForDash: pr = gin.dashP; held = gin.dashH; break;
            case A_ListenForMenuSubmit: pr = gin.submitP; break;
            case A_ListenForMenuCancel: pr = gin.cancelP; break;
            case A_ListenForDreamNail: pr = gin.dreamNailP; held = gin.dreamNailH; break;
            case A_ListenForQuickMap: pr = gin.quickMapP; break;
            case A_ListenForInventory: pr = gin.inventoryP; break;
            case A_ListenForSuperdash: pr = gin.superDashP; break;
        }
        rel = a->b0 && !held; a->b0 = held;
        unsigned sw = f->nswitch;
        if (pr) ev(f, EV("wasPressed"));
        if (f->nswitch != sw || f->pending >= 0) break;
        if (rel) ev(f, EV("wasReleased"));
        if (held) { ev(f, EV("isPressed")); setb(VAR("isPressedBool"), true); }
        else { ev(f, EV("isNotPressed")); setb(VAR("isPressedBool"), false); }
        break; }
    case A_iTweenMoveTo: {
        o = GO("gameObject"); if (!o) break;
        a->t0 += dt; if (a->t0 < 0) break;
        float dur = NONE("time") ? 1 : PF("time", 1), t = dur > 0 ? a->t0 / dur : 1; if (t > 1) t = 1;
        float e = -0.5f * (cosf((float)M_PI * t) - 1);    // easeInOutSine-ish default
        float x = a->f[0] + (a->f[2] - a->f[0]) * e, y = a->f[1] + (a->f[3] - a->f[1]) * e;
        if (a->b1) { o->lx = x; o->ly = y; } else uo_set_world_pos(o, x, y);
        if (t >= 1) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_ListenForPromptContinue:
        if (!game_is_paused() && (gin.submitP || gin.jumpP || gin.attackP || gin.castP)) ev(f, EV("ContinuePressed"));
        break;
    case A_GetButtonDown: {
        const char *bn = PS("buttonName", ""); bool p = false;
        if (!strcmp(bn, "Jump")) p = gin.jumpP; else if (!strcmp(bn, "Attack")) p = gin.attackP; else if (!strcmp(bn, "Cast")) p = gin.castP;
        else if (!strcmp(bn, "Dash")) p = gin.dashP; else if (!strcmp(bn, "Submit")) p = gin.submitP; else if (!strcmp(bn, "Cancel")) p = gin.cancelP;
        if (p) { setb(VAR("storeResult"), true); ev(f, EV("sendEvent")); }
        break; }
    case A_IntCompareToBool: {
        int x = PI("integer1", 0), y = PI("integer2", 0);
        if (!NONE("equalBool")) setb(VAR("equalBool"), x == y);
        if (!NONE("lessThanBool")) setb(VAR("lessThanBool"), x < y);
        if (!NONE("greaterThanBool")) setb(VAR("greaterThanBool"), x > y);
        break; }
    case A_GetPosition2D: { o = GO("gameObject"); if (o) { setf(VAR("x"), uo_wx(o)); setf(VAR("y"), uo_wy(o)); } break; }
    case A_SetMaterialColor: case A_SetTextMeshProColor: case A_SetSpriteRendererColor: {   // the renderer's tint (fades)
        float c[4]; o = GO("gameObject");
        if (o && pcolor(f, a, "color", c)) { o->tintR = c[0]; o->tintG = c[1]; o->tintB = c[2]; o->tintA = c[3]; }
        break; }
    case A_GetMaterialColor: case A_Tk2dSpriteGetColor: { o = GO("gameObject"); if (o) { float c[4] = { o->tintR, o->tintG, o->tintB, o->tintA }; store_color(VAR("color"), c); } break; }
    case A_SetColorValue: { float c[4]; if (pcolor(f, a, "color", c)) store_color(VAR("colorVariable"), c); break; }
    case A_GetColorRGBA: { float c[4]; if (pcolor(f, a, "color", c)) { setf(VAR("storeRed"), c[0]); setf(VAR("storeGreen"), c[1]); setf(VAR("storeBlue"), c[2]); setf(VAR("storeAlpha"), c[3]); } break; }
    case A_iTweenShakePosition: {
        o = a->o0; a->t0 += dt;
        float ax, ay, az; if (!pvec(f, a, "vector", &ax, &ay, &az)) { ax = ay = 0.1f; }
        if (alive(o)) { if (a->t0 < a->t1) { o->lx = a->f[0] + frand(-ax, ax); o->ly = a->f[1] + frand(-ay, ay); } else { o->lx = a->f[0]; o->ly = a->f[1]; } }
        if (a->t0 >= a->t1) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_GetAxis: { const char *n = PS("axisName", ""); float v = 0;
        if (!strcasecmp(n, "Horizontal")) v = (gin.rightH ? 1 : 0) - (gin.leftH ? 1 : 0);
        else if (!strcasecmp(n, "Vertical")) v = (gin.upH ? 1 : 0) - (gin.downH ? 1 : 0);
        if (!NONE("multiplier")) v *= PF("multiplier", 1);
        setf(VAR("store"), v); break; }
    case A_ColorInterpolate: {
        JVal *cs = praw(a, "colors"); int n = cs ? cs->n : 0; FVar *sv = VAR("storeColor"); float T = PF("time", 1);
        a->t0 += dt;
        float cols[8][4]; int m = n > 8 ? 8 : n;
        for (int k = 0; k < m; k++) { JVal *e = jat(cs, k); for (int i = 0; i < 4; i++) cols[k][i] = 1;
            if (e && e->t == J_ARR) for (int i = 0; i < 4; i++) cols[k][i] = (float)jnum(jat(e, i), 1);
            else if (e && e->t == J_OBJ && jget(e, "var")) { FVar *x = fsm_var_j(f, jget(e, "var")); if (x) for (int i = 0; i < 4; i++) cols[k][i] = x->v[i]; } }
        if (a->t0 > T) { if (sv) store_color(sv, cols[m - 1]); FIN(); ev(f, EV("finishEvent")); break; }
        float num = (m - 1) * a->t0 / (T > 0 ? T : 1); int i0 = (int)floorf(num), i1 = (int)ceilf(num); if (i1 >= m) i1 = m - 1; float fr = num - i0;
        float c[4]; for (int i = 0; i < 4; i++) c[i] = cols[i0][i] + (cols[i1][i] - cols[i0][i]) * fr;
        if (sv) store_color(sv, c);
        break; }
    case A_RotateTo: {
        o = GO("gameObject"); if (!o) break;
        float tgt = PF("targetAngle", 0), sp = PF("speed", 0), cur = norm360(o->rz);
        float d = tgt - cur;
        if ((d < 0) ? (d < -180) : !(d > 180)) { o->rz = cur + sp * dt; if (norm360(o->rz) > tgt && o->rz - sp * dt <= tgt) o->rz = tgt; }
        else { o->rz = cur - sp * dt; if (norm360(o->rz) < tgt) o->rz = tgt; }
        break; }
    case A_iTweenFadeTo: case A_iTweenFadeFrom: {
        o = a->o0; if (!alive(o)) { FIN(); break; }
        a->t0 += dt; if (a->t0 < 0) break;
        float T = NONE("time") ? 1 : PF("time", 1), t = T > 0 ? a->t0 / T : 1; if (t > 1) t = 1;
        float v = a->f[0] + (a->f[1] - a->f[0]) * ease(PI("easeType", 21), t);
        bool kids = NONE("includeChildren") || PB("includeChildren", true);
        o->tintA = v;
        if (kids) { UObj *st[64]; int n = 0; for (UObj *c = o->child; c && n < 64; c = c->next) st[n++] = c;
            while (n > 0) { UObj *c = st[--n]; c->tintA = v; for (UObj *d = c->child; d && n < 64; d = d->next) st[n++] = d; } }
        if (t >= 1) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_ScaleTo: {
        o = GO("gameObject"); if (!o) break;
        a->t0 += dt;
        float dl = PF("delay", 0), T = a->b0 ? (NONE("time") ? 1 : PF("time", 1)) : PF("duration", 1);
        float t = T > 0 ? (a->t0 - dl) / T : 1; if (t < 0) t = 0; if (t > 1) t = 1;
        float e = a->b0 ? ease(PI("easeType", 21), t) : t;
        o->sx = a->f[0] + (a->f[2] - a->f[0]) * e; o->sy = a->f[1] + (a->f[3] - a->f[1]) * e;
        if (a->t0 > T + dl) { FIN(); ev(f, EV("finishEvent")); }
        break; }
    case A_ListenForQuickCast: {
        if (gin.quickCastP) ev(f, EV("wasPressed"));
        if (gin.quickCastR) ev(f, EV("wasReleased"));
        if (gin.quickCastH) ev(f, EV("isPressed")); else ev(f, EV("isNotPressed"));
        break; }
    case A_FindClosest: {
        o = GO("gameObject"); if (!o) break;
        const char *tg = PS("withTag", ""); bool ign = PB("ignoreOwner", true);
        UObj *best = NULL; float bd = 1e18f, ox = uo_wx(o), oy = uo_wy(o);
        for (int i = 0; i < uo_count(); i++) { UObj *q = uo_obj(i); if (!q || q->destroyed || !uo_active(q) || q->pseudo) continue;
            if (tg[0] && strcmp(tg, "Untagged") && strcmp(q->tag, tg)) continue; if (ign && q == f->owner) continue;
            float dx = uo_wx(q) - ox, dy = uo_wy(q) - oy, d = dx * dx + dy * dy; if (d < bd) { bd = d; best = q; } }
        setgo(VAR("storeObject"), best); if (!NONE("storeDistance")) setf(VAR("storeDistance"), best ? sqrtf(bd) : 0);
        break; }
    case A_SpawnObjectFromGlobalPoolOverTime: {
        a->t0 += dt; if (a->t0 < PF("frequency", 1)) break; a->t0 = 0;
        int ti = tmpl_of(f, a, "gameObject"); if (ti < 0) break;
        float x = 0, y = 0, z = 0; UObj *sp = GO("spawnPoint"); float px, py, pz;
        if (sp) { x = uo_wx(sp); y = uo_wy(sp); } if (!NONE("position") && pvec(f, a, "position", &px, &py, &pz)) { x += px; y += py; }
        uo_spawn(ti, x, y, 0);
        break; }
    case A_SpawnRandomObjectsOverTime: {
        a->t0 += dt; if (a->t0 < PF("frequency", 1)) break; a->t0 = 0;
        int ti = tmpl_of(f, a, "gameObject"); if (ti < 0) break;
        float x = 0, y = 0, z = 0; UObj *sp = GO("spawnPoint");
        float px, py, pz; bool hp = !NONE("position") && pvec(f, a, "position", &px, &py, &pz);
        if (sp) { x = uo_wx(sp); y = uo_wy(sp); if (hp) { x += px; y += py; } } else if (hp) { x = px; y = py; }
        int n = irand(PI("spawnMin", 1), PI("spawnMax", 1) + 1); float ov = PF("originVariation", 0);
        for (int i = 0; i < n; i++) {
            UObj *c = uo_spawn(ti, x + frand(-ov, ov), y + frand(-ov, ov), 0); if (!c) continue;
            float s = frand(PF("speedMin", 0), PF("speedMax", 0)), an = frand(PF("angleMin", 0), PF("angleMax", 0)) * (float)M_PI / 180;
            if (c->rb.has) { c->rb.vx = s * cosf(an); c->rb.vy = s * sinf(an); }
        }
        break; }
    default: break;
    }
}

void act_fixed(Fsm *f, FsmAct *a, float dt) {
    UObj *o;
    switch (a->type) {
    case A_IdleBuzzV2: case A_IdleBuzzV3: do_idle_buzz_v(f, a, a->type == A_IdleBuzzV3); break;
    case A_ChaseObjectGround: do_chase_ground(f, a, false); break;
    case A_RunAway: do_chase_ground(f, a, true); break;
    case A_DistanceFlyV2: do_distance_fly2(f, a); break;
    case A_DistanceFlySmooth: do_distance_fly_smooth(f, a); break;
    case A_DistanceWalk: do_distance_walk(f, a); break;
    case A_AddForce2dAsAngle: do_add_force_angle(f, a); break;
    case A_FaceAngleV2: do_face_angle_v2(f, a); break;
    case A_SetVelocity2dIfFalse: do_set_vel_if_false(f, a); break;

    case A_ChaseObjectV2: {
        o = GO("gameObject"); URb *rb = rbody(o); UObj *t = GO("target"); if (!rb || !t) break;
        float vx = uo_wx(t) + PF("offsetX", 0) - uo_wx(o), vy = uo_wy(t) + PF("offsetY", 0) - uo_wy(o);
        float m = sqrtf(vx * vx + vy * vy); if (m > 1) { vx /= m; vy /= m; }
        float acc = PF("accelerationForce", 0);
        rb->vx += vx * acc * HERO_FIXED_DT; rb->vy += vy * acc * HERO_FIXED_DT;
        float sm = PF("speedMax", 0), sp = sqrtf(rb->vx * rb->vx + rb->vy * rb->vy);
        if (sp > sm && sp > 0) { rb->vx *= sm / sp; rb->vy *= sm / sp; }
        return; }
    case A_DecelerateXY: {
        o = GO("gameObject"); URb *rb = rbody(o); if (!rb) return;
        if (!NONE("decelerationX")) { float d = PF("decelerationX", 1); rb->vx *= d; if (fabsf(rb->vx) < 0.001f) rb->vx = 0; }
        if (!NONE("decelerationY")) { float d = PF("decelerationY", 1); rb->vy *= d; if (fabsf(rb->vy) < 0.001f) rb->vy = 0; }
        return; }
    default: break;
    }
    switch (a->type) {
    case A_SetVelocity2d: do_set_velocity(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetVelocityAsAngle: do_velocity_angle(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_AddForce2dV2: do_add_force(f, a); break;
    case A_Decelerate: do_decel(f, a, false); break;
    case A_DecelerateV2: do_decel(f, a, true); break;
    case A_IdleBuzz: do_idle_buzz(f, a); break;
    case A_DistanceFly: do_distance_fly(f, a); break;
    case A_ChaseObject: do_chase(f, a); break;
    case A_FireAtTarget: do_fire_at(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_ObjectJitter: {
        o = GO("gameObject"); if (!o) break;
        float jx = NONE("x") ? 0 : PF("x", 0), jy = NONE("y") ? 0 : PF("y", 0);
        if (PB("allowMovement", false)) uo_set_world_pos(o, uo_wx(o) + frand(-jx, jx), uo_wy(o) + frand(-jy, jy));
        else uo_set_world_pos(o, a->f[0] + frand(-jx, jx), a->f[1] + frand(-jy, jy));
        break; }
    case A_Translate: case A_TranslateV2: if (PB("fixedUpdate", false)) do_translate(f, a, dt); if (!PB("everyFrame", true)) FIN(); break;
    case A_FloatDivideV2: if (PB("fixedUpdate", false)) { FVar *v = VAR("floatVariable"); float d = PF("divideBy", 1); if (v && d != 0) var_set_float(v, v->f / d); } break;
    case A_FloatAddV2:
        if (PB("fixedUpdate", false) && (NONE("activeBool") || PB("activeBool", false))) { FVar *v = VAR("floatVariable"); if (v) v->f += PB("perSecond", false) ? PF("add", 0) * dt : PF("add", 0); }
        break;
    case A_FloatMultiplyV2: if (PB("fixedUpdate", false)) { FVar *v = VAR("floatVariable"); if (v) v->f *= PF("multiplyBy", 1); } break;
    case A_FaceAngle: { o = GO("gameObject"); URb *rb = rbody(o); if (rb) o->rz = atan2f(rb->vy, rb->vx) * 180 / (float)M_PI + PF("angleOffset", 0); break; }
    case A_AccelerateVelocity: {
        URb *rb = rbody(GO("gameObject")); if (!rb) break;
        if (!NONE("xAccel")) { float v = rb->vx + PF("xAccel", 0), m = PF("xMaxSpeed", 0); rb->vx = v < -m ? -m : v > m ? m : v; }
        if (!NONE("yAccel")) { float v = rb->vy + PF("yAccel", 0), m = PF("yMaxSpeed", 0); rb->vy = v < -m ? -m : v > m ? m : v; }
        break; }
    case A_ProjectileSquash: {
        o = GO("gameObject"); URb *rb = rbody(o); if (!rb) break;
        float sp = sqrtf(rb->vx * rb->vx + rb->vy * rb->vy), k = PF("stretchFactor", 1.4f);
        float sy = 1 - sp * k * 0.01f, sx = 1 + sp * k * 0.01f;
        if (sx < 0.5f) sx = 0.5f; if (sy > 2) sy = 2;
        float m = PF("scaleModifier", 1); o->sx = sx * m; o->sy = sy * m; break; }
    case A_RandomFloatV2: setf(VAR("storeResult"), frand(PF("min", 0), PF("max", 1))); break;
    case A_IntAddV2: { FVar *v = VAR("intVariable"); if (v) v->i += PI("add", 0); break; }
    case A_AddForce2d: do_add_force(f, a); break;
    case A_WalkLeftRight: {
        o = a->o0; if (!alive(o)) break;
        URb *rb = rbody(o);
        float dir = (o->sx >= 0 ? 1.0f : -1.0f) * (PB("spriteFacesLeft", false) ? -1.0f : 1.0f);
        if (a->i0 == 1) {       // Turn coroutine
            a->t0 -= dt;
            if (a->t0 <= 0) { o->sx = -o->sx; if (o->an.has) uo_anim_play(o, PS("walkAnimName", NULL), false); a->i0 = 0; }
            break;
        }
        if (!rb) break;
        rb->vx = PF("walkSpeed", 4) * dir;
        float x0, y0, x1, y1;
        bool turn = a->b0;
        if (!turn && bounds(o, &x0, &y0, &x1, &y1)) {
            float cx = (x0 + x1) / 2, w = x1 - x0, by = y0 + 0.5f;
            bool grounded = ray(cx, by, 0, -1, 1);
            bool wall = ray(cx, by, dir, 0, w / 2 + 0.1f);
            bool floor = !ray(cx + (w / 2 + 0.1f) * dir, by, 0, -1, 1);
            turn = grounded && (wall || floor) && uo_time >= a->t1;
        }
        if (turn) {
            a->b0 = false; a->t1 = uo_time + PF("turnDelay", 1);
            rb->vx = 0;
            const char *tc = PS("turnAnimName", NULL);
            float len = uo_anim_clip_len(o, tc);
            if (len > 0) { uo_anim_play(o, tc, false); a->t0 = len; } else a->t0 = 0;
            a->i0 = 1;
            if (a->t0 <= 0) { o->sx = -o->sx; if (o->an.has) uo_anim_play(o, PS("walkAnimName", NULL), false); a->i0 = 0; }
        }
        break; }
    case A_CheckTrackTriggerCount: { UObj *t = GO("target"); if (PB("everyFrame", true) && t && track_test(f, a, t)) ev(f, EV("successEvent")); break; }
    default: break;
    }
}

void act_late(Fsm *f, FsmAct *a, float dt) {
    switch (a->type) {
    case A_GetAngleToTarget: {     // Vector3.Angle(target - self, transform.forward): 2D objects face +z, so 90 unless the target is at z
        UObj *o = GO("gameObject"); UObj *t = GO("targetObject"); if (!o) break;
        float tx, ty, tz = 0; if (t) { tx = uo_wx(t); ty = uo_wy(t); } else if (!pvec(f, a, "targetPosition", &tx, &ty, &tz)) break;
        if (PB("ignoreHeight", true)) ty = uo_wy(o);
        float dx = tx - uo_wx(o), dy = ty - uo_wy(o), m = sqrtf(dx * dx + dy * dy + tz * tz);
        setf(VAR("storeAngle"), m > 0 ? acosf(tz / m) * 180 / (float)M_PI : 0);
        if (!PB("everyFrame", false)) FIN();
        break; }
    case A_SetScale: if (PB("lateUpdate", false)) do_set_scale(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetPosition: if (PB("lateUpdate", false)) do_set_position(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_SetRotation: if (PB("lateUpdate", false)) do_set_rotation(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_FlipScale: if (PB("lateUpdate", false)) do_flip(f, a); if (!PB("everyFrame", false)) FIN(); break;
    case A_Translate: case A_TranslateV2: if (PB("lateUpdate", false)) do_translate(f, a, dt); if (!PB("everyFrame", true)) FIN(); break;
    case A_Rotate: {    // dt < 0: called from OnUpdate with -dt
        bool fromUpdate = dt < 0; float d = fabsf(dt);
        if (!fromUpdate && !PB("lateUpdate", false) && dt != 0) break;
        UObj *o = GO("gameObject"); if (!o) break;
        float z = 0, vx, vy, vz;
        if (!NONE("vector") && pvec(f, a, "vector", &vx, &vy, &vz)) z = vz;
        if (!NONE("zAngle")) z = PF("zAngle", 0);
        if (PB("perSecond", false)) z *= d;
        o->rz += z;
        if (!fromUpdate && dt != 0 && !PB("everyFrame", false)) FIN();
        break; }
    default: break;
    }
}

void act_exit(Fsm *f, FsmAct *a) {
    switch (a->type) {
    case A_ActivateGameObject:
        if (a->o0 && PB("resetOnExit", false)) { bool act = !PB("activate", true); if (PB("recursive", true)) do_activate(f, a, a->o0, act, true); else uo_set_active(a->o0, act); }
        break;
    case A_iTweenMoveBy: case A_iTweenScaleBy: break;
    case A_ForceHeroFootstepSound: if (PB("endOnStateExit", false)) P.forceFootstep = 0; break;
    case A_PlayParticleEmitterInState: { UObj *o = GO("gameObject"); if (o) particles_obj_play(o, false); break; }   // stops when the state ends
    default: break;
    }
}
