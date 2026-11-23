#pragma once
#include <3ds.h>
#include <stdbool.h>

bool audio_init(void);
void audio_exit(void);
// streamed ogg (romfs:/music/<name>.ogg). slot 0 = music, slot 1 = ambience
void audio_play_stream(int slot, const char *name, bool loop, float fadeIn);
void audio_stop_stream(int slot, float fadeOut);
void audio_set_stream_volume(int slot, float v);
void audio_fade_stream(int slot, float v, float seconds);   // to volume v (0..1) over the given time; it keeps playing
void audio_update(float dt);
// one-shot effects (romfs:/sfx/<name>.pcm, 22050 Hz mono s16)
int  sfx_play(const char *name, float volume);              // returns a handle for sfx_stop (-1: not played)
int  sfx_play_pitch(const char *name, float volume, float pitch);
void sfx_stop(int handle);
void sfx_preload(const char *name);                        // load now so the first play isn't delayed by the file read
float sfx_length(const char *name);     // seconds, 0 if the sound isn't there
// looping sounds (object AudioSources with Loop): returns a slot (-1: none free / no sound)
int  sfx_loop_start(const char *name, float volume, float pitch);
void sfx_loop_volume(int slot, float volume);
void sfx_loop_stop(int slot);
bool sfx_loop_playing(int slot);   // false after the system paused audio (sleep / HOME)
extern bool g_sfxMute;
extern float g_musicVolume, g_sfxVolume;
