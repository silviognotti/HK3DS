// Audio: ogg music/ambience streamed with stb_vorbis on a worker thread, PCM sound effects via NDSP.
#include "audio.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "ext/stb_vorbis.c"

float g_musicVolume = 0.8f, g_sfxVolume = 1.0f;
bool g_sfxMute = false;       // set while the game runs silent physics simulations

static bool ndspOk;
#ifdef HK_HD
static u32 SFX_BUDGET = 8u * 1024 * 1024;        // HD build: the art's bigger textures take part of the sounds' share
#else
static u32 SFX_BUDGET = 14u * 1024 * 1024;
#endif       // sound effects kept in linear memory at once (the rest is evicted LRU); 4 MB on an Old 3DS
#define STREAM_BUFS 4
#define STREAM_SAMPLES 4096
#define SFX_CH0 4
#define SFX_CHN 15
#define LOOP_CH0 (SFX_CH0 + SFX_CHN)    // 5 looping channels: up to 4 object sounds (uo.c: Myla's humming, machinery, water...)
#define LOOP_CHN 5                      // and the Knight's run / walk footsteps (hero.c)
static ndspWaveBuf loopwb[LOOP_CHN]; static bool loopOn[LOOP_CHN];

typedef struct {
    int ch;
    stb_vorbis *v;
    u8 *file;
    int channels, rate;
    bool loop, active, stopping;
    float vol, target, fadeRate;
    ndspWaveBuf wb[STREAM_BUFS];
    s16 *pcm;
    char name[32];
    LightLock lock;
} Stream;

static Stream streams[2];
static Thread worker;
static volatile bool workerRun;
static volatile bool audioPaused;     // HOME menu / sleep: stop feeding the DSP until we're back
static aptHookCookie aptCookie;
static LightEvent workEvent;

static void stream_fill(Stream *s, ndspWaveBuf *b) {
    int got = 0;
    s16 *out = (s16 *)b->data_pcm16;
    while (got < STREAM_SAMPLES) {
        int n = stb_vorbis_get_samples_short_interleaved(s->v, s->channels, out + got * s->channels, (STREAM_SAMPLES - got) * s->channels);
        if (n <= 0) {
            if (s->loop) { stb_vorbis_seek_start(s->v); continue; }
            break;
        }
        got += n;
    }
    if (got == 0) { s->active = false; return; }
    b->nsamples = got;
    DSP_FlushDataCache(b->data_pcm16, got * s->channels * 2);
    ndspChnWaveBufAdd(s->ch, b);
}

static void stream_close(Stream *s) {
    if (s->ch >= 0 && ndspOk) ndspChnWaveBufClear(s->ch);
    if (s->v) stb_vorbis_close(s->v);
    s->v = NULL;
    free(s->file); s->file = NULL;
    s->active = false;
}

static void worker_main(void *arg) {
    (void)arg;
    while (workerRun) {
        for (int i = 0; i < 2; i++) {
            Stream *s = &streams[i];
            LightLock_Lock(&s->lock);
            if (s->active && s->v && !audioPaused) {
                for (int b = 0; b < STREAM_BUFS; b++)
                    if (s->wb[b].status == NDSP_WBUF_DONE || s->wb[b].status == NDSP_WBUF_FREE) stream_fill(s, &s->wb[b]);
            }
            LightLock_Unlock(&s->lock);
        }
        LightEvent_Wait(&workEvent);
    }
}

static void ndsp_callback(void *data) { (void)data; LightEvent_Signal(&workEvent); }

// Suspend (HOME menu) and sleep (lid closed): drop everything queued on our channels and stop refilling, so nothing
// touches the DSP while the system tears it down and brings it back; music picks up where it was afterwards.
static void audio_pause(bool pause) {
    if (!ndspOk) return;
    if (pause) {
        audioPaused = true;
        for (int i = 0; i < 2; i++) {
            Stream *s = &streams[i];
            LightLock_Lock(&s->lock);
            ndspChnWaveBufClear(s->ch);
            for (int b = 0; b < STREAM_BUFS; b++) s->wb[b].status = NDSP_WBUF_FREE;
            LightLock_Unlock(&s->lock);
        }
        for (int c = 0; c < SFX_CHN; c++) ndspChnWaveBufClear(SFX_CH0 + c);
        for (int c = 0; c < LOOP_CHN; c++) { ndspChnWaveBufClear(LOOP_CH0 + c); loopOn[c] = false; }
    } else {
        audioPaused = false;
        LightEvent_Signal(&workEvent);
    }
}

static void apt_hook(APT_HookType hook, void *param) {
    (void)param;
    switch (hook) {
        case APTHOOK_ONSUSPEND: case APTHOOK_ONSLEEP: audio_pause(true); break;
        case APTHOOK_ONRESTORE: case APTHOOK_ONWAKEUP: audio_pause(false); break;
        default: break;
    }
}

bool audio_init(void) {
#ifndef __HOST__
    if (linearSpaceFree() < (40u << 20)) SFX_BUDGET = 4u * 1024 * 1024;   // Old 3DS: GPU memory goes to the art first
#endif
    ndspOk = R_SUCCEEDED(ndspInit());
    if (!ndspOk) { printf("ndsp init failed (dspfirm.cdc missing?)\n"); return false; }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    for (int i = 0; i < 2; i++) {
        memset(&streams[i], 0, sizeof(Stream));
        streams[i].ch = i;
        LightLock_Init(&streams[i].lock);
        streams[i].pcm = linearAlloc(STREAM_BUFS * STREAM_SAMPLES * 2 * sizeof(s16));
    }
    LightEvent_Init(&workEvent, RESET_ONESHOT);
    ndspSetCallback(ndsp_callback, NULL);
    workerRun = true;
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    worker = threadCreate(worker_main, NULL, 128 * 1024, prio - 1, -2, false);
    aptHook(&aptCookie, apt_hook, NULL);
    return true;
}

void audio_exit(void) {
    if (!ndspOk) return;
    aptUnhook(&aptCookie);
    workerRun = false;
    LightEvent_Signal(&workEvent);
    if (worker) { threadJoin(worker, U64_MAX); threadFree(worker); }
    for (int i = 0; i < 2; i++) { stream_close(&streams[i]); linearFree(streams[i].pcm); }
    ndspExit();
}

static void apply_vol(Stream *s) {
    float v = s->vol * g_musicVolume;
    float mix[12] = { v, v };
    ndspChnSetMix(s->ch, mix);
}

void audio_play_stream(int slot, const char *name, bool loop, float fadeIn) {
    if (!ndspOk || slot < 0 || slot > 1) return;
    Stream *s = &streams[slot];
    if (s->active && !strcmp(s->name, name) && !s->stopping) return;
    LightLock_Lock(&s->lock);
    stream_close(s);
    if (!name || !name[0]) { LightLock_Unlock(&s->lock); return; }
    char path[96];
    snprintf(path, sizeof path, "romfs:/music/%s.ogg", name);
    size_t sz;
    s->file = read_file(path, &sz);
    if (!s->file) { LightLock_Unlock(&s->lock); return; }
    int err = 0;
    s->v = stb_vorbis_open_memory(s->file, sz, &err, NULL);
    if (!s->v) { free(s->file); s->file = NULL; LightLock_Unlock(&s->lock); return; }
    stb_vorbis_info info = stb_vorbis_get_info(s->v);
    s->channels = info.channels > 2 ? 2 : info.channels;
    s->rate = info.sample_rate;
    strncpy(s->name, name, 31);
    s->loop = loop; s->stopping = false;
    s->vol = fadeIn > 0 ? 0 : 1; s->target = 1; s->fadeRate = fadeIn > 0 ? 1.0f / fadeIn : 0;
    ndspChnReset(s->ch);
    ndspChnSetInterp(s->ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(s->ch, s->rate);
    ndspChnSetFormat(s->ch, s->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    apply_vol(s);
    memset(s->wb, 0, sizeof s->wb);
    for (int b = 0; b < STREAM_BUFS; b++) s->wb[b].data_pcm16 = s->pcm + b * STREAM_SAMPLES * 2;
    s->active = true;
    if (!audioPaused) for (int b = 0; b < STREAM_BUFS; b++) stream_fill(s, &s->wb[b]);
    LightLock_Unlock(&s->lock);
}

void audio_stop_stream(int slot, float fadeOut) {
    if (!ndspOk) return;
    Stream *s = &streams[slot];
    if (!s->active) return;
    if (fadeOut <= 0) { LightLock_Lock(&s->lock); stream_close(s); s->name[0] = 0; LightLock_Unlock(&s->lock); return; }
    s->stopping = true; s->target = 0; s->fadeRate = 1.0f / fadeOut;
}

void audio_set_stream_volume(int slot, float v) { streams[slot].target = v; if (!streams[slot].fadeRate) streams[slot].fadeRate = 2; }
void audio_fade_stream(int slot, float v, float seconds) {
    if (!ndspOk || slot < 0 || slot > 1 || streams[slot].stopping) return;
    streams[slot].target = v; streams[slot].fadeRate = seconds > 0.01f ? 1.0f / seconds : 100.0f;
}

void audio_update(float dt) {
    if (!ndspOk) return;
    for (int i = 0; i < 2; i++) {
        Stream *s = &streams[i];
        if (!s->active) continue;
        if (s->vol != s->target) {
            float d = s->fadeRate * dt;
            if (s->vol < s->target) { s->vol += d; if (s->vol > s->target) s->vol = s->target; }
            else { s->vol -= d; if (s->vol < s->target) s->vol = s->target; }
        }
        apply_vol(s);
        if (s->stopping && s->vol <= 0.001f) { LightLock_Lock(&s->lock); stream_close(s); s->name[0] = 0; LightLock_Unlock(&s->lock); }
    }
}

/* ---------------- sound effects ---------------- */
typedef struct { char name[32]; s16 *data; u32 n, bytes, lastUse; bool missing; } Sfx;
#define MAX_SFX 384
static Sfx sfx[MAX_SFX];
static int nsfx;
static u32 sfxBytes, sfxClock;
static ndspWaveBuf sfxwb[SFX_CHN];

static bool sfx_in_use(const Sfx *s) {
    for (int i = 0; i < SFX_CHN; i++) if (sfxwb[i].data_pcm16 == s->data && ndspChnIsPlaying(SFX_CH0 + i)) return true;
    for (int i = 0; i < LOOP_CHN; i++) if (loopOn[i] && loopwb[i].data_pcm16 == s->data) return true;
    return false;
}
// frees the least recently used loaded sound that isn't playing; false when there's nothing left to free
static bool sfx_evict_one(void) {
    Sfx *best = NULL;
    for (int i = 0; i < nsfx; i++) { Sfx *s = &sfx[i]; if (!s->data || sfx_in_use(s)) continue; if (!best || s->lastUse < best->lastUse) best = s; }
    if (!best) return false;
    linearFree(best->data); best->data = NULL; sfxBytes -= best->bytes; best->bytes = 0;
    return true;
}
bool sfx_free_memory(void) { return sfx_evict_one(); }    // linear memory is short (textures): drop the least recently used sound
static Sfx *sfx_get(const char *name) {
    Sfx *s = NULL;
    for (int i = 0; i < nsfx; i++) if (!strcmp(sfx[i].name, name)) { s = &sfx[i]; break; }
    if (s && s->data) { s->lastUse = ++sfxClock; return s; }
    if (s && s->missing) return NULL;
    if (!s) {
        if (nsfx < MAX_SFX) s = &sfx[nsfx++];
        else {   // table full: reuse the least recently used slot
            for (int i = 0; i < nsfx; i++) if (!sfx[i].data || !sfx_in_use(&sfx[i])) { if (!s || sfx[i].lastUse < s->lastUse) s = &sfx[i]; }
            if (!s) return NULL;
            if (s->data) { linearFree(s->data); sfxBytes -= s->bytes; }
        }
        memset(s, 0, sizeof *s); strncpy(s->name, name, 31);
    }
    char path[96]; snprintf(path, sizeof path, "romfs:/sfx/%s.pcm", name);
    size_t sz; void *buf = read_file(path, &sz);
    if (!buf) { s->missing = true; return NULL; }
    while (sfxBytes + sz > SFX_BUDGET && sfx_evict_one()) {}
    s->data = linearAlloc(sz);
    while (!s->data && sfx_evict_one()) s->data = linearAlloc(sz);      // out of linear memory: make room
    if (!s->data) { free(buf); hk_log("sfx: no memory for %s (%u bytes)", name, (unsigned)sz); return NULL; }
    memcpy(s->data, buf, sz); free(buf);
    s->n = sz / 2; s->bytes = sz; sfxBytes += sz; s->lastUse = ++sfxClock;
    DSP_FlushDataCache(s->data, sz);
    return s;
}

static int sfxGen[SFX_CHN], sfxGenCtr;
void sfx_preload(const char *name) { if (ndspOk && name) sfx_get(name); }
void sfx_stop(int h) {
    if (h < 0 || !ndspOk) return;
    int i = h & 0xff;
    if (i >= SFX_CHN || sfxGen[i] != (h >> 8)) return;      // the channel was reused for another sound
    ndspChnWaveBufClear(SFX_CH0 + i);
}
int sfx_play_pitch(const char *name, float volume, float pitch) {
#ifdef __HOST__
    { static int tr = -1; if (tr < 0) tr = getenv("HK_SNDTRACE") != NULL;
      if (tr && name) { extern int uo_frame; char p[96]; snprintf(p, sizeof p, "romfs/sfx/%s.pcm", name); FILE *t = fopen(p, "rb");
                        printf("[snd %d] %s vol %.2f pitch %.2f%s\n", uo_frame, name, volume, pitch, t ? "" : " (MISSING)"); if (t) fclose(t); } }
#endif
    if (!ndspOk || audioPaused || g_sfxMute) return -1;
    Sfx *s = sfx_get(name);
    if (!s) return -1;
    int best = -1;
    for (int i = 0; i < SFX_CHN; i++) {
        int ch = SFX_CH0 + i;
        if (!ndspChnIsPlaying(ch)) { best = i; break; }
    }
    if (best < 0) best = rand() % SFX_CHN;
    int ch = SFX_CH0 + best;
    ndspChnWaveBufClear(ch);
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, 22050.0f * pitch);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
    float v = volume * g_sfxVolume;
    float mix[12] = { v, v };
    ndspChnSetMix(ch, mix);
    memset(&sfxwb[best], 0, sizeof(ndspWaveBuf));
    sfxwb[best].data_pcm16 = s->data;
    sfxwb[best].nsamples = s->n;
    ndspChnWaveBufAdd(ch, &sfxwb[best]);
    sfxGen[best] = ++sfxGenCtr & 0xffffff;
    return best | (sfxGen[best] << 8);
}

/* looping sounds: slot 0..LOOP_CHN-1 */
#ifdef __HOST__
#define LOOP_OK true                 // the PC build has no sound; the loop bookkeeping still runs for the tests
#else
#define LOOP_OK ndspOk
#endif
int sfx_loop_start(const char *name, float volume, float pitch) {
    if (!LOOP_OK || !name) return -1;
    int i = 0; while (i < LOOP_CHN && loopOn[i]) i++;
    if (i == LOOP_CHN) return -1;
    Sfx *s = sfx_get(name); if (!s) return -1;
    int ch = LOOP_CH0 + i;
    ndspChnWaveBufClear(ch); ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR); ndspChnSetRate(ch, 22050.0f * (pitch > 0 ? pitch : 1)); ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
    float v = g_sfxMute ? 0 : volume * g_sfxVolume; float mix[12] = { v, v };
    ndspChnSetMix(ch, mix);
    memset(&loopwb[i], 0, sizeof(ndspWaveBuf));
    loopwb[i].data_pcm16 = s->data; loopwb[i].nsamples = s->n; loopwb[i].looping = true;
    ndspChnWaveBufAdd(ch, &loopwb[i]);
    loopOn[i] = true;
    return i;
}
void sfx_loop_volume(int i, float volume) {
    if (!LOOP_OK || i < 0 || i >= LOOP_CHN || !loopOn[i]) return;
    float v = g_sfxMute ? 0 : volume * g_sfxVolume; float mix[12] = { v, v };
    ndspChnSetMix(LOOP_CH0 + i, mix);
}
bool sfx_loop_playing(int i) { return LOOP_OK && i >= 0 && i < LOOP_CHN && loopOn[i]; }
void sfx_loop_stop(int i) {
    if (!LOOP_OK || i < 0 || i >= LOOP_CHN || !loopOn[i]) return;
    ndspChnWaveBufClear(LOOP_CH0 + i); loopOn[i] = false;
}

float sfx_length(const char *name) { Sfx *s = sfx_get(name); return s ? s->n / 22050.0f : 0; }
int sfx_play(const char *name, float volume) { return sfx_play_pitch(name, volume, 1.0f); }
