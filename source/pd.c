// PlayerData: the game's save fields by name (PlayerData.GetBool/SetBool/GetInt/SetInt as FSMs use them).
// Fields the demake models directly are mapped onto the hero; everything else lives in a small table that is
// written to the save file.
#include "pd.h"
#include "hero.h"
#include <string.h>
#include <stdio.h>
#include "pd_defaults.h"

typedef struct { char name[64]; int v; bool isBool; } PdEnt;
#define PD_CAP 8192
#define PD_HASH 16384
static PdEnt tab[PD_CAP]; static int ntab;
static short hidx[PD_HASH]; static bool hready;     // index+1 into tab, 0 = empty (open addressing)
static unsigned hstr(const char *s) { unsigned h = 2166136261u; while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }
static void rehash(void) {
    memset(hidx, 0, sizeof hidx);
    for (int i = 0; i < ntab; i++) { unsigned h = hstr(tab[i].name) & (PD_HASH - 1); while (hidx[h]) h = (h + 1) & (PD_HASH - 1); hidx[h] = (short)(i + 1); }
    hready = true;
}
static PdEnt *find(const char *n, bool make, bool isBool) {
    if (!hready) rehash();
    unsigned h = hstr(n) & (PD_HASH - 1);
    while (hidx[h]) { PdEnt *e = &tab[hidx[h] - 1]; if (!strcmp(e->name, n)) return e; h = (h + 1) & (PD_HASH - 1); }
    if (!make || ntab >= PD_CAP) return NULL;
    PdEnt *e = &tab[ntab]; memset(e, 0, sizeof *e); strncpy(e->name, n, 63); e->isBool = isBool;
    hidx[h] = (short)(ntab + 1); ntab++;
    return e;
}

static int *mapped_int(const char *n) {
    if (!strcmp(n, "health")) return &P.health;
    if (!strcmp(n, "maxHealth")) return &P.maxHealth;
    if (!strcmp(n, "geo")) return &P.geo;
    if (!strcmp(n, "MPCharge")) return &P.soul;
    return NULL;
}
static bool *mapped_bool(const char *n) {
    if (!strcmp(n, "hasDash") || !strcmp(n, "canDash")) return &P.hasDash;
    if (!strcmp(n, "hasWalljump") || !strcmp(n, "canWallJump")) return &P.hasWalljump;
    if (!strcmp(n, "hasDoubleJump")) return &P.hasDoubleJump;
    return NULL;
}

#define PDD_N (int)(sizeof PD_DEFAULTS / sizeof *PD_DEFAULTS)
static short dhash[1024]; static bool dready;
static int pd_default(const char *n) {      // PlayerData.SetupNewPlayerData
    if (!dready) {
        for (int i = 0; i < PDD_N; i++) { unsigned h = hstr(PD_DEFAULTS[i].name) & 1023; while (dhash[h]) h = (h + 1) & 1023; dhash[h] = (short)(i + 1); }
        dready = true;
    }
    unsigned h = hstr(n) & 1023;
    while (dhash[h]) { if (!strcmp(PD_DEFAULTS[dhash[h] - 1].name, n)) return PD_DEFAULTS[dhash[h] - 1].v; h = (h + 1) & 1023; }
    return 0;
}
bool pd_bool(const char *n) {
    if (!n) return false;
    bool *m = mapped_bool(n); if (m) return *m;
    PdEnt *e = find(n, false, true); return e ? e->v != 0 : pd_default(n) != 0;
}
void pd_set_bool(const char *n, bool v) {
    if (!n) return;
#ifdef __HOST__
    { static int tr = -1; if (tr < 0) tr = getenv("HK_PDTRACE") != NULL; if (tr) fprintf(stderr, "[pd] %s = %d\n", n, v); }
#endif
    bool *m = mapped_bool(n); if (m) { *m = v; return; }
    PdEnt *e = find(n, true, true); if (e) e->v = v;
}
int pd_int(const char *n) {
    if (!n) return 0;
    int *m = mapped_int(n); if (m) return *m;
    if (!strcmp(n, "fireballLevel")) { PdEnt *e = find(n, false, false); return e ? e->v : 0; }
    PdEnt *e = find(n, false, false); return e ? e->v : pd_default(n);
}
void pd_set_int(const char *n, int v) {
    if (!n) return;
#ifdef __HOST__
    { static int tr = -1; if (tr < 0) tr = getenv("HK_PDTRACE") != NULL; if (tr) fprintf(stderr, "[pd] %s = %d (int)\n", n, v); }
#endif
    int *m = mapped_int(n); if (m) { *m = v; return; }
    PdEnt *e = find(n, true, false); if (e) e->v = v;
}
/* strings (PlayerData string fields: nextScene, bench scene...) */
typedef struct { char name[48]; char v[64]; } PdStr;
static PdStr stab[128]; static int nstab;
const char *pd_str(const char *n) {
    if (!n) return "";
    for (int i = 0; i < nstab; i++) if (!strcmp(stab[i].name, n)) return stab[i].v;
    return "";
}
void pd_set_str(const char *n, const char *v) {
    if (!n) return;
    PdStr *e = NULL;
    for (int i = 0; i < nstab; i++) if (!strcmp(stab[i].name, n)) e = &stab[i];
    if (!e) { if (nstab >= 128) return; e = &stab[nstab++]; snprintf(e->name, sizeof e->name, "%s", n); }
    snprintf(e->v, sizeof e->v, "%s", v ? v : "");
}

void pd_reset(void) { ntab = 0; nstab = 0; hready = false; }
// names may contain spaces (object names in pbs:/msk: keys): written with '|' instead
static void put_name(FILE *f, const char *n) { for (; *n; n++) fputc(*n == ' ' ? '|' : *n, f); }
static void unbar(char *n) { for (; *n; n++) if (*n == '|') *n = ' '; }
void pd_save(FILE *f) {
    for (int i = 0; i < ntab; i++) { fputc(tab[i].isBool ? 'B' : 'I', f); fputc(' ', f); put_name(f, tab[i].name); fprintf(f, " %d\n", tab[i].v); }
    for (int i = 0; i < nstab; i++) { fputs("T ", f); put_name(f, stab[i].name); fputc(' ', f); put_name(f, stab[i].v[0] ? stab[i].v : "~"); fputc('\n', f); }
}
bool pd_load_line(const char *line) {
    char t, n[64]; int v;
    if (line[0] == 'T' && line[1] == ' ') {
        char sv[64];
        if (sscanf(line, "T %63s %63s", n, sv) != 2) return false;
        unbar(n); unbar(sv); pd_set_str(n, strcmp(sv, "~") ? sv : "");
        return true;
    }
    if (sscanf(line, "%c %63s %d", &t, n, &v) != 3 || (t != 'B' && t != 'I')) return false;
    unbar(n);
    if (t == 'B') pd_set_bool(n, v != 0); else pd_set_int(n, v);
    return true;
}
bool pd_any_set(const char *pre) {
    size_t l = strlen(pre);
    for (int i = 0; i < ntab; i++) if (tab[i].isBool && tab[i].v && !strncmp(tab[i].name, pre, l)) return true;
    return false;
}
void pd_clear_prefix(const char *pre) {
    size_t l = strlen(pre); int j = 0;
    for (int i = 0; i < ntab; i++) if (strncmp(tab[i].name, pre, l)) tab[j++] = tab[i];
    ntab = j; hready = false;
}
