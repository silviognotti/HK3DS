// Language sheets (TeamCherry.Localization.Language.Get): romfs:/text/<Sheet>.txt (English) and
// romfs:/text/es/<Sheet>.txt (Spanish), one "KEY<TAB>text" per line (tools/text_sheets.py), single-byte text
// (ASCII plus the Latin-1 letters the fonts carry). Pages are '\f', line breaks '\n' (stored escaped as \f and \n).
// Only the chosen language's sheets are loaded, a few at a time, as the game asks for them; switching language
// retires the old ones, freed at the next safe point (lang_gc: nothing may still be showing their text).
// lang_port() gives the Spanish for the lines the port draws itself (romfs:/text/es/_port.txt, tools/port_text.py).
#include "lang.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int g_lang;                          // LANG_EN / LANG_ES
static const char *LDIR[LANG_COUNT] = { "", "es/" };

typedef struct { char name[24]; int lang; char *buf; char **key, **val; int n; } Sheet;
#define MAX_SHEETS 12
static Sheet sheets[MAX_SHEETS];
static int nsheets, nextEvict;
static void *retired[48]; static int nretired;      // buffers of the language switched away from

static void retire(void *p) {
    if (!p) return;
    if (nretired < 48) retired[nretired++] = p; else free(p);
}
void lang_gc(void) { while (nretired > 0) free(retired[--nretired]); }

static void parse(Sheet *s, char *buf, size_t sz) {
    s->buf = buf;
    int lines = 1; for (size_t i = 0; i < sz; i++) if (buf[i] == '\n') lines++;
    s->key = malloc(lines * sizeof(char *)); s->val = malloc(lines * sizeof(char *));
    char *p = buf;
    while (*p) {
        char *eol = strchr(p, '\n'); if (eol) *eol = 0;
        char *tab = strchr(p, '\t');
        if (tab) {
            *tab = 0;
            char *v = tab + 1, *w = v;      // unescape \n \f in place
            for (char *r = v; *r; r++) {
                if (r[0] == '\\' && (r[1] == 'n' || r[1] == 'f')) { *w++ = r[1] == 'n' ? '\n' : '\f'; r++; }
                else *w++ = *r;
            }
            *w = 0;
            s->key[s->n] = p; s->val[s->n] = v; s->n++;
        }
        if (!eol) break;
        p = eol + 1;
    }
}

static Sheet *load_sheet_l(const char *name, int lang) {
    for (int i = 0; i < nsheets; i++) if (sheets[i].lang == lang && !strcmp(sheets[i].name, name)) return &sheets[i];
    char path[96]; snprintf(path, sizeof path, "romfs:/text/%s%s.txt", LDIR[lang], name);
    size_t sz; char *buf = read_file(path, &sz);
    if (!buf && lang != LANG_EN) {           // a sheet the language doesn't have: English
        snprintf(path, sizeof path, "romfs:/text/%s.txt", name); buf = read_file(path, &sz);
    }
    if (!buf) return NULL;
    Sheet *s;
    if (nsheets < MAX_SHEETS) s = &sheets[nsheets++];
    else { s = &sheets[nextEvict]; nextEvict = (nextEvict + 1) % MAX_SHEETS; free(s->buf); free(s->key); free(s->val); }
    memset(s, 0, sizeof *s);
    snprintf(s->name, sizeof s->name, "%s", name); s->lang = lang;
    parse(s, buf, sz);
    return s;
}

static const char *get_l(const char *key, const char *sheet, int lang) {
    if (!key || !sheet) return NULL;
    Sheet *s = load_sheet_l(sheet, lang);
    if (!s) return NULL;
    for (int i = 0; i < s->n; i++) if (!strcmp(s->key[i], key)) return s->val[i];
    return NULL;
}

const char *lang_get(const char *key, const char *sheet) { return get_l(key, sheet, g_lang); }
const char *lang_get_en(const char *key, const char *sheet) { return get_l(key, sheet, LANG_EN); }

const char *lang_get_or_key(const char *key, const char *sheet) {
    const char *v = lang_get(key, sheet);
    return v ? v : (key ? key : "");
}

// a text file of the chosen language (romfs:/text/<lang>/<name>.txt), or the English one
void lang_path(const char *name, char *out, int n) {
    snprintf(out, n, "romfs:/text/%s%s.txt", LDIR[g_lang], name);
    if (g_lang != LANG_EN) { FILE *f = fopen(out, "rb"); if (f) fclose(f); else snprintf(out, n, "romfs:/text/%s.txt", name); }
}

// the port's own lines
static Sheet port; static int portLang = -1;
const char *lang_port(const char *en) {
    if (!en || g_lang == LANG_EN) return en;
    if (portLang != g_lang) {
        if (port.buf) { retire(port.buf); retire(port.key); retire(port.val); }
        memset(&port, 0, sizeof port); portLang = g_lang;
        char path[64]; snprintf(path, sizeof path, "romfs:/text/%s_port.txt", LDIR[g_lang]);
        size_t sz; char *buf = read_file(path, &sz);
        if (buf) parse(&port, buf, sz);
    }
    for (int i = 0; i < port.n; i++) if (!strcmp(port.key[i], en)) return port.val[i];
    return en;
}

void lang_set(int lang) {
    if (lang < 0 || lang >= LANG_COUNT || lang == g_lang) return;
    for (int i = 0; i < nsheets; i++) { retire(sheets[i].buf); retire(sheets[i].key); retire(sheets[i].val); }
    nsheets = 0; nextEvict = 0;
    g_lang = lang;
}

const char *lang_name(int lang) { return lang == LANG_ES ? "Espa\xf1ol" : "English"; }

// settings.txt on the SD card: the language, the volumes and the button hints
#include "audio.h"
bool g_hints = true;
bool g_lowEffects = false;
bool g_lowParticles = false;
void settings_load(void) {
    FILE *f = fopen("sdmc:/3ds/hk3ds/settings.txt", "rb"); if (!f) return;
    char ln[64];
    while (fgets(ln, sizeof ln, f)) {
        int v;
        if (sscanf(ln, "lang %d", &v) == 1) lang_set(v);
        else if (sscanf(ln, "music %d", &v) == 1 && v >= 0 && v <= 10) g_musicVolume = v / 10.0f;
        else if (sscanf(ln, "sound %d", &v) == 1 && v >= 0 && v <= 10) g_sfxVolume = v / 10.0f;
        else if (sscanf(ln, "hints %d", &v) == 1) g_hints = v != 0;
        else if (sscanf(ln, "effects %d", &v) == 1) g_lowEffects = v != 0;
        else if (sscanf(ln, "particles %d", &v) == 1) g_lowParticles = v != 0;
    }
    fclose(f);
}
void settings_save(void) {
    mkdir("sdmc:/3ds", 0777); mkdir("sdmc:/3ds/hk3ds", 0777);
    FILE *f = fopen("sdmc:/3ds/hk3ds/settings.txt", "wb"); if (!f) return;
    fprintf(f, "lang %d\nmusic %d\nsound %d\nhints %d\neffects %d\nparticles %d\n",
            g_lang,
            (int)(g_musicVolume * 10 + 0.5f),
            (int)(g_sfxVolume * 10 + 0.5f),
            g_hints ? 1 : 0,
            g_lowEffects ? 1 : 0,
            g_lowParticles ? 1 : 0);
    fclose(f);
}
