// The Knight: a port of Hollow Knight's HeroController + HeroAnimationController + NailSlash (decompiled
// Assembly-CSharp), running on the room's collision grid instead of Box2D.
// Update() runs every rendered frame, FixedUpdate() + the physics step at the game's fixed 50 Hz.
#pragma once
#include <stdbool.h>
#include "gfx.h"

#define HERO_FIXED_DT 0.02f

// Knight collider (BoxCollider2D on the Knight prefab): 0.5 x 1.28125, offset (0, -0.75)
#define HALF_W    0.25f
#define HALF_H    0.640625f
#define COL_OFF_Y (-0.75f)
// HeroBox: the damage hurtbox (0.455 x 1.170, offset -0.694)
#define HURT_HW   0.2277f
#define HURT_HH   0.5849f
#define HURT_OY   (-0.6943f)

typedef enum { AS_INACTIVE, AS_IDLE, AS_RUNNING, AS_AIRBORNE, AS_HARD_LANDING, AS_DASH_LANDING, AS_NO_INPUT, AS_PREVIOUS, AS_GROUNDED } ActorState;
typedef enum { TS_WAITING_TO_TRANSITION, TS_EXITING_SCENE, TS_WAITING_TO_ENTER_LEVEL, TS_ENTERING_SCENE, TS_DROPPING_DOWN } TransState;
typedef enum { GP_TOP, GP_RIGHT, GP_LEFT, GP_BOTTOM, GP_DOOR, GP_UNKNOWN } GatePos;
typedef enum { CS_TOP, CS_LEFT, CS_RIGHT, CS_BOTTOM, CS_OTHER } CollSide;

typedef struct {           // HeroControllerStates (the subset the demake uses)
    bool facingRight, onGround, jumping, wallJumping, doubleJumping, dashing, backDashing, attacking, upAttacking,
         downAttacking, altAttack, lookingUp, lookingDown, lookingUpAnim, lookingDownAnim, falling, willHardLand,
         recoilFrozen, recoiling, recoilingLeft, recoilingRight, invulnerable, casting, castRecoiling, bouncing,
         shroomBouncing, wallSliding, touchingWall, touchingNonSlider, hazardDeath, hazardRespawning, dead,
         transitioning, focusing, wasOnGround, nailCharging, shadowDashing, preventDash, dashCooldown, spellQuake,
         freezeCharge, inWalkZone, superDashing, superDashOnWall, swimming, inAcid,
         onConveyor, onConveyorV, inConveyorZone, slidingLeft, slidingRight;
} CState;

typedef struct {           // one frame of input, already mapped to HK actions
    float mx, my;                      // move vector (-1..1)
    bool leftH, rightH, upH, downH, downP;
    bool jumpP, jumpH, jumpR;
    bool atkP, atkH;
    bool dashP, dashH;
    bool castP, focusH;
} HeroInput;

typedef struct {
    // transform / rigidbody
    float x, y, vx, vy;
    float px, py;                      // position before the last physics step (interpolation)
    float gravityScale, prevGravityScale;
    int facing;                        // +1 right, -1 left (mirrors cState.facingRight)
    bool onGround;                     // mirrors cState.onGround (old name used around game.c)
    CState c;
    ActorState state, prevState;
    TransState trans; GatePos gatePos; float transVx, transVy; bool stopWalkingOut;
    float enterT; int enterPhase; float enterHazX, enterHazY;
    // HeroController private state
    float move_input, vertical_input;
    int jump_steps, jumped_steps, doubleJump_steps;
    float dash_timer, attack_time, attack_cooldown, attackDuration, altAttackTime;
    float bounceTimer, recoilTimer, lookDelayTimer, fallTimer, hardLandingTimer, dashLandingTimer;
    float hardLandFailSafeTimer, floatingBufferTimer, hazardDeathTimer;
    int recoilSteps; bool recoilLarge; float recoilVx, recoilVy;
    int landingBufferSteps, ledgeBufferSteps, headBumpSteps, jumpReleaseQueueSteps;
    int jumpQueueSteps, doubleJumpQueueSteps, dashQueueSteps, attackQueueSteps;
    bool jumpQueuing, doubleJumpQueuing, dashQueuing, attackQueuing, jumpReleaseQueuing;
    bool wallLocked, wallJumpedL, wallJumpedR; int wallLockSteps, wallUnstickSteps;
    float currentWalljumpSpeed, walljumpSpeedDecel;
    bool touchingWallL, touchingWallR, wallSlidingL, wallSlidingR;
    bool airDashed, doubleJumped, dashingDown, hardLanded;
    float dashCooldownTimer, shadowDashTimer;
    bool acceptingInput, controlRelinquished;
    float invulFreeze, invulDuration; bool invulRoutine;
    float freezeT;                     // damage freeze (GameManager.FreezeMoment)
    float timeSinceLevelLoad;
    int contacts;                      // terrain contact sides last physics step (bit per CollSide)
    // NailSlash state
    bool slashing, slashPoly; int slashStep, slashPolyCount; bool slashAnimDone; float slashAngle;
    int swing;                         // increments every swing; targets remember the last swing that hit them
    int attackDir;                     // 0 side, 1 up, 2 down (old name used around game.c)
    int slashKind;                     // which NailSlash object swung: 0 Slash, 1 AltSlash, 2 UpSlash, 3 DownSlash, 4 WallSlash
    // HeroAnimationController
    bool playLanding, playRunToIdle, playDashToIdle, wasAttacking, wasFacingRight, setEntryAnim, animWaitingToEnter;
    ActorState animState;
    // game-side state (PlayerData and demake extras)
    int health, maxHealth, soul, geo;
    bool hasDash, hasWalljump, hasDoubleJump;
    bool dead; float deadTimer;
    float hazardTimer;                 // hazard death -> respawn delay
    bool sitting;
    bool animCtrlStopped;              // HeroAnimationController.StopControl (FSMs animate the Knight)
    int forceFootstep;                 // ForceHeroFootstepSound (0 off)
    float focusTimer; bool focusing;
    AnimPlayer anim, slashFx;
    float stepTimer;
    HeroInput in;                      // last input (FixedUpdate reads IsPressed from it)
    // MP drain (focus), parry / quake invulnerability, damage mode (0 full, 1 hazards only, 2 none), nail art charge
    bool drainMP; float drainMPTimer, drainMPTime; int mpDrained, focusMPAmount;
    float conveyorSpeed, conveyorSpeedV; bool convGravityOff;
    float parryInvulnTimer; int damageMode;
    float nailChargeTimer, nailChargeTime; bool nailCharged;
} Player;

extern Player P;

void hero_reset(void);                                  // new game / fresh state
void hero_update(float dt, const HeroInput *in);        // Update()
void hero_footsteps_stop(void);        // silence the run / walk loop now
void hero_footsteps_end_frame(void);   // once a game frame: silences it after a frame the Knight didn't update
void hero_fixed(void);                                  // FixedUpdate() + physics step + collision callbacks
void hero_take_damage(float fromX, int amount, int hazardType);   // hazardType: 1 enemy, 2 spikes, 3 acid, 5 pit
void hero_hazard_respawn(float x, float y, bool facingRight);
void hero_relinquish_control(void);
void hero_stop_anim_control(void);
void hero_start_anim_control(void);
bool hero_can_talk(void);                  // HeroController.CanTalk
bool hero_can_input(void);
bool hero_get_state(const char *name);
void hero_anim_play(const char *clip, bool restart);        // the Knight's tk2dSpriteAnimator.Play (from FSMs)
void hero_anim_play_from(const char *clip, int frame);      // NULL clip: the current one
bool hero_anim_is(const char *clip);
float hero_anim_clip_len(const char *clip);
void hero_face_left(void);
void hero_affected_by_gravity(bool on);
void hero_face_right(void);
void hero_regain_control(void);
void hero_leave_scene(GatePos side);     // HeroController.LeaveScene (an exit fired)
void hero_enter_scene(GatePos pos, float gx, float gy, float ox, float oy, int flags);   // start the EnterScene sequence
void hero_set_position(float x, float y);
bool hero_check_touching_ground(void);
void hero_face(bool right);
// NailSlash.OnTriggerEnter2D results reported by the game when the slash overlaps something
void hero_slash_hit_enemy(void);          // layer 11 (recoil / pogo)
void hero_slash_hit_bouncer(void);        // spikes "Tink", layer 17/19 objects you can pogo off (down slash only)
void hero_bounce(void);
void hero_set_sliding(bool left, bool right, bool on);    // SteepSlope contact
void hero_sharp_shadow(bool on);                 // uo.c: the Knight's Sharp Shadow child (damages enemies while shadow dashing)
void hero_slash_hit_shroom(void);               // the nail hit a bounce shroom: ShroomBounce / long recoil
void hero_shroom_recoil(void);                  // BounceShroom top contact: Bounce + a random RecoilLeft/Right
void hero_conveyor(int kind, bool on, float speed, bool vertical);   // kind 0 ConveyorBelt, 1 ConveyorZone, 2 ConveyorSpeedZone
void hero_recoil_left(void);
void hero_recoil_right(void);
void hero_recoil_down(void);
void hero_nail_box(float *x0, float *y0, float *x1, float *y1);
void hero_slash_xf(float *ox, float *oy, float *sx, float *sy);   // slash effect: offset from the Knight (world units) and draw scale
// HeroController methods for the Knight's FSMs (Spell Control, Superdash, Nail Arts, Dream Nail...)
void hero_proxy_event(const char *ev);          // ProxyFSM "HeroCtrl-..." events
void hero_set_cstate(const char *name, bool v);
bool hero_can_cast(void);
bool hero_can_focus(void);
bool hero_can_nail_art(void);
bool hero_can_quick_map(void);
bool hero_can_superdash(void);
bool hero_can_dream_nail(void);
void hero_take_mp(int n);
void hero_start_mp_drain(float t);
void hero_stop_mp_drain(void);
void hero_quake_invuln(void);
void hero_reset_air_moves(void);
void hero_set_damage_mode(int m);
void hero_reset_hard_landing(void);
void hero_cancel_attack(void);
void hero_flip_sprite(void);
void hero_set_mp(int v);
void hero_anim_stop(void);
bool hero_slash_active(void);

// game.c hooks the hero calls
void game_hero_dead(void);
void game_hero_hazard_death(void);
void game_shake(float t);
void game_take_geo(int n);           // HeroController.TakeGeo (with the HUD counter)
void game_add_geo(int n);
void game_add_health(int n);
void game_max_health(void);
void game_add_max_health(int n);
void game_charm_update(void);
int  game_absorb_blue(int amount);
void game_add_soul(int n);
void game_hud_out(bool out);         // HUD Canvas OUT / IN (dialogue)
bool game_is_paused(void);
void game_set_bench_respawn(void);    // HeroController.SetBenchRespawn: the nearest bench / respawn marker
void game_load_level(const char *name);   // LoadLevel (Cinematic_Stag_travel -> stag trip)
void game_camera_fade(bool out, bool instant);
void game_begin_transition(const char *scene, const char *gate);   // GameManager.BeginSceneTransition
void game_freeze(float t);
void game_music_cue(const char *guid, float delay, float transition);
void game_audio_snapshot(const char *mixer, const char *snapshot, float transition);
const char *game_map_zone(void);
void game_save_now(void);
bool game_nonslider(float x0, float y0, float x1, float y1);
int  game_darkness_level(void);
void game_add_blue(int n);
void game_cam_shake(int kind);
void game_global_event(const char *ev);
void game_load_level(const char *name);   // NonSlider walls of this room
const char *game_entry_gate(void);
extern bool g_testGod;
