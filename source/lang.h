#pragma once
#include <stdbool.h>
enum { LANG_EN, LANG_ES, LANG_COUNT };
extern int g_lang;
const char *lang_get(const char *key, const char *sheet);          // NULL if missing
const char *lang_get_or_key(const char *key, const char *sheet);   // the key itself if missing (like Language.Get)
const char *lang_get_en(const char *key, const char *sheet);       // the English text whatever the language
const char *lang_port(const char *en);     // the chosen language's version of a line the port draws itself
#define TR(s) lang_port(s)
void lang_path(const char *name, char *out, int n);                // romfs path of a text file in the chosen language
void lang_set(int lang);                   // switch language: the old text is freed at the next lang_gc()
void lang_gc(void);                        // free the switched-away language (call where no old text is on show)
const char *lang_name(int lang);           // "English" / "Español"
extern bool g_hints;                       // the button hints at the bottom of the screens (Options: Button Hints)
extern bool g_lowEffects;                  // reduce vignette/darkness/overdraw effects
extern bool g_lowParticles;                // reduce particle emitter and alive budgets
void settings_load(void);                  // sdmc:/3ds/hk3ds/settings.txt: language, volumes, button hints
void settings_save(void);
