/* LXRXXL ZYNTH -- the plugin engine (mpc-vst-plugins wrapper/engine.h): parameters, MIDI and audio for one instance.
 *
 * The knobs and switches of both pages are the selected track's sound (as on the DB-01, the panel edits the track you
 * pick), the delay's and the gate/duck's. The keys are text parameters whose "<key>_on" is their LED (the wrapper polls
 * those every 10 ms). The display is six read-only pictures (three characters, three dots) the engine moves; the
 * wrapper reports them with the display_rev poll (HAS_DISPLAY_REV). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "engine.h"
#include "plugin_dir.h"
#include "bl.h"
#include "param_ids.h"
#include "param_map.h"

typedef struct {
    Bl x;
    char shown[4];
    uint8_t shownDots;
} Inst;

/* the data folder (presets): MODULE_DIR, outside the plugin folder so presets survive updates; else beside the .so */
static int writable_dir(const char *d)
{
    char probe[700], tmp[600];
    char *p;
    FILE *f;
    snprintf(tmp, sizeof tmp, "%s", d);
    for (p = tmp + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(tmp, 0755); *p = '/'; }
    mkdir(tmp, 0755);
    snprintf(probe, sizeof probe, "%s/.probe", d);
    f = fopen(probe, "w");
    if (!f) return 0;
    fclose(f);
    remove(probe);
    return 1;
}

static void *create(const char *dataDir)
{
    Inst *in = calloc(1, sizeof *in);
    if (!in) return NULL;
    if (dataDir && dataDir[0] && writable_dir(dataDir)) snprintf(in->x.dataDir, sizeof in->x.dataDir, "%s", dataDir);
    else {
        char here[400];
        if (mpc_plugin_dir(here, sizeof here)) {
            snprintf(in->x.dataDir, sizeof in->x.dataDir, "%s/LXRXXL ZYNTH Data", here);
            if (!writable_dir(in->x.dataDir)) in->x.dataDir[0] = 0;
        }
    }
    bl_init(&in->x);
    {
        int t, ok = in->x.fx.buf[0] && in->x.fx.buf[1];
        for (t = 0; t < NUM_TRACKS; t++) {
            voice_attach(&in->x.v[t]);
            if (!in->x.v[t].eng[0].mi || !in->x.v[t].eng[1].mi) ok = 0;
        }
        if (!ok) {
            for (t = 0; t < NUM_TRACKS; t++) voice_detach(&in->x.v[t]);
            fx_free(&in->x.fx);
            free(in);
            return NULL;
        }
    }
    return in;
}

static void destroy(void *p)
{
    Inst *in = p;
    int t;
    preset_forget(&in->x);
    fx_free(&in->x.fx);
    for (t = 0; t < NUM_TRACKS; t++) voice_detach(&in->x.v[t]);
    free(in);
}

static void midi(void *p, const uint8_t *m, int len)
{
    Inst *in = p;
    const uint8_t st = m[0] & 0xF0, ch = m[0] & 0x0F;
    if (len < 3) return;
    if (st == 0x90) ui_midiNote(&in->x, ch, m[1] & 127, m[2] & 127);
    else if (st == 0x80) ui_midiNote(&in->x, ch, m[1] & 127, 0);
    else if (st == 0xB0 && (m[1] == 120 || m[1] == 123)) {   /* all sound / notes off */
        int t;
        for (t = 0; t < NUM_TRACKS; t++) { voice_noteOff(&in->x.v[t]); in->x.ui.held[t] = 0; }
    }
}

/* the parameter a key names; *on: the key asked for its LED ("<key>_on") */
static int key_index(const char *k, int *on)
{
    int i;
    size_t n = strlen(k);
    *on = 0;
    for (i = 0; i < P_COUNT; i++) if (!strcmp(k, PARAM_KEYS[i])) return i;
    if (n > 3 && !strcmp(k + n - 3, "_on")) {
        for (i = 0; i < P_COUNT; i++)
            if (strlen(PARAM_KEYS[i]) == n - 3 && !strncmp(k, PARAM_KEYS[i], n - 3)) { *on = 1; return i; }
    }
    return -1;
}

static float read_value(const ParamMap *m, const char *v)
{
    const float f = (float)atof(v);
    if (m->nopts) {
        int i = atoi(v);
        return (float)(i < 0 ? 0 : i >= m->nopts ? m->nopts - 1 : i);
    }
    return clampf(f, 0, 1);
}

/* the parameters the readout follows: values (not keys or the display) */
static int m_kind_editable(int i)
{
    const int k = PARAM_MAP[i].kind;
    return k == PK_SOUND || k == PK_GLOBAL || k == PK_MIX ||
           (k == PK_UI && PARAM_MAP[i].id != UI_TRACK && PARAM_MAP[i].id != UI_SLOT);
}

static void set_param(void *p, const char *k, const char *v)
{
    Inst *in = p;
    Bl *x = &in->x;
    int i, on;
    if (!strcmp(k, "state")) { store_deserialize(x, v); return; }
    if (!strcmp(k, "host_time")) {
        int playing = 0;
        double ppq = 0, tempo = 0;
        if (sscanf(v, "%d %lf %lf", &playing, &ppq, &tempo) == 3) seq_hostTime(x, playing, ppq, tempo);
        return;
    }
    i = key_index(k, &on);
    if (i < 0 || on) return;
    {
        const ParamMap *m = &PARAM_MAP[i];
        switch (m->kind) {
        case PK_SOUND: {
            Sound *snd = &x->p.snd[x->ui.track];
            if (m->id == SP_VOICE) sound_setVoice(snd, (int)read_value(m, v));
            else if (m->id >= SP_V1 && m->id <= SP_V20) sound_setSlider(snd, m->id - SP_V1, read_value(m, v));
            else { snd->v[m->id] = read_value(m, v); sound_sync(snd); }
            ui_soundTouched(x, m->id);
            break;
        }
        case PK_GLOBAL:
            x->p.g[m->id] = read_value(m, v);
            ui_globalTouched(x, m->id);
            break;
        case PK_UI:
            if (m->id == UI_TRACK) ui_press(x, B_TRK1 + (int)read_value(m, v));
            else if (m->id == UI_SLOT) preset_slot(x, (int)read_value(m, v));
            else if (m->id == UI_TIMBRE || m->id == UI_MORPH) {   /* the voice's slider behind the front knob */
                Sound *snd = &x->p.snd[x->ui.track];
                const int k = ENG_DEFS[(int)(snd->v[SP_VOICE] + 0.5f) % NUM_ENGINES].macro[m->id == UI_MORPH];
                sound_setSlider(snd, k, read_value(m, v));
                ui_soundTouched(x, SP_V1 + k);
            }
            else if (m->id == UI_RND_AMT || m->id == UI_SRND_AMT) {
                char pc[8];
                const uint8_t a = (uint8_t)read_value(m, v);
                if (m->id == UI_RND_AMT) x->rnd2.amt = a; else x->rnd2.seqAmt = a;
                snprintf(pc, sizeof pc, "%d", a * 10);
                ui_message(x, pc, 0, 800);
            }
            else ui_knob(x, m->id == UI_SEL ? 0 : 1, (float)atof(v));
            break;
        case PK_MIX: {
            const float f = read_value(m, v);
            const int t = m->id % 8, w = m->id / 8;
            if (m->id == 24) x->p.master = f;
            else if (w == 0) x->p.snd[t].v[SP_LEVEL] = f;
            else if (w == 1) x->p.snd[t].v[SP_PAN] = f;
            else x->p.send[t] = f;
            break;
        }
        case PK_BUTTON:
            /* a tap: whatever value the host sends is a press (the key's picture is its LED, which the host toggled on
             * its own: report the real LED again a moment after the tap so the picture follows it) */
            ui_press(x, m->id);
            x->ui.pressFlip[m->id] = x->clock + 600;
            break;
        default: break;   /* the display's pictures: read-only */
        }
    }
    if (m_kind_editable(i)) ui_touch(x, i);
    x->displayRev++;
}

static int put(char *buf, int n, const char *s)
{
    if (n <= 0) return 0;
    snprintf(buf, (size_t)n, "%s", s);
    return (int)strlen(s) > 0 ? (int)strlen(s) : 0;
}

static int get_param(void *p, const char *k, char *buf, int n)
{
    Inst *in = p;
    Bl *x = &in->x;
    char t[64], t2[32];
    int i, on;
    if (!strcmp(k, "state")) {
        int len = 0, r = 0;
        char *s = store_serialize(x, &len);
        if (s) {
            if (len < n) memcpy(buf, s, (size_t)len + 1);
            r = len + 1;   /* the wrapper grows its buffer when this is larger than what it gave */
            free(s);
        }
        return r;
    }
    if (!strcmp(k, "display_rev")) { snprintf(t, sizeof t, "%u", (unsigned)x->displayRev); return put(buf, n, t); }
    i = key_index(k, &on);
    if (i < 0) return 0;
    {
        const ParamMap *m = &PARAM_MAP[i];
        switch (m->kind) {
        case PK_SOUND: {
            const float v = x->p.snd[x->ui.track].v[m->id];
            if (on) return 0;
            if (m->nopts) snprintf(t, sizeof t, "%d", (int)v); else snprintf(t, sizeof t, "%.5f", v);
            return put(buf, n, t);
        }
        case PK_GLOBAL: {
            const float v = x->p.g[m->id];
            if (on) return 0;
            if (m->nopts) snprintf(t, sizeof t, "%d", (int)v); else snprintf(t, sizeof t, "%.5f", v);
            return put(buf, n, t);
        }
        case PK_UI:
            if (on) return 0;
            if (m->id == UI_TRACK) snprintf(t, sizeof t, "%d", x->ui.track);
            else if (m->id == UI_SLOT) snprintf(t, sizeof t, "%d", x->pr.slot);
            else if (m->id == UI_RND_AMT) snprintf(t, sizeof t, "%d", x->rnd2.amt);
            else if (m->id == UI_SRND_AMT) snprintf(t, sizeof t, "%d", x->rnd2.seqAmt);
            else if (m->id == UI_TIMBRE || m->id == UI_MORPH) {
                const Sound *snd = &x->p.snd[x->ui.track];
                const int k = ENG_DEFS[(int)(snd->v[SP_VOICE] + 0.5f) % NUM_ENGINES].macro[m->id == UI_MORPH];
                snprintf(t, sizeof t, "%.5f", snd->v[SP_V1 + k]);
            }
            else snprintf(t, sizeof t, "%.5f", ui_knobGet(x, m->id == UI_SEL ? 0 : 1));
            return put(buf, n, t);
        case PK_MIX: {
            const int t = m->id % 8, w = m->id / 8;
            const float v = m->id == 24 ? x->p.master : w == 0 ? x->p.snd[t].v[SP_LEVEL] : w == 1 ? x->p.snd[t].v[SP_PAN] : x->p.send[t];
            if (on) return 0;
            snprintf(t2, sizeof t2, "%.5f", v);
            return put(buf, n, t2);
        }
        case PK_SHOW: {
            char d[4];
            uint8_t dots;
            if (on) return 0;
            if (m->id == SH_READOUT) { char ro[96]; ui_readout(x, ro, sizeof ro); return put(buf, n, ro); }
            if (m->id == SH_SNDNAME) {
                const int ty = (int)(x->p.snd[x->ui.track].v[SP_VOICE] + 0.5f) % NUM_ENGINES;
                return put(buf, n, sound_starterName(ty, x->p.starter[x->ui.track][ty]));
            }
            ui_display(x, d, &dots);
            if (m->id <= SH_DISP3) {
                const char *g = strchr(GLYPHS, d[m->id - SH_DISP1]);
                snprintf(t, sizeof t, "%d", g && d[m->id - SH_DISP1] ? (int)(g - GLYPHS) : 0);
            } else snprintf(t, sizeof t, "%d", (dots >> (m->id - SH_DOT1)) & 1);
            return put(buf, n, t);
        }
        case PK_BUTTON:
            if (on) {
                int led = ui_led(x, m->id);
                if (x->ui.pressFlip[m->id] && (int32_t)(x->ui.pressFlip[m->id] - x->clock) > 0) led = !led;
                else x->ui.pressFlip[m->id] = 0;
                return put(buf, n, led ? "1" : "0");
            }
            return put(buf, n, " ");
        default: return 0;
        }
    }
}

/* Denormal numbers (the tails of decaying strings, resonators and filters) are many times slower to compute: the
 * audio runs with flush-to-zero on, and the host's setting is put back afterwards. */
#if defined(__arm__)
static inline unsigned fz_on(void)
{
    unsigned old;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(old));
    __asm__ volatile("vmsr fpscr, %0" : : "r"(old | (1u << 24)));
    return old;
}
static inline void fz_off(unsigned old) { __asm__ volatile("vmsr fpscr, %0" : : "r"(old)); }
#elif defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
static inline unsigned fz_on(void) { const unsigned old = _mm_getcsr(); _mm_setcsr(old | 0x8040u); return old; }
static inline void fz_off(unsigned old) { _mm_setcsr(old); }
#else
static inline unsigned fz_on(void) { return 0; }
static inline void fz_off(unsigned old) { (void)old; }
#endif

static void render(void *p, int16_t *out, int frames)
{
    const unsigned fpu = fz_on();
    Inst *in = p;
    float L[128], R[128];
    int i;
    char d[4];
    uint8_t dots;
    if (frames > 128) frames = 128;
    preset_poll(&in->x);
    bl_render(&in->x, L, R, frames);
    /* the display changed by itself (a message ran out, the tempo moved): report it */
    ui_display(&in->x, d, &dots);
    if (memcmp(d, in->shown, 3) || dots != in->shownDots) {
        memcpy(in->shown, d, 4);
        in->shownDots = dots;
        in->x.displayRev++;
    }
    for (i = 0; i < frames; i++) {
        const float l = L[i] * 32767.f, r = R[i] * 32767.f;
        out[2 * i] = (int16_t)(l > 32767.f ? 32767 : l < -32768.f ? -32768 : (int)l);
        out[2 * i + 1] = (int16_t)(r > 32767.f ? 32767 : r < -32768.f ? -32768 : (int)r);
    }
    fz_off(fpu);
}

static const mpc_engine_t ENGINE = {create, destroy, midi, set_param, get_param, render, NULL};
const mpc_engine_t *mpc_engine(void) { return &ENGINE; }
