int hk_zlib_uncompress(void *out, unsigned int *outLen, const void *src, unsigned int srcLen);
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ext/tinf.h"

// A big HKZ1 file is unpacked in one buffer: the packed stream is read into the end of the buffer and unpacked from
// there towards its start, so the packed and the unpacked data never need memory of their own at once. The rooms' art
// (.lyr) is HKZ1 around tiles that are packed already, so the two are about the same size: holding both doubled what a
// room switch needed, and on a console short of memory the art didn't load and the room stayed black.
// The output can't catch up with the unread input: the gap between them is MARGIN plus however much the rest of the
// stream will still grow by unpacking. It dips below MARGIN only where the rest of the stream is bigger than what it
// unpacks to, and zlib picks the smallest form for every block, so that is part of one block at most (level 9 blocks
// hold 16K codes) plus 5 bytes per stored block; libdeflate writes at most a few hundred bytes ahead of its output.
#define INPLACE_MARGIN(raw) (65536u + ((raw) >> 12))

// Data files may be zlib-wrapped: 'HKZ1' u32 rawSize, zlib stream.
void *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;                    // (many callers just look whether a file is there)
    long n = 0;
    if (fseek(f, 0, SEEK_END) == 0) n = ftell(f);
    if (n < 0) n = 0;
    fseek(f, 0, SEEK_SET);
    u8 hdr[8]; u32 raw = 0;
    bool hkz = n >= 8 && fread(hdr, 1, 8, f) == 8 && !memcmp(hdr, "HKZ1", 4);
    if (hkz) memcpy(&raw, hdr + 4, 4);
    if (hkz && (size_t)(n - 8) > INPLACE_MARGIN(raw)) {
        size_t packed = n - 8, total = (size_t)raw + INPLACE_MARGIN(raw);
        if (total < packed) total = packed;
        u8 *out = malloc(total + 1);
        if (!out) { fclose(f); hk_log("read %s: no memory (%luK)", path, (unsigned long)(total / 1024)); return NULL; }
        u8 *src = out + total - packed;
        bool ok = fread(src, 1, packed, f) == packed;
        fclose(f);
        unsigned int outLen = raw;
        if (!ok || hk_zlib_uncompress(out, &outLen, src, packed) != TINF_OK) {
            hk_log("read %s: %s", path, ok ? "damaged (doesn't unpack)" : "read failed"); free(out); return NULL;
        }
        out[outLen] = 0;                    // always NUL-terminated (text files are parsed with strtok)
        u8 *shrunk = realloc(out, (size_t)outLen + 1);      // the margin and the packed stream's room back
        if (size) *size = outLen;
        return shrunk ? shrunk : out;
    }
    u8 *buf = malloc(n + 1);
    if (!buf) { fclose(f); hk_log("read %s: no memory (%ldK)", path, n / 1024); return NULL; }
    fseek(f, 0, SEEK_SET);
    bool ok = fread(buf, 1, n, f) == (size_t)n;
    fclose(f);
    if (!ok) { free(buf); hk_log("read %s: read failed", path); return NULL; }
    buf[n] = 0;
    if (hkz) {
        u8 *out = malloc(raw + 1);
        unsigned int outLen = raw;
        if (!out || hk_zlib_uncompress(out, &outLen, buf + 8, n - 8) != TINF_OK) {
            hk_log("read %s: %s", path, out ? "damaged (doesn't unpack)" : "no memory to unpack");
            free(out); free(buf); return NULL;
        }
        free(buf);
        out[outLen] = 0;
        if (size) *size = outLen;
        return out;
    }
    if (size) *size = n;
    return buf;
}

void tex_upload(C3D_Tex *t, const void *data, size_t size) {
    if (size > t->size) size = t->size;
    memcpy(t->data, data, size);
    GSPGPU_FlushDataCache(t->data, size);
}

static GPU_TEXCOLOR fmt_of(u8 f) {
    switch (f) {
        case 1: return GPU_RGB565;
        case 2: return GPU_RGBA4;
        case 3: return GPU_RGBA5551;
        default: return GPU_RGBA8;
    }
}

#define RD(T, p) ({ T _v; memcpy(&_v, p, sizeof(T)); p += sizeof(T); _v; })

#include <stdarg.h>
#include <malloc.h>
void hk_log(const char *fmt, ...) {
    static bool first = true;
    if (first) {     // keep the last session's log (a problem seen before a restart is still there to send)
        remove("sdmc:/3ds/hk3ds/log_prev.txt"); rename("sdmc:/3ds/hk3ds/log.txt", "sdmc:/3ds/hk3ds/log_prev.txt");
    }
    FILE *f = fopen("sdmc:/3ds/hk3ds/log.txt", first ? "w" : "a"); first = false;
    if (!f) return;
#ifdef __HOST__
    if (getenv("HK_MEMLOG")) { struct mallinfo2 mi = mallinfo2(); fprintf(f, "[used %6zuK] ", mi.uordblks / 1024); }
#else
    struct mallinfo mi = mallinfo();
    fprintf(f, "[lin %5luK heap %5luK] ", (unsigned long)(linearSpaceFree() / 1024), (unsigned long)((envGetHeapSize() - mi.uordblks) / 1024));
#endif
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static bool metaOnly;
bool g_lowmem;      // an Old 3DS: about half the memory (main.c)
// names, frames and animations without the textures (to see what a set holds before spending GPU memory on it)
bool spr_load_meta(SpriteSet *s, const char *path) { metaOnly = true; bool ok = spr_load(s, path); metaOnly = false; return ok; }
bool spr_load(SpriteSet *s, const char *path) {
    memset(s, 0, sizeof(*s));
    size_t sz;
    u8 *buf = read_file(path, &sz);
    if (!buf) { printf("missing %s\n", path); return false; }
    u8 *p = buf;
    if (memcmp(p, "HKSP", 4)) { free(buf); return false; }
    p += 4;
    s->npages = RD(u16, p); s->nframes = RD(u16, p); s->nanims = RD(u16, p); s->nnames = RD(u16, p);
    s->pages = calloc(s->npages, sizeof(C3D_Tex));
    u8 *dens = calloc(s->npages ? s->npages : 1, 1);     // HD sheets: texels per screen pixel across (2 on the 800-wide screen)
    for (int i = 0; i < s->npages; i++) {
        u16 w = RD(u16, p), h = RD(u16, p);
        u8 fmt = RD(u8, p); dens[i] = RD(u8, p); (void)RD(u16, p);
        if (dens[i] < 1) dens[i] = 1;
        u32 size = RD(u32, p);
        bool packed = fmt & 0x80; fmt &= 0x7F;
        if (metaOnly) { p += size; continue; }
        bool as16 = g_lowmem && fmt == 0;
        bool texOk = false;
        if (!as16) {
            texOk = C3D_TexInit(&s->pages[i], w, h, fmt_of(fmt));
            { extern bool sfx_free_memory(void); int ev = 0;     // out of (contiguous) linear memory: give back cached sounds and retry
              while (!texOk && sfx_free_memory()) { ev++; texOk = C3D_TexInit(&s->pages[i], w, h, fmt_of(fmt)); }
              { extern bool room_tilecache_release(void); while (!texOk && room_tilecache_release()) { ev++; texOk = C3D_TexInit(&s->pages[i], w, h, fmt_of(fmt)); } }
              if (ev) hk_log("tex %s page %d: freed %d sounds -> %s", path, i, ev, texOk ? "ok" : "still failed"); }
            // still no room for a full-colour page (a New 3DS whose memory a plugin or the system took part of): keep it at
            // 16 bits like the Old 3DS does instead of losing the page (missing sprites, enemies with no death animation)
            if (!texOk && fmt == 0) { hk_log("tex %s page %d: no room at 32 bits, trying 16", path, i); as16 = true; }
        }
        if (as16) {     // Old 3DS: full-colour pages are stored at 16 bits (half the GPU memory)
            u8 *tmp = malloc((size_t)w * h * 4);
            if (tmp) {
                if (packed) {
                    u8 *dst = tmp, *end = tmp + (size_t)w * h * 4; const u8 *q = p, *qe = p + size;
                    while (q < qe && dst < end) {
                        u16 hd; memcpy(&hd, q, 2); q += 2; int n = hd & 0x7FFF;
                        if (hd & 0x8000) { for (int k = 0; k < n && dst < end; k++) { memcpy(dst, q, 4); dst += 4; } q += 4; }
                        else { int bytes = n * 4; if (dst + bytes > end) bytes = end - dst; memcpy(dst, q, bytes); dst += bytes; q += n * 4; }
                    }
                } else memcpy(tmp, p, size < (u32)w * h * 4 ? size : (u32)w * h * 4);
                bool ok4 = C3D_TexInit(&s->pages[i], w, h, GPU_RGBA4);
                { extern bool sfx_free_memory(void); extern bool room_tilecache_release(void);
                  while (!ok4 && sfx_free_memory()) ok4 = C3D_TexInit(&s->pages[i], w, h, GPU_RGBA4);
                  while (!ok4 && room_tilecache_release()) ok4 = C3D_TexInit(&s->pages[i], w, h, GPU_RGBA4); }
                if (ok4) {
                    u16 *o16 = s->pages[i].data;     // the same texel order (tiling goes by pixel position): convert texel by texel
                    for (u32 k = 0; k < (u32)w * h; k++) { const u8 *t = tmp + k * 4;   // RGBA8 bytes: A B G R
                        o16[k] = (u16)(((t[3] >> 4) << 12) | ((t[2] >> 4) << 8) | ((t[1] >> 4) << 4) | (t[0] >> 4)); }
                    GSPGPU_FlushDataCache(s->pages[i].data, s->pages[i].size);
                    C3D_TexSetFilter(&s->pages[i], GPU_LINEAR, GPU_LINEAR);
                    C3D_TexSetWrap(&s->pages[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
                } else { hk_log("TEX ALLOC FAIL %s page %d %dx%d (16 bit)", path, i, w, h); memset(&s->pages[i], 0, sizeof s->pages[i]); }
                free(tmp); p += size; continue;
            }
        }
        if (!texOk) { hk_log("TEX ALLOC FAIL %s page %d %dx%d fmt %d", path, i, w, h, fmt); memset(&s->pages[i], 0, sizeof s->pages[i]); }
        else {
            if (packed) {
                int unit = fmt == 0 ? 4 : 2;
                u8 *dst = s->pages[i].data, *end = dst + s->pages[i].size;
                const u8 *q = p, *qe = p + size;
                while (q < qe && dst < end) {
                    u16 hd; memcpy(&hd, q, 2); q += 2;
                    int n = hd & 0x7FFF;
                    if (hd & 0x8000) { for (int k = 0; k < n && dst < end; k++) { memcpy(dst, q, unit); dst += unit; } q += unit; }
                    else { int bytes = n * unit; if (dst + bytes > end) bytes = end - dst; memcpy(dst, q, bytes); dst += bytes; q += n * unit; }
                }
                GSPGPU_FlushDataCache(s->pages[i].data, s->pages[i].size);
            } else tex_upload(&s->pages[i], p, size);
            C3D_TexSetFilter(&s->pages[i], GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap(&s->pages[i], GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
        p += size;
    }
    s->frames = calloc(s->nframes, sizeof(Frame));
    for (int i = 0; i < s->nframes; i++) {
        Frame *f = &s->frames[i];
        f->page = RD(u16, p); f->x = RD(u16, p); f->y = RD(u16, p); f->w = RD(u16, p); f->h = RD(u16, p);
        f->ox = RD(s16, p); f->oy = RD(s16, p); u16 fd = RD(u16, p);
        C3D_Tex *t = &s->pages[f->page];
        u16 texw = f->w; int d = fd ? fd : f->page < s->npages ? dens[f->page] : 1;     // a frame's own density, or its page's
        // an HD frame: twice the texels across, sized and placed in screen pixels like any other (the packer keeps its
        // width and offset even), so the drawing and layout code never knows
        if (d > 1) { f->w = texw / d; f->ox = f->ox / d; }
        f->sub.width = f->w; f->sub.height = f->h;
        f->sub.left = f->x / (float)t->width;
        f->sub.right = (f->x + texw) / (float)t->width;
        f->sub.top = 1.0f - f->y / (float)t->height;   // tex3ds convention: row 0 of memory = top of image = v 1.0
        f->sub.bottom = 1.0f - (f->y + f->h) / (float)t->height;   // top > bottom (top < bottom means 'rotated' to citro2d)
        f->img.tex = t; f->img.subtex = &f->sub;
    }
    s->anims = calloc(s->nanims ? s->nanims : 1, sizeof(Anim));
    int total = 0;
    for (int i = 0; i < s->nanims; i++) {
        Anim *a = &s->anims[i];
        memcpy(a->name, p, 32); a->name[31] = 0; p += 32;
        a->fps = RD(float, p); a->wrap = RD(u8, p); a->loopStart = RD(u8, p);
        a->first = RD(u16, p); a->count = RD(u16, p); (void)RD(u16, p);
        total += a->count;
    }
    s->animFrames = calloc(total ? total : 1, sizeof(u16));
    s->animFlags = calloc(total ? total : 1, sizeof(u16));
    for (int i = 0; i < total; i++) { s->animFrames[i] = RD(u16, p); s->animFlags[i] = RD(u16, p); }
    s->names = calloc(s->nnames ? s->nnames : 1, 32);
    s->nameFrame = calloc(s->nnames ? s->nnames : 1, sizeof(u16));
    for (int i = 0; i < s->nnames; i++) {
        memcpy(s->names[i], p, 32); s->names[i][31] = 0; p += 32;
        s->nameFrame[i] = RD(u16, p); (void)RD(u16, p);
    }
    free(buf); free(dens);
    s->loaded = true;
    return true;
}

// one frame drawn from its own 8-bit alpha texture (black with alpha): the Knight's vignette is a soft gradient drawn
// ~10x magnified, and at 16 bits (an Old 3DS) its 4-bit alpha showed rings across the whole screen
bool spr_frame_alpha8(SpriteSet *s, int frame, const char *path, int w, int h) {
    static C3D_Tex tex; static Tex3DS_SubTexture sub; static bool ok, tried;
    if (!s->loaded || frame < 0 || frame >= s->nframes) return false;
    if (!ok && !tried) {
        tried = true;
        size_t sz; void *buf = read_file(path, &sz);
        if (buf && sz >= 256 * 256 && C3D_TexInit(&tex, 256, 256, GPU_A8)) {
            memcpy(tex.data, buf, 256 * 256); GSPGPU_FlushDataCache(tex.data, tex.size);
            C3D_TexSetFilter(&tex, GPU_LINEAR, GPU_LINEAR); C3D_TexSetWrap(&tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            sub.width = w; sub.height = h; sub.left = 0; sub.right = w / 256.0f; sub.top = 1.0f; sub.bottom = 1.0f - h / 256.0f;
            ok = true;
        } else hk_log("alpha8 texture %s not loaded", path);
        free(buf);
    }
    if (!ok) return false;
    Frame *f = &s->frames[frame]; f->img.tex = &tex; f->img.subtex = &sub;
    return true;
}

void spr_free(SpriteSet *s) {
    if (!s->loaded) return;
    for (int i = 0; i < s->npages; i++) if (s->pages[i].data) C3D_TexDelete(&s->pages[i]);
    free(s->pages); free(s->frames); free(s->anims); free(s->animFrames); free(s->animFlags);
    free(s->names); free(s->nameFrame);
    memset(s, 0, sizeof(*s));
}

int spr_anim(const SpriteSet *s, const char *name) {
    for (int i = 0; i < s->nanims; i++) if (!strcmp(s->anims[i].name, name)) return i;
    return -1;
}

int spr_named(const SpriteSet *s, const char *name) {
    for (int i = 0; i < s->nnames; i++) if (!strcmp(s->names[i], name)) return s->nameFrame[i];
    return -1;
}

void spr_draw_tint(const SpriteSet *s, int frame, float x, float y, float sx, float sy, u32 color, float blend) {
    if (!s->loaded || frame < 0 || frame >= s->nframes) return;
    const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;   // its page failed to allocate
    // citro2d draws a mirrored image (negative scale) over [px, px + w*|sx|] with the texture flipped, so the left
    // edge of a mirrored sprite is x - (ox + w) * |sx|
    float px = (sx >= 0) ? x + f->ox * sx : x - (f->ox + f->w) * -sx;
    float py = y + f->oy * sy;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);
    gfx_tint_mode(false);
    C2D_DrawImageAt(f->img, px, py, 0.5f, &tint, sx, sy);
}

void spr_draw(const SpriteSet *s, int frame, float x, float y, float sx, float sy, float alpha) {
    if (alpha >= 0.999f) {
        if (!s->loaded || frame < 0 || frame >= s->nframes) return;
        const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;   // its page failed to allocate
        float px = (sx >= 0) ? x + f->ox * sx : x - (f->ox + f->w) * -sx;
        C2D_DrawImageAt(f->img, px, y + f->oy * sy, 0.5f, NULL, sx, sy);
    } else {
        u32 c = C2D_Color32f(1, 1, 1, alpha < 0 ? 0 : alpha);
        C2D_ImageTint tint;
        for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = 0; }
        if (!s->loaded || frame < 0 || frame >= s->nframes) return;
        const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;   // its page failed to allocate
        float px = (sx >= 0) ? x + f->ox * sx : x - (f->ox + f->w) * -sx;
        C2D_DrawImageAt(f->img, px, y + f->oy * sy, 0.5f, &tint, sx, sy);
    }
}


// general sprite draw: pivot at (x,y), scale (negative mirrors around the pivot), rotation in degrees (counter-
// clockwise in world space), alpha and optional white flash
void spr_draw_ex(const SpriteSet *s, int frame, float x, float y, float sx, float sy, float angleDeg, float alpha, float flash) {
    if (!s->loaded || frame < 0 || frame >= s->nframes) return;
    const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;   // its page failed to allocate
    float L = sx >= 0 ? f->ox * sx : -(f->ox + f->w) * -sx;
    float T = sy >= 0 ? f->oy * sy : -(f->oy + f->h) * -sy;
    C2D_DrawParams p = { { x, y, f->w * sx, f->h * sy }, { -L, -T }, 0.5f, -angleDeg * 3.14159265f / 180.0f };
    C2D_ImageTint tint; bool useTint = alpha < 0.999f || flash > 0;
    if (useTint) { u32 c = C2D_Color32f(1, 1, 1, alpha < 0 ? 0 : alpha); for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = flash; } }
    if (flash > 0) gfx_tint_mode(false);
    C2D_DrawImage(f->img, &p, useTint ? &tint : NULL);
}

// a tinted sprite (SpriteRenderer.color / tk2dSprite.color times material colour): citro2d blends towards the tint
// colour, so a multiply is approximated by blending towards (r,g,b) by 1 - min(r,g,b)
// a colour tint multiplies the texture (Unity's sprite colour, a scene's ambient light): citro2d's multiply tint mode.
// (It used to blend the texture toward the colour, which flattened tinted sprites into a wash of that colour.)
static int tintMode = -1;
void gfx_tint_mode(bool mult) {
    if (tintMode == (int)mult) return;
    tintMode = mult; C2D_SetTintMode(mult ? C2D_TintMult : C2D_TintSolid);
}
void spr_draw_col(const SpriteSet *s, int frame, float x, float y, float sx, float sy, float angleDeg, float r, float g, float b, float alpha, float flash) {
    if (r > 0.99f && g > 0.99f && b > 0.99f) { spr_draw_ex(s, frame, x, y, sx, sy, angleDeg, alpha, flash); return; }
    if (!s->loaded || frame < 0 || frame >= s->nframes) return;
    const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;   // its page failed to allocate
    float L = sx >= 0 ? f->ox * sx : -(f->ox + f->w) * -sx;
    float T = sy >= 0 ? f->oy * sy : -(f->oy + f->h) * -sy;
    C2D_DrawParams p = { { x, y, f->w * sx, f->h * sy }, { -L, -T }, 0.5f, -angleDeg * 3.14159265f / 180.0f };
    if (flash > 0) {      // a hit flash: towards white
        gfx_tint_mode(false);
        u32 c = C2D_Color32f(1, 1, 1, alpha < 0 ? 0 : alpha);
        C2D_ImageTint tint; for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = flash; }
        C2D_DrawImage(f->img, &p, &tint); return;
    }
    u32 c = C2D_Color32f(r < 0 ? 0 : r > 1 ? 1 : r, g < 0 ? 0 : g > 1 ? 1 : g, b < 0 ? 0 : b > 1 ? 1 : b, alpha < 0 ? 0 : alpha > 1 ? 1 : alpha);
    C2D_ImageTint tint; for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = 1; }
    gfx_tint_mode(true);          // left on: switching costs a flush, and only flashes and colour washes need it off
    C2D_DrawImage(f->img, &p, &tint);
}

/* ---------------- animation ---------------- */
void anim_play_idx(AnimPlayer *a, int idx, bool restart) {
    if (idx < 0) return;
    if (a->anim == idx && !restart && !a->stopped) return;
    a->anim = idx; a->t = 0; a->frame = 0; a->done = false; a->stopped = false;
}

void anim_play(AnimPlayer *a, const SpriteSet *s, const char *name, bool restart) {
    a->set = s;
    int idx = spr_anim(s, name);
    if (idx < 0) return;
    anim_play_idx(a, idx, restart);
}

void anim_update(AnimPlayer *a, float dt) {
    if (!a->set || a->anim < 0 || a->anim >= a->set->nanims || a->stopped) return;
    const Anim *an = &a->set->anims[a->anim];
    if (an->count == 0) return;
    a->t += dt;
    int n = (int)(a->t * an->fps);
    switch (an->wrap) {
        case WRAP_LOOP: a->frame = n % an->count; break;
        case WRAP_LOOPSECTION:
            if (n < an->count) a->frame = n;
            else {
                int ls = an->loopStart < an->count ? an->loopStart : 0;
                int len = an->count - ls; if (len < 1) len = 1;
                a->frame = ls + (n - an->count) % len;
            }
            break;
        case WRAP_PINGPONG: {
            int period = an->count > 1 ? (an->count - 1) * 2 : 1;
            int k = n % period;
            a->frame = k < an->count ? k : period - k;
            break;
        }
        case WRAP_SINGLE: a->frame = 0; a->done = true; break;
        default:
            if (n >= an->count) { a->frame = an->count - 1; a->done = true; }
            else a->frame = n;
    }
}

int anim_cur_frame(const AnimPlayer *a) {
    if (!a->set || a->anim < 0 || a->anim >= a->set->nanims) return -1;
    const Anim *an = &a->set->anims[a->anim];
    if (!an->count) return -1;
    return a->set->animFrames[an->first + a->frame];
}

float anim_length(const AnimPlayer *a) {
    if (!a->set || a->anim < 0) return 0;
    const Anim *an = &a->set->anims[a->anim];
    return an->fps > 0 ? an->count / an->fps : 0;
}

bool anim_is(const AnimPlayer *a, const char *name) {
    if (!a->set || a->anim < 0) return false;
    return !strcmp(a->set->anims[a->anim].name, name);
}

/* ---------------- fonts ---------------- */
bool font_load(Font *f, const char *path) {
    if (!spr_load(&f->set, path)) return false;
    for (int c = 0; c < 224; c++) {
        char nm[8]; snprintf(nm, sizeof nm, "c%d", c + 32);
        f->glyph[c] = spr_named(&f->set, nm);
    }
    int gi = f->glyph['A' - 32];
    f->lineh = gi >= 0 ? f->set.frames[gi].h + 2 * f->set.frames[gi].oy : 12;
    return true;
}

float font_width(const Font *f, const char *txt) {
    float w = 0;
    for (const unsigned char *p = (const unsigned char *)txt; *p; p++) {
        int c = *p - 32;
        if (c < 0 || c >= 224 || f->glyph[c] < 0) { w += 4; continue; }
        const Frame *fr = &f->set.frames[f->glyph[c]];
        w += fr->w + 2 * fr->ox;
    }
    return w;
}

float font_fit(const Font *f, const char *txt, float maxw, float scale) {
    float w = font_width(f, txt) * scale;
    return w > maxw && w > 0 ? scale * maxw / w : scale;
}

void font_draw(const Font *f, const char *txt, float x, float y, float scale, u32 color) {
    float cx = x;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, 1.0f);
    gfx_tint_mode(false);
    for (const unsigned char *p = (const unsigned char *)txt; *p; p++) {
        if (*p == '\n') { cx = x; y += f->lineh * scale; continue; }
        int c = *p - 32;
        if (c < 0 || c >= 224 || f->glyph[c] < 0) { cx += 4 * scale; continue; }
        const Frame *fr = &f->set.frames[f->glyph[c]];
        C2D_DrawImageAt(fr->img, floorf(cx + fr->ox * scale), floorf(y + fr->oy * scale), 0.5f, &tint, scale, scale);
        cx += (fr->w + 2 * fr->ox) * scale;
    }
}

void font_draw_centered(const Font *f, const char *txt, float cx, float y, float scale, u32 color) {
    font_draw(f, txt, cx - font_width(f, txt) * scale / 2, y, scale, color);
}

// additive blending for glow / light sprites (Unity additive particle and sprite materials)
void gfx_additive(bool on) {
#ifdef __HOST__
    extern int host_additive; host_additive = on;
#else
    C2D_Flush();
    if (on) C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
    else C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
#endif
}

// draws only the bottom `frac` (0..1) of a frame, as if it were filled from below (the HUD soul orb liquid)
void spr_draw_fill(const SpriteSet *s, int frame, float x, float y, float sx, float sy, float frac, float alpha) {
    if (!s->loaded || frame < 0 || frame >= s->nframes || frac <= 0) return;
    const Frame *f = &s->frames[frame]; if (!f->img.tex || !f->img.tex->data) return;
    if (frac > 1) frac = 1;
    Tex3DS_SubTexture sub = f->sub;
    float hv = sub.top - sub.bottom;                  // v span (top > bottom)
    sub.top = sub.bottom + hv * frac;
    sub.height = (u16)(f->h * frac + 0.5f); if (!sub.height) return;
    C2D_Image img = { f->img.tex, &sub };
    C2D_ImageTint tint; u32 c = C2D_Color32f(1, 1, 1, alpha);
    for (int i = 0; i < 4; i++) { tint.corners[i].color = c; tint.corners[i].blend = 0; }
    C2D_DrawImageAt(img, x + f->ox * sx, y + (f->oy + f->h * (1 - frac)) * sy, 0.5f, &tint, sx, sy);
}
