/* LXRXXL ZYNTH -- presets on the device's storage: SOUNDS (one track's voice) and PERFORMANCES (the whole instance:
 * the eight sounds, the 64 patterns, the delay, the gate/duck, the mixer, the tempo).
 *
 *   <data>/Sounds/SOUND 01.zysound ... SOUND 64.zysound          text: a header line, then key=value per parameter
 *                                                                (and m<voice>_<slider>: every voice's sliders)
 *   <data>/Performances/PERF 01.zyperf ... PERF 64.zyperf        text: the same as MPC's project keeps (store.c)
 *
 * <data> is /sdcard/LXRXXL ZYNTH (outside the plugin folder, so presets survive updates and reinstalls), or, when
 * that can't be written, "LXRXXL ZYNTH Data" beside the plugin. The keys (FX MIXER): SOUND / PERF pick what, the
 * SLOT number which one; SAVE (tap twice: the second tap writes) and LOAD (a sound loads at once onto the selected
 * track; a performance asks for a second tap, since it replaces everything). Files are read and written on a worker
 * thread, so the audio never waits for the card; a load is handed over at the next audio block (preset_poll). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "bl.h"
#include "param_map.h"

#define SOUND_MAGIC "LXRXXL-ZYNTH SOUND 1"
#define ARM_FRAMES (44100u * 6u)   /* a first SAVE / LOAD tap waits this long for the second */

static uint32_t g_nextId = 1;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------------------------------------------------- paths */
static void mkdirs(const char *path)
{
    char tmp[600];
    char *p;
    snprintf(tmp, sizeof tmp, "%s", path);
    for (p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
}

static void folder(const Bl *x, int kind, char *out, size_t n)
{
    snprintf(out, n, "%s/%s", x->dataDir, kind == PR_SOUND ? "Sounds" : "Performances");
}

static void file_path(const Bl *x, int kind, int slot, char *out, size_t n)
{
    char d[560];
    folder(x, kind, d, sizeof d);
    snprintf(out, n, kind == PR_SOUND ? "%s/SOUND %02d.zysound" : "%s/PERF %02d.zyperf", d, slot + 1);
}

static int exists(const Bl *x, int kind, int slot)
{
    char p[640];
    struct stat st;
    if (!x->dataDir[0]) return 0;
    file_path(x, kind, slot, p, sizeof p);
    return stat(p, &st) == 0 && st.st_size > 0;
}

/* ---------------------------------------------------------------- the readout's line */
static void say(Bl *x, const char *fmt, int slot, int track)
{
    char b[96];
    snprintf(b, sizeof b, fmt, slot + 1, track + 1);
    snprintf(x->pr.msg, sizeof x->pr.msg, "%s", b);
    x->touched = TOUCH_PRESET;
    x->displayRev++;
}

static void status(Bl *x)
{
    const int s = x->pr.slot, k = x->pr.kind;
    if (!x->dataDir[0]) { say(x, "NO STORAGE: PRESETS CAN'T BE SAVED", s, x->ui.track); return; }
    if (k == PR_SOUND) say(x, exists(x, k, s) ? "SOUND %02d   (T%d)" : "SOUND %02d   EMPTY   (T%d)", s, x->ui.track);
    else say(x, exists(x, k, s) ? "PERF %02d" : "PERF %02d   EMPTY", s, x->ui.track);
}

/* ---------------------------------------------------------------- writing (worker thread) */
typedef struct { char dir[600], path[640]; char *buf; size_t len; } WriteJob;

static void *write_thread(void *arg)
{
    WriteJob *j = arg;
    char tmp[660];
    FILE *f;
    mkdirs(j->dir);
    snprintf(tmp, sizeof tmp, "%s.new", j->path);
    f = fopen(tmp, "wb");
    if (f) {
        const size_t w = fwrite(j->buf, 1, j->len, f);
        if (fclose(f) == 0 && w == j->len) rename(tmp, j->path);   /* never leave a half-written preset */
        else remove(tmp);
    }
    free(j->buf);
    free(j);
    return NULL;
}

static int write_async(const char *dir, const char *path, char *buf, size_t len)
{
    pthread_t th;
    pthread_attr_t at;
    WriteJob *j = calloc(1, sizeof *j);
    if (!j) { free(buf); return 0; }
    snprintf(j->dir, sizeof j->dir, "%s", dir);
    snprintf(j->path, sizeof j->path, "%s", path);
    j->buf = buf;
    j->len = len;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&th, &at, write_thread, j) != 0) write_thread(j);
    pthread_attr_destroy(&at);
    return 1;
}

static char *sound_text(const Sound *s, size_t *len)
{
    size_t cap = 65536, n;
    char *b = malloc(cap);
    int k, e;
    if (!b) return NULL;
    n = (size_t)snprintf(b, cap, "%s\n", SOUND_MAGIC);
    for (k = 0; k < SP_COUNT && n < cap; k++) {
        if (SOUND_DEFS[k].nopts) n += (size_t)snprintf(b + n, cap - n, "%s=%d\n", SOUND_DEFS[k].key, (int)s->v[k]);
        else n += (size_t)snprintf(b + n, cap - n, "%s=%.6f\n", SOUND_DEFS[k].key, s->v[k]);
    }
    for (e = 0; e < NUM_ENGINES; e++)   /* every voice's whole sound as this track left it: m<voice>.<key>= */
        for (k = 0; k < SP_COUNT && n < cap; k++) {
            if (k == SP_VOICE || k == SP_LEVEL || k == SP_PAN) continue;
            if (SOUND_DEFS[k].nopts) n += (size_t)snprintf(b + n, cap - n, "m%d.%s=%d\n", e, SOUND_DEFS[k].key, (int)s->full[e][k]);
            else n += (size_t)snprintf(b + n, cap - n, "m%d.%s=%.6f\n", e, SOUND_DEFS[k].key, s->full[e][k]);
        }
    *len = n < cap ? n : cap - 1;
    return b;
}

static int save(Bl *x)
{
    char dir[600], path[640];
    char *buf;
    size_t len = 0;
    folder(x, x->pr.kind, dir, sizeof dir);
    file_path(x, x->pr.kind, x->pr.slot, path, sizeof path);
    if (x->pr.kind == PR_SOUND) buf = sound_text(&x->p.snd[x->ui.track], &len);
    else {
        int l = 0;
        buf = store_serialize(x, &l);   /* a snapshot: the card write happens later, off the audio thread */
        len = (size_t)(l > 0 ? l : 0);
    }
    if (!buf) return 0;
    return write_async(dir, path, buf, len);
}

/* ---------------------------------------------------------------- loading (worker thread, handed over in preset_poll) */
typedef struct Loaded { uint32_t id; int kind, slot, track; char *text; struct Loaded *next; } Loaded;
static Loaded *g_loaded;
typedef struct { uint32_t id; int kind, slot, track; char path[640]; } ReadJob;

static void *read_thread(void *arg)
{
    ReadJob *j = arg;
    FILE *f = fopen(j->path, "rb");
    char *text = NULL;
    if (f) {
        long n;
        if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && n < (8l << 20) && fseek(f, 0, SEEK_SET) == 0) {
            text = malloc((size_t)n + 1);
            if (text && fread(text, 1, (size_t)n, f) == (size_t)n) text[n] = 0;
            else { free(text); text = NULL; }
        }
        fclose(f);
    }
    {
        Loaded *l = calloc(1, sizeof *l);
        if (l) {
            l->id = j->id; l->kind = j->kind; l->slot = j->slot; l->track = j->track; l->text = text;
            pthread_mutex_lock(&g_lock);
            l->next = g_loaded;
            g_loaded = l;
            pthread_mutex_unlock(&g_lock);
        } else free(text);
    }
    free(j);
    return NULL;
}

static int load(Bl *x)
{
    pthread_t th;
    pthread_attr_t at;
    ReadJob *j = calloc(1, sizeof *j);
    if (!j) return 0;
    j->id = x->pr.id;
    j->kind = x->pr.kind;
    j->slot = x->pr.slot;
    j->track = x->ui.track;
    file_path(x, j->kind, j->slot, j->path, sizeof j->path);
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&th, &at, read_thread, j) != 0) read_thread(j);
    pthread_attr_destroy(&at);
    x->pr.loading++;
    return 1;
}

static int parse_sound(const char *text, Sound *s)
{
    const char *p = text;
    int k, got = 0;
    if (strncmp(p, SOUND_MAGIC, strlen(SOUND_MAGIC))) return 0;
    init_sound(s);   /* a key the file lacks: its default */
    while ((p = strchr(p, '\n'))) {
        char key[48], sub[40];
        float v;
        int e, j;
        p++;
        if (sscanf(p, "%47[^=\n]=%f", key, &v) != 2) continue;
        if (sscanf(key, "m%d.%39s", &e, sub) == 2) {   /* a voice's whole sound (0.2.0) */
            if (e < 0 || e >= NUM_ENGINES) continue;
            for (k = 0; k < SP_COUNT; k++)
                if (!strcmp(sub, SOUND_DEFS[k].key)) {
                    const float mx = SOUND_DEFS[k].nopts ? (float)(SOUND_DEFS[k].nopts - 1) : 1.f;
                    if (k != SP_VOICE && v >= 0.f && v <= mx) s->full[e][k] = SOUND_DEFS[k].nopts ? (float)(int)(v + 0.5f) : v;
                    break;
                }
            continue;
        }
        if (sscanf(key, "m%d_%d", &e, &j) == 2) {   /* 0.1.0: a voice's six sliders */
            if (e >= 0 && e < NUM_ENGINES && j >= 0 && j < ENG_SLIDERS_V1 && v >= 0.f && v <= 1.f) s->full[e][SP_V1 + j] = v;
            continue;
        }
        for (k = 0; k < SP_COUNT; k++)
            if (!strcmp(key, SOUND_DEFS[k].key)) {
                const float mx = SOUND_DEFS[k].nopts ? (float)(SOUND_DEFS[k].nopts - 1) : 1.f;
                if (v >= 0.f && v <= mx) { s->v[k] = SOUND_DEFS[k].nopts ? (float)(int)(v + 0.5f) : v; got++; }
                break;
            }
    }
    if (!strstr(text, "\nsustain=")) {   /* a 0.1.0 sound: VCA ENV fully down held the note */
        s->v[SP_SUSTAIN] = s->v[SP_VCA_DEC] <= 0.02f ? 1.f : 0.f;
    }
    sound_sync(s);   /* the sound in use is the file's */
    return got > 0;
}

void preset_poll(Bl *x)
{
    Loaded *mine = NULL, **pp;
    if (!x->pr.loading) return;
    pthread_mutex_lock(&g_lock);
    for (pp = &g_loaded; *pp;) {
        if ((*pp)->id == x->pr.id) { Loaded *l = *pp; *pp = l->next; l->next = mine; mine = l; }
        else pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&g_lock);
    while (mine) {
        Loaded *l = mine;
        mine = l->next;
        if (x->pr.loading) x->pr.loading--;
        if (l->kind == PR_SOUND) {
            Sound s;
            if (l->text && parse_sound(l->text, &s)) {
                x->p.snd[l->track] = s;
                say(x, "LOADED SOUND %02d ON T%d", l->slot, l->track);
            } else say(x, "SOUND %02d: CAN'T READ IT", l->slot, l->track);
        } else {
            if (l->text && store_deserialize(x, l->text)) say(x, "LOADED PERF %02d", l->slot, 0);
            else say(x, "PERF %02d: CAN'T READ IT", l->slot, 0);
        }
        free(l->text);
        free(l);
    }
}

/* ---------------------------------------------------------------- the keys */
void preset_init(Bl *x)
{
    pthread_mutex_lock(&g_lock);
    x->pr.id = g_nextId++;
    pthread_mutex_unlock(&g_lock);
    x->pr.kind = PR_SOUND;
    snprintf(x->pr.msg, sizeof x->pr.msg, "SOUND 01");
}

void preset_forget(Bl *x)
{
    Loaded **pp;
    pthread_mutex_lock(&g_lock);
    for (pp = &g_loaded; *pp;) {
        if ((*pp)->id == x->pr.id) { Loaded *l = *pp; *pp = l->next; free(l->text); free(l); }
        else pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&g_lock);
}

void preset_slot(Bl *x, int slot)
{
    x->pr.slot = (uint8_t)(slot < 0 ? 0 : slot >= NUM_SLOTS ? NUM_SLOTS - 1 : slot);
    x->pr.armed = PR_NONE;
    status(x);
}

void preset_press(Bl *x, int b)
{
    const int s = x->pr.slot;
    const int armed = x->pr.armed && (int32_t)(x->pr.armUntil - x->clock) > 0 ? x->pr.armed : PR_NONE;
    switch (b) {
    case B_PR_SOUND: case B_PR_PERF:
        x->pr.kind = b == B_PR_SOUND ? PR_SOUND : PR_PERF;
        x->pr.armed = PR_NONE;
        status(x);
        break;
    case B_PR_SAVE:
        if (!x->dataDir[0]) { status(x); break; }
        if (armed == PR_SAVE) {
            x->pr.armed = PR_NONE;
            if (!save(x)) say(x, "SAVE FAILED", s, x->ui.track);
            else if (x->pr.kind == PR_SOUND) say(x, "SAVED T%2$d AS SOUND %1$02d", s, x->ui.track);
            else say(x, "SAVED PERF %02d", s, 0);
            ui_message(x, "SAV", 0, 1000);
        } else {
            x->pr.armed = PR_SAVE;
            x->pr.armUntil = x->clock + ARM_FRAMES;
            if (x->pr.kind == PR_SOUND)
                say(x, exists(x, PR_SOUND, s) ? "OVERWRITE SOUND %02d WITH T%d?  TAP SAVE AGAIN" : "SAVE T%2$d AS SOUND %1$02d?  TAP SAVE AGAIN", s, x->ui.track);
            else say(x, exists(x, PR_PERF, s) ? "OVERWRITE PERF %02d?  TAP SAVE AGAIN" : "SAVE PERF %02d?  TAP SAVE AGAIN", s, 0);
        }
        break;
    case B_PR_LOAD:
        if (!exists(x, x->pr.kind, s)) { x->pr.armed = PR_NONE; status(x); break; }
        if (x->pr.kind == PR_SOUND || armed == PR_LOAD) {   /* a sound loads at once: try slot after slot */
            x->pr.armed = PR_NONE;
            if (load(x)) say(x, x->pr.kind == PR_SOUND ? "LOADING SOUND %02d" : "LOADING PERF %02d", s, x->ui.track);
            ui_message(x, "LOD", 0, 1000);
        } else {
            x->pr.armed = PR_LOAD;
            x->pr.armUntil = x->clock + ARM_FRAMES;
            say(x, "LOAD PERF %02d?  IT REPLACES EVERYTHING.  TAP LOAD AGAIN", s, 0);
        }
        break;
    default: break;
    }
}

int preset_led(Bl *x, int b)
{
    const int armed = x->pr.armed && (int32_t)(x->pr.armUntil - x->clock) > 0 ? x->pr.armed : PR_NONE;
    const int blink = (x->clock / 11025u) & 1u;
    switch (b) {
    case B_PR_SOUND: return x->pr.kind == PR_SOUND;
    case B_PR_PERF: return x->pr.kind == PR_PERF;
    case B_PR_SAVE: return armed == PR_SAVE && blink;
    case B_PR_LOAD: return (armed == PR_LOAD && blink) || x->pr.loading;
    default: return 0;
    }
}
