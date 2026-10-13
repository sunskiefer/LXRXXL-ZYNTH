/* LXRXXL ZYNTH -- the front panel: the keys, the two encoders and the display.
 *
 * TRACK 1-8 pick the track the knobs and the step keys edit. The 16 step keys toggle the steps of the bar shown (BAR < >)
 * and select the step BACK/NOTE gives its note (SELECT/GATE walks the steps). SHIFT, then a step key, is the function
 * printed under it (the DB-01's): ACC, SLIDE, MOD and PITCH ENV make the step keys set those flags; LAST STEP takes the
 * track's length from the next key; PLAY MODE, TRANSP, SCALE, ARPEGG and LFO are edited with BACK/NOTE (and the keys);
 * RND PATT, CLEAR, COPY and PASTE act at once; BANK then PATTERN pick the pattern. SHIFT again leaves a function. SHIFT +
 * MUTE is RESET (the factory project). The display shows the tempo (its last dot lit: MPC's tempo), a value while it
 * changes and the function in use. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bl.h"
#include "param_map.h"

#define MS(ms) ((uint32_t)((ms) * 44.1))

extern const char *const LFO_DIV_NAMES[];
float voice_fmRatio(float v);
float voice_lfoHz(float v);
float voice_relSeconds(float v);

static int voice_of(const Sound *s) { return (int)clampf(s->v[SP_VOICE] + 0.5f, 0, NUM_ENGINES - 1); }

/* a time in the display's three characters: "12" ms below a second, "1.5" seconds above */
static void time_text(float sec, char *buf, int full)
{
    if (sec < 0.995f) snprintf(buf, 16, full ? "%d ms" : "%d", (int)lroundf(sec * 1000.f));
    else snprintf(buf, 16, full ? "%.1f s" : "%.1f", sec);
}

static const char *const PM_NAMES[PM_COUNT] = {"FWD", "REV", "PNG", "RND"};
static const char *const ARP_NAMES[ARP_COUNT] = {"OFF", "UP", "DWN", "U-D", "RND"};

void ui_init(Bl *x)
{
    memset(&x->ui, 0, sizeof x->ui);
    x->ui.mode = MD_NORMAL;
    x->ui.encPos[0] = x->ui.encPos[1] = -1.f;
    x->displayRev++;
}

/* ---------------------------------------------------------------- the display */
/* text with dots ("1.5", "C.3") into 3 cells, right-aligned; a dot belongs to the character before it */
static void pack(const char *s, char out[4], uint8_t *dots)
{
    char c[8];
    uint8_t d[8];
    int n = 0, i;
    memset(d, 0, sizeof d);
    for (; *s && n < 7; s++) {
        if (*s == '.' && n > 0) { d[n - 1] = 1; continue; }
        if (*s == '.') { c[n] = ' '; d[n] = 1; n++; continue; }
        c[n] = (char)(*s >= 'a' && *s <= 'z' ? *s - 32 : *s);
        d[n] = 0;
        n++;
    }
    while (n > 3) {   /* too long: keep the first three */
        n = 3;
    }
    *dots = 0;
    for (i = 0; i < 3; i++) {
        const int k = i - (3 - n);
        out[i] = k >= 0 ? c[k] : ' ';
        if (k >= 0 && d[k]) *dots |= (uint8_t)(1u << i);
    }
    out[3] = 0;
}

void ui_message(Bl *x, const char *text, uint8_t dots, int ms)
{
    uint8_t d;
    pack(text, x->ui.msg, &d);
    x->ui.msgDots = (uint8_t)(d | dots);
    x->ui.msgUntil = x->clock + MS(ms);
    x->displayRev++;
}

static void note_name(int note, char *buf)
{
    static const char *const N[12] = {"C", "C.", "D", "D.", "E", "F", "F.", "G", "G.", "A", "A.", "B"};   /* a dot: sharp */
    const int oct = note / 12 - 2;   /* MPC's names: 60 = C3 */
    if (oct < 0) snprintf(buf, 8, "%s-%d", N[note % 12], -oct);
    else snprintf(buf, 8, "%s%d ", N[note % 12], oct);
}

static void short_label(const char *opt, char *buf)
{
    int i, n = 0;
    for (i = 0; opt[i] && n < 3; i++) {
        char c = opt[i];
        if (c == '&') c = '-';
        buf[n++] = c;
    }
    buf[n] = 0;
}

static void fmt_number(float v, char *buf)
{
    if (v >= 9.95f) snprintf(buf, 8, "%d", (int)lroundf(v));
    else if (v >= 0.995f) snprintf(buf, 8, "%.1f", v);
    else snprintf(buf, 8, ".%02d", (int)lroundf(v * 100.f) % 100);
}

/* a sound parameter's value as the display shows it */
static void sound_text(Bl *x, int sp, char *buf)
{
    const Sound *s = &x->p.snd[x->ui.track];
    const float v = s->v[sp];
    if (sp == SP_VOICE) { snprintf(buf, 8, "%s", ENG_DEFS[voice_of(s)].shortName); return; }
    if (sp >= SP_V1 && sp <= SP_V6) { eng_value_text(voice_of(s), sp - SP_V1, v, buf, 8, 0); return; }
    if (sp == SP_VCA_REL) { time_text(voice_relSeconds(v), buf, 0); return; }
    if (SOUND_DEFS[sp].nopts) { short_label(SOUND_DEFS[sp].opts[(int)v], buf); return; }
    switch (sp) {
    case SP_PAN: {
        const int p = (int)lroundf((v - 0.5f) * 100.f);
        if (p == 0) strcpy(buf, " C ");
        else snprintf(buf, 8, "%c%d", p < 0 ? 'L' : 'R', p < 0 ? -p : p);
        return;
    }
    case SP_FM_RATIO: fmt_number(voice_fmRatio(v), buf); return;
    case SP_LFO_RATE:
        if (s->v[SP_MOD_SYNC] >= 1.f) { strcpy(buf, LFO_DIV_NAMES[(int)lroundf(clampf(v, 0, 1) * 9.f)]); return; }
        fmt_number(voice_lfoHz(v), buf);
        return;
    default: snprintf(buf, 8, "%d", (int)lroundf(v * 100.f)); return;
    }
}

static void global_text(Bl *x, int g, char *buf)
{
    static const char *const F[11] = {"/8", "/6", "/4", "/3", "/2", "X1", "X2", "X3", "X4", "X6", "X8"};
    const float v = x->p.g[g];
    if (g == G_GD_CH) { snprintf(buf, 8, "C%d", (int)v + 1); return; }
    if (GLOBAL_DEFS[g].nopts) { short_label(GLOBAL_DEFS[g].opts[(int)v], buf); return; }
    if (g == G_DLY_TIME) {
        if (x->p.dlySync) strcpy(buf, F[fx_syncFactorIndex(v)]);
        else snprintf(buf, 8, "%d", (int)lroundf(fx_delaySeconds(x) * 1000.f));
        return;
    }
    snprintf(buf, 8, "%d", (int)lroundf(v * 100.f));
}

/* full value text for the readout */
static void full_text(Bl *x, int kind, int id, char *buf, int n)
{
    char b[16];
    if (kind == PK_SOUND) {
        const Sound *s = &x->p.snd[x->ui.track];
        const float v = s->v[id];
        if (id == SP_VOICE) { snprintf(buf, (size_t)n, "%s", ENG_DEFS[voice_of(s)].name); return; }
        if (id >= SP_V1 && id <= SP_V6) { eng_value_text(voice_of(s), id - SP_V1, v, buf, n, 1); return; }
        if (id == SP_VCA_REL) { time_text(voice_relSeconds(v), buf, 1); return; }
        if (SOUND_DEFS[id].nopts) { snprintf(buf, (size_t)n, "%s", SOUND_DEFS[id].opts[(int)v]); return; }
        sound_text(x, id, b);
        if (id == SP_LFO_RATE && x->p.snd[x->ui.track].v[SP_MOD_SYNC] < 1.f) snprintf(buf, (size_t)n, "%s Hz", b);
        else if (id == SP_FM_RATIO) snprintf(buf, (size_t)n, "x %s", b);
        else snprintf(buf, (size_t)n, "%s", b);
        return;
    }
    if (kind == PK_GLOBAL) {
        const float v = x->p.g[id];
        if (id == G_GD_CH) { snprintf(buf, (size_t)n, "CH %d", (int)v + 1); return; }
        if (GLOBAL_DEFS[id].nopts) { snprintf(buf, (size_t)n, "%s", GLOBAL_DEFS[id].opts[(int)v]); return; }
        global_text(x, id, b);
        snprintf(buf, (size_t)n, id == G_DLY_TIME && !x->p.dlySync ? "%s ms" : "%s", b);
        return;
    }
    if (kind == PK_MIX) {
        const int t = id % 8, w = id / 8;
        const float v = id == 24 ? x->p.master : w == 0 ? x->p.snd[t].v[SP_LEVEL] : w == 1 ? x->p.snd[t].v[SP_PAN] : x->p.send[t];
        if (id != 24 && w == 1) {
            const int p = (int)lroundf((v - 0.5f) * 100.f);
            if (p == 0) snprintf(buf, (size_t)n, "C");
            else snprintf(buf, (size_t)n, "%c %d", p < 0 ? 'L' : 'R', p < 0 ? -p : p);
        } else snprintf(buf, (size_t)n, "%d", (int)lroundf(v * 100.f));
        return;
    }
    snprintf(buf, (size_t)n, "-");
}

void ui_touch(Bl *x, int param) { x->touched = (int16_t)param; x->displayRev++; }

void ui_readout(Bl *x, char *buf, int n)
{
    const int p = x->touched;
    char v[48], nm[48];
    if (p == TOUCH_PRESET) { snprintf(buf, (size_t)n, "%s", x->pr.msg); return; }
    if (p < 0 || p >= P_COUNT) { snprintf(buf, (size_t)n, "TRACK %d", x->ui.track + 1); return; }
    {
        const ParamMap *m = &PARAM_MAP[p];
        if (m->kind == PK_UI) {
            const Step *st = &bl_edit(x)->s[x->ui.sel];
            if (m->id == UI_SEL) snprintf(buf, (size_t)n, "T%d   STEP %d", x->ui.track + 1, x->ui.sel + 1);
            else if (m->id == UI_NOTE) {
                char nn[8];
                static const char *const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                snprintf(nn, sizeof nn, "%s%d", N[st->note % 12], st->note / 12 - 2);
                snprintf(buf, (size_t)n, "T%d   STEP %d   %s", x->ui.track + 1, x->ui.sel + 1, nn);
            } else if (m->id == UI_RND_AMT) snprintf(buf, (size_t)n, "SOUND RANDOM AMOUNT   %d %%", x->rnd2.amt * 10);
            else if (m->id == UI_SRND_AMT) snprintf(buf, (size_t)n, "STEP RANDOM AMOUNT   %d %%", x->rnd2.seqAmt * 10);
            else snprintf(buf, (size_t)n, "TRACK %d", x->ui.track + 1);
            return;
        }
        full_text(x, m->kind, m->id, v, sizeof v);
        if (m->kind == PK_SOUND && m->id >= SP_V1 && m->id <= SP_V6) {
            const int ty = voice_of(&x->p.snd[x->ui.track]);
            snprintf(nm, sizeof nm, "T%d   %s %s", x->ui.track + 1, ENG_DEFS[ty].name, ENG_DEFS[ty].slider[m->id - SP_V1]);
        } else if (m->kind == PK_SOUND) snprintf(nm, sizeof nm, "T%d   %s", x->ui.track + 1, SOUND_DEFS[m->id].name);
        else if (m->kind == PK_GLOBAL) snprintf(nm, sizeof nm, "%s", GLOBAL_DEFS[m->id].name);
        else if (m->kind == PK_MIX) {
            static const char *const W[3] = {"LEVEL", "PAN", "DLY SEND"};
            if (m->id == 24) snprintf(nm, sizeof nm, "MASTER");
            else snprintf(nm, sizeof nm, "T%d   %s", m->id % 8 + 1, W[m->id / 8]);
        } else nm[0] = 0;
        snprintf(buf, (size_t)n, "%s   %s", nm, v);
    }
}

void ui_soundTouched(Bl *x, int sp)
{
    char b[16];
    sound_text(x, sp, b);
    ui_message(x, b, 0, 1500);
}

void ui_globalTouched(Bl *x, int g)
{
    char b[16];
    global_text(x, g, b);
    ui_message(x, b, 0, 1500);
}

static void mode_text(Bl *x, char *b)
{
    const TrackPat *tp = bl_edit(x);
    switch (x->ui.mode) {
    case MD_ACC: strcpy(b, "ACC"); break;
    case MD_SLIDE: strcpy(b, "SLD"); break;
    case MD_MOD: strcpy(b, "MOD"); break;
    case MD_PENV: strcpy(b, "PEN"); break;
    case MD_LAST: snprintf(b, 8, "L%d", tp->len); break;
    case MD_PLAYMODE: strcpy(b, PM_NAMES[tp->playMode % PM_COUNT]); break;
    case MD_TRANSP: snprintf(b, 8, tp->transp > 0 ? "+%d" : "%d", tp->transp); break;
    case MD_SCALE: strcpy(b, seq_scaleName(tp->scale)); break;
    case MD_ARP: strcpy(b, ARP_NAMES[tp->arp % ARP_COUNT]); break;
    case MD_LFO: short_label(SOUND_DEFS[SP_MOD_WAVE].opts[(int)x->p.snd[x->ui.track].v[SP_MOD_WAVE]], b); break;
    case MD_BANK: snprintf(b, 8, "BK%c", 'A' + x->ui.bank); break;
    case MD_PATTERN: snprintf(b, 8, "%c%02d", 'A' + x->ui.bank, (x->patSlot % 16) + 1); break;
    default: b[0] = 0; break;
    }
}

void ui_display(Bl *x, char out[4], uint8_t *dots)
{
    char b[16];
    if (x->ui.msgUntil && (int32_t)(x->ui.msgUntil - x->clock) > 0) {
        memcpy(out, x->ui.msg, 4);
        *dots = x->ui.msgDots;
        return;
    }
    x->ui.msgUntil = 0;
    if (x->ui.mode != MD_NORMAL) { mode_text(x, b); pack(b, out, dots); return; }
    if (x->ui.shift) { pack("SHF", out, dots); return; }
    /* the tempo; the last dot lit: MPC's tempo */
    snprintf(b, sizeof b, "%d", (int)lroundf(bl_tempo(x)));
    pack(b, out, dots);
    if (x->p.bpm == 0) *dots |= 4;
}

/* ---------------------------------------------------------------- the keys */
static int track_len(Bl *x) { const TrackPat *tp = bl_edit(x); return tp->len ? tp->len : 16; }

static void flash(Bl *x, int b) { x->ui.flash[b] = x->clock + MS(150); }

static void step_key(Bl *x, int k)
{
    TrackPat *tp = bl_edit(x);
    const int idx = x->ui.bar * 16 + k;
    Step *st = idx < NUM_STEPS ? &tp->s[idx] : NULL;
    char b[16];
    switch (x->ui.mode) {
    case MD_NORMAL:
        if (!st || idx >= track_len(x)) { ui_message(x, "---", 0, 600); return; }
        st->f ^= ST_GATE;
        if (!st->note) st->note = 36;
        x->ui.sel = (uint8_t)idx;
        if (st->f & ST_GATE) { note_name(st->note, b); ui_message(x, b, 0, 1200); }
        else ui_message(x, "OFF", 0, 600);
        break;
    case MD_ACC: case MD_SLIDE: case MD_MOD: case MD_PENV: {
        static const uint8_t fl[] = {0, ST_ACC, ST_SLIDE, ST_MOD, ST_PENV};
        if (!st || idx >= track_len(x)) return;
        st->f ^= fl[x->ui.mode];
        x->ui.sel = (uint8_t)idx;
        break;
    }
    case MD_LAST:
        tp->len = (uint8_t)(idx + 1);
        x->ui.mode = MD_NORMAL;
        snprintf(b, sizeof b, "L%d", tp->len);
        ui_message(x, b, 0, 1000);
        break;
    case MD_PLAYMODE: if (k < PM_COUNT) tp->playMode = (uint8_t)k; break;
    case MD_TRANSP: if (k <= 12) tp->transp = (int8_t)k; break;
    case MD_SCALE: if (k < NUM_SCALES) tp->scale = (uint8_t)k; break;
    case MD_ARP: if (k < ARP_COUNT) tp->arp = (uint8_t)k; break;
    case MD_LFO: {
        Sound *s = &x->p.snd[x->ui.track];
        if (k < 5) { s->v[SP_MOD_WAVE] = (float)k; ui_soundTouched(x, SP_MOD_WAVE); }
        else if (k >= 8 && k < 12) { s->v[SP_MOD_DEST] = (float)(k - 8); ui_soundTouched(x, SP_MOD_DEST); }
        else if (k == 15) { s->v[SP_MOD_SYNC] = s->v[SP_MOD_SYNC] >= 1.f ? 0.f : 1.f; ui_soundTouched(x, SP_MOD_SYNC); }
        break;
    }
    case MD_BANK:
        if (k < NUM_BANKS) { x->ui.bank = (uint8_t)k; x->ui.mode = MD_PATTERN; }
        break;
    case MD_PATTERN:
        seq_queuePattern(x, (uint8_t)(x->ui.bank * 16 + k));
        x->ui.mode = MD_NORMAL;
        snprintf(b, sizeof b, "%c%02d", 'A' + x->ui.bank, k + 1);
        ui_message(x, b, 0, 1000);
        break;
    default: break;
    }
    x->displayRev++;
}

static void function_key(Bl *x, int k)
{
    TrackPat *tp = bl_edit(x);
    int i;
    x->ui.shift = 0;
    switch (k) {
    case 0: x->ui.mode = MD_ACC; break;
    case 1: x->ui.mode = MD_SLIDE; break;
    case 2: x->ui.mode = MD_MOD; break;
    case 3: x->ui.mode = MD_PENV; break;
    case 4: x->ui.mode = MD_LAST; break;
    case 5: x->ui.mode = MD_PLAYMODE; break;
    case 6: x->ui.mode = MD_TRANSP; break;
    case 7: x->ui.mode = MD_SCALE; break;
    case 8: x->ui.mode = MD_ARP; break;
    case 9: x->ui.mode = MD_LFO; break;
    case 10: seq_randomTrack(x, x->ui.track); ui_message(x, "RND", 0, 800); break;
    case 11:
        for (i = 0; i < NUM_STEPS; i++) tp->s[i].f = 0;
        ui_message(x, "CLR", 0, 800);
        break;
    case 12: x->clip.t[0] = *tp; x->clipValid = 1; ui_message(x, "CPY", 0, 800); break;
    case 13:
        if (x->clipValid) { *tp = x->clip.t[0]; ui_message(x, "PST", 0, 800); }
        else ui_message(x, "---", 0, 800);
        break;
    case 14: x->ui.mode = MD_BANK; break;
    case 15: x->ui.bank = (uint8_t)(x->patSlot / 16); x->ui.mode = MD_PATTERN; break;
    default: break;
    }
    flash(x, B_S1 + k);
    x->displayRev++;
}

static void tap_tempo(Bl *x)
{
    const double now = x->clock / 44100.0;
    const double dt = now - x->ui.lastTap;
    char b[16];
    if (x->ui.lastTap > 0 && dt > 0.2 && dt < 2.0) {
        float bpm = (float)(60.0 / dt);
        if (x->p.bpm > 0 && x->ui.taps > 1) bpm = 0.5f * (bpm + x->p.bpm);
        x->p.bpm = clampf(bpm, 40.f, 240.f);
        x->ui.taps++;
        snprintf(b, sizeof b, "%d", (int)lroundf(x->p.bpm));
        ui_message(x, b, 0, 1000);
    } else x->ui.taps = 1;
    x->ui.lastTap = now;
}

static void delay_tap(Bl *x)
{
    /* TAP SYNC: one tap syncs the delay to MPC's beat, taps in a row set the period; SHIFT + TAP SYNC: free (ms) */
    Fx *f = &x->fx;
    const double now = x->clock / 44100.0;
    const double dt = now - f->lastTap;
    if (x->ui.shift) { x->ui.shift = 0; x->p.dlySync = 0; ui_message(x, "FRE", 0, 1000); f->lastTap = 0; return; }
    if (f->lastTap > 0 && dt > 0.05 && dt < 2.0) {
        x->p.dlyTap = (float)dt;
        x->p.dlySync = 2;
        ui_message(x, "TAP", 0, 800);
    } else {
        x->p.dlySync = 1;
        ui_message(x, "SYN", 0, 800);
    }
    f->lastTap = now;
}

void ui_press(Bl *x, int b)
{
    char t[16];
    const uint8_t bit = (uint8_t)(1u << x->ui.track);
    if (b >= B_TRK1 && b <= B_TRK8) {
        x->ui.track = (uint8_t)(b - B_TRK1);
        x->ui.shift = 0;
        if (x->ui.sel >= track_len(x)) x->ui.sel = 0;
        if (x->ui.bar * 16 >= track_len(x)) x->ui.bar = 0;
        snprintf(t, sizeof t, "TR%d", x->ui.track + 1);
        ui_message(x, t, 0, 800);
        return;
    }
    if (b >= B_S1 && b <= B_S16) {
        if (x->ui.shift) function_key(x, b - B_S1);
        else step_key(x, b - B_S1);
        return;
    }
    switch (b) {
    case B_LINK:
        if (x->ui.track == 0) { ui_message(x, "---", 0, 800); break; }
        x->p.links ^= bit;
        ui_message(x, x->p.links & bit ? "LNK" : "OFF", 0, 800);
        break;
    case B_MUTE:
        if (x->ui.shift) {   /* RESET */
            bl_factory(x);
            ui_init(x);
            ui_message(x, "RST", 0, 1200);
            flash(x, B_MUTE);
            break;
        }
        x->p.mutes ^= bit;
        ui_message(x, x->p.mutes & bit ? "MUT" : "ON", 0, 800);
        break;
    case B_SHIFT:
        if (x->ui.mode != MD_NORMAL) { x->ui.mode = MD_NORMAL; x->ui.shift = 0; }
        else x->ui.shift = (uint8_t)!x->ui.shift;
        break;
    case B_PLAY:
        if (x->seq.running) seq_stop(x); else seq_start(x);
        break;
    case B_STOP: {
        int t2;
        seq_stop(x);
        for (t2 = 0; t2 < NUM_TRACKS; t2++) voice_noteOff(&x->v[t2]);
        flash(x, B_STOP);
        break;
    }
    case B_REC:
        x->seq.recording = (uint8_t)!x->seq.recording;
        ui_message(x, x->seq.recording ? "REC" : "OFF", 0, 800);
        break;
    case B_BARL: case B_BARR: {
        const int maxBar = (track_len(x) - 1) / 16;
        int bar = x->ui.bar + (b == B_BARR ? 1 : -1);
        bar = bar < 0 ? 0 : bar > maxBar ? maxBar : bar;
        x->ui.bar = (uint8_t)bar;
        snprintf(t, sizeof t, "BR%d", bar + 1);
        ui_message(x, t, 0, 800);
        flash(x, b);
        break;
    }
    case B_OCTD: case B_OCTU: {
        Step *st = &bl_edit(x)->s[x->ui.sel];
        int n = st->note + (b == B_OCTU ? 12 : -12);
        if (n >= 0 && n <= 127) st->note = (uint8_t)n;
        note_name(st->note, t);
        ui_message(x, t, 0, 1000);
        flash(x, b);
        break;
    }
    case B_TAP:
        if (x->ui.shift) { x->ui.shift = 0; x->p.bpm = 0; ui_message(x, "MPC", 0, 1000); }
        else tap_tempo(x);
        flash(x, B_TAP);
        break;
    case B_D_ADD: x->fx.add = (uint8_t)!x->fx.add; ui_message(x, x->fx.add ? "ADD" : "OFF", 0, 800); break;
    case B_D_HOLD:
        x->fx.hold = (uint8_t)!x->fx.hold;
        if (!x->fx.hold) x->fx.add = 0;
        ui_message(x, x->fx.hold ? "HLD" : "OFF", 0, 800);
        break;
    case B_D_REV: x->fx.rev = (uint8_t)!x->fx.rev; x->fx.revPos = 0; ui_message(x, x->fx.rev ? "REV" : "FWD", 0, 800); break;
    case B_D_TAP: delay_tap(x); flash(x, B_D_TAP); break;
    case B_GD_ON:
        x->p.gdOn = (uint8_t)!x->p.gdOn;
        ui_message(x, x->p.gdOn ? "ON" : "OFF", 0, 800);
        break;
    case B_MUTE1: case B_MUTE2: case B_MUTE3: case B_MUTE4: case B_MUTE5: case B_MUTE6: case B_MUTE7: case B_MUTE8:
        x->p.mutes ^= (uint8_t)(1u << (b - B_MUTE1));
        snprintf(t, sizeof t, "M%d", b - B_MUTE1 + 1);
        ui_message(x, x->p.mutes & (1u << (b - B_MUTE1)) ? t : "ON", 0, 600);
        break;
    case B_SOLO1: case B_SOLO2: case B_SOLO3: case B_SOLO4: case B_SOLO5: case B_SOLO6: case B_SOLO7: case B_SOLO8:
        x->p.solos ^= (uint8_t)(1u << (b - B_SOLO1));
        snprintf(t, sizeof t, "S%d", b - B_SOLO1 + 1);
        ui_message(x, x->p.solos & (1u << (b - B_SOLO1)) ? t : "OFF", 0, 600);
        break;
    case B_TRK_NEXT: ui_press(x, B_TRK1 + (x->ui.track + 1) % NUM_TRACKS); flash(x, B_TRK_NEXT); break;
    case B_PR_SOUND: case B_PR_PERF: case B_PR_SAVE: case B_PR_LOAD: preset_press(x, b); break;
    case B_V_RESET:
        if (x->ui.shift) {   /* MUTE ALL (a panic): every track muted and silenced; again: every track back */
            int t;
            if (x->p.mutes == 0xff) x->p.mutes = 0;
            else {
                x->p.mutes = 0xff;
                for (t = 0; t < NUM_TRACKS; t++) { voice_noteOff(&x->v[t]); voice_kill(&x->v[t]); x->ui.held[t] = 0; }
            }
            x->ui.shift = 0;
            ui_message(x, x->p.mutes ? "ALL" : "ON", 0, 1000);
            snprintf(x->pr.msg, sizeof x->pr.msg, x->p.mutes ? "MUTE ALL" : "ALL TRACKS ON");
        } else {             /* VOICE RESET: the selected track's sound back to the default (its voice kept) */
            Sound *snd = &x->p.snd[x->ui.track];
            const int ty = voice_of(snd);
            init_sound(snd);
            sound_setVoice(snd, ty);
            ui_message(x, "INI", 0, 1000);
            snprintf(x->pr.msg, sizeof x->pr.msg, "T%d   VOICE RESET", x->ui.track + 1);
        }
        x->touched = TOUCH_PRESET;   /* the readout shows what happened */
        flash(x, B_V_RESET);
        break;
    case B_V_PREV: case B_V_NEXT: {   /* MODULES: the selected track's voice, one step either way (round) */
        Sound *snd = &x->p.snd[x->ui.track];
        const int ty = (voice_of(snd) + (b == B_V_NEXT ? 1 : NUM_ENGINES - 1)) % NUM_ENGINES;
        sound_setVoice(snd, ty);
        ui_message(x, ENG_DEFS[ty].shortName, 0, 1000);
        snprintf(x->pr.msg, sizeof x->pr.msg, "T%d   VOICE   %s", x->ui.track + 1, ENG_DEFS[ty].name);
        x->touched = TOUCH_PRESET;
        flash(x, b);
        break;
    }
    case B_RND: case B_SRND: {   /* the randomizers: every track, or with SHIFT the selected one */
        const int one = x->ui.shift;
        const int pct = (b == B_RND ? x->rnd2.amt : x->rnd2.seqAmt) * 10;
        if (b == B_RND) { rnd_sounds(x, pct, one ? x->ui.track : -1); x->rnd2.flash = x->clock + 13230; }
        else { rnd_steps(x, pct, one ? x->ui.track : -1); x->rnd2.seqFlash = x->clock + 13230; }
        x->ui.shift = 0;
        ui_message(x, b == B_RND ? "RND" : "STP", 0, 800);
        if (one) snprintf(x->pr.msg, sizeof x->pr.msg, "T%d   %s RANDOM   %d %%", x->ui.track + 1, b == B_RND ? "SOUND" : "STEP", pct);
        else snprintf(x->pr.msg, sizeof x->pr.msg, "ALL TRACKS   %s RANDOM   %d %%", b == B_RND ? "SOUND" : "STEP", pct);
        x->touched = TOUCH_PRESET;
        break;
    }
    case B_CH_NEXT:
        x->p.g[G_GD_CH] = (float)(((int)x->p.g[G_GD_CH] + 1) % 16);
        ui_globalTouched(x, G_GD_CH);
        flash(x, B_CH_NEXT);
        break;
    default: break;
    }
    x->displayRev++;
}

int ui_led(Bl *x, int b)
{
    const uint8_t bit = (uint8_t)(1u << x->ui.track);
    const int blink = (x->clock / 11025u) & 1u;
    if (x->ui.flash[b] && (int32_t)(x->ui.flash[b] - x->clock) > 0) return 1;
    if (b >= B_TRK1 && b <= B_TRK8) {
        const int t = b - B_TRK1;
        if (t == x->ui.track) return 1;
        return (x->p.mutes & (1u << t)) ? 0 : (x->v[t].aStage && x->v[t].gate);   /* a playing track glows */
    }
    if (b >= B_S1 && b <= B_S16) {
        const TrackPat *tp = bl_edit(x);
        const int k = b - B_S1, idx = x->ui.bar * 16 + k;
        const Step *st = &tp->s[idx];
        const int inLen = idx < (tp->len ? tp->len : 16);
        if (x->ui.shift) return 0;
        switch (x->ui.mode) {
        case MD_NORMAL: {
            int on = inLen && (st->f & ST_GATE);
            if (x->seq.running && inLen && bl_trackPat(x, x->ui.track) == tp && x->seq.pos16[x->ui.track] == idx) on = !on;
            return on;
        }
        case MD_ACC: return inLen && (st->f & ST_ACC);
        case MD_SLIDE: return inLen && (st->f & ST_SLIDE);
        case MD_MOD: return inLen && (st->f & ST_MOD);
        case MD_PENV: return inLen && (st->f & ST_PENV);
        case MD_LAST: return inLen;
        case MD_PLAYMODE: return k == tp->playMode;
        case MD_TRANSP: return k == tp->transp;
        case MD_SCALE: return k == tp->scale;
        case MD_ARP: return k == tp->arp;
        case MD_LFO: {
            const Sound *s = &x->p.snd[x->ui.track];
            return k == (int)s->v[SP_MOD_WAVE] || k == 8 + (int)s->v[SP_MOD_DEST] || (k == 15 && s->v[SP_MOD_SYNC] >= 1.f);
        }
        case MD_BANK: return k == x->patSlot / 16;
        case MD_PATTERN:
            if (x->nextPat != 0xff && x->nextPat == x->ui.bank * 16 + k) return blink;
            return x->patSlot == x->ui.bank * 16 + k;
        default: return 0;
        }
    }
    switch (b) {
    case B_LINK: return (x->p.links & bit) != 0;
    case B_MUTE: return (x->p.mutes & bit) != 0;
    case B_SHIFT: return x->ui.shift || x->ui.mode != MD_NORMAL;
    case B_PLAY: return x->seq.running;
    case B_REC: return x->seq.recording;
    case B_TAP: {   /* the beat */
        const double beat = x->seq.running ? x->seq.pos / 4.0 : x->clock / 44100.0 * bl_tempo(x) / 60.0;
        return beat - floor(beat) < 0.15;
    }
    case B_D_ADD: return x->fx.add;
    case B_D_HOLD: return x->fx.hold;
    case B_D_REV: return x->fx.rev;
    case B_D_TAP: return x->p.dlySync != 0;
    case B_GD_ON: return x->p.gdOn;
    case B_PR_SOUND: case B_PR_PERF: case B_PR_SAVE: case B_PR_LOAD: return preset_led(x, b);
    case B_V_RESET: return x->p.mutes == 0xff;   /* lit while everything is muted */
    case B_RND: return (int32_t)(x->rnd2.flash - x->clock) > 0;
    case B_SRND: return (int32_t)(x->rnd2.seqFlash - x->clock) > 0;
    default:
        if (b >= B_MUTE1 && b <= B_MUTE8) return (x->p.mutes >> (b - B_MUTE1)) & 1;
        if (b >= B_SOLO1 && b <= B_SOLO8) return (x->p.solos >> (b - B_SOLO1)) & 1;
        return 0;
    }
}

/* ---------------------------------------------------------------- the encoders */
/* The data knobs are endless encoders: each stands where it was last turned, and only its own moves count. A move
 * steps the value by the knob's travel (the whole range per full turn); MPC moves a knob by a small step from where it
 * reads it (a wheel click 1/100, a Q-Link tick 1/128), and any move steps at least one value in its direction. So
 * turning SELECT/GATE to another step leaves BACK/NOTE where it stands (it used to jump to that step's note). At an
 * end of its travel MPC can only send the end again: that is a move further that way. */
static float *enc_pos;   /* the knob being moved (ui_knob sets it; one engine call at a time) */
static int enc_index(float norm, int cur, int n)
{
    float pos;
    int t, steps;
    if (n < 2) return 0;
    pos = *enc_pos >= 0.f ? *enc_pos : cur / (float)(n - 1);
    steps = (int)lroundf((norm - pos) * (n - 1));
    if (!steps) {
        if (norm > pos + 1e-4f || (norm >= 0.9999f && pos >= 0.9999f)) steps = 1;
        else if (norm < pos - 1e-4f || (norm <= 1e-4f && pos <= 1e-4f)) steps = -1;
    }
    t = cur + steps;
    t = t < 0 ? 0 : t > n - 1 ? n - 1 : t;
    *enc_pos = t / (float)(n - 1);   /* the knob stands at the new value */
    return t;
}

void ui_knob(Bl *x, int which, float norm)
{
    TrackPat *tp = bl_edit(x);
    Sound *s = &x->p.snd[x->ui.track];
    char b[16];
    norm = clampf(norm, 0, 1);
    if (x->ui.encMode[which] != x->ui.mode) { x->ui.encPos[which] = -1.f; x->ui.encMode[which] = x->ui.mode; }
    enc_pos = &x->ui.encPos[which];
    /* in a function, BACK/NOTE sets its value; SELECT/GATE stays the step knob (in LFO it picks the wave) */
    switch (which == 1 || x->ui.mode == MD_LFO ? x->ui.mode : MD_NORMAL) {
    case MD_PLAYMODE: tp->playMode = (uint8_t)enc_index(norm, tp->playMode, PM_COUNT); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_TRANSP: tp->transp = (int8_t)(enc_index(norm, tp->transp + 24, 49) - 24); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_SCALE: tp->scale = (uint8_t)enc_index(norm, tp->scale, NUM_SCALES); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_ARP: tp->arp = (uint8_t)enc_index(norm, tp->arp, ARP_COUNT); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_LAST: tp->len = (uint8_t)(1 + enc_index(norm, tp->len - 1, 64)); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_LFO:
        if (which == 0) { s->v[SP_MOD_WAVE] = (float)enc_index(norm, (int)s->v[SP_MOD_WAVE], 5); ui_soundTouched(x, SP_MOD_WAVE); }
        else { s->v[SP_MOD_DEST] = (float)enc_index(norm, (int)s->v[SP_MOD_DEST], 4); ui_soundTouched(x, SP_MOD_DEST); }
        break;
    case MD_BANK: x->ui.bank = (uint8_t)enc_index(norm, x->ui.bank, NUM_BANKS); mode_text(x, b); ui_message(x, b, 0, 1200); break;
    case MD_PATTERN: {
        const int cur = x->nextPat != 0xff && x->nextPat / 16 == x->ui.bank ? x->nextPat % 16 : x->patSlot % 16;
        seq_queuePattern(x, (uint8_t)(x->ui.bank * 16 + enc_index(norm, cur, 16)));
        mode_text(x, b);
        ui_message(x, b, 0, 1200);
        break;
    }
    default:
        if (which == 0) {   /* SELECT/GATE: the step */
            const int len = track_len(x);
            x->ui.sel = (uint8_t)enc_index(norm, x->ui.sel < len ? x->ui.sel : len - 1, len);
            x->ui.bar = (uint8_t)(x->ui.sel / 16);
            snprintf(b, sizeof b, "S%d", x->ui.sel + 1);
            ui_message(x, b, 0, 1200);
        } else {            /* BACK/NOTE: the selected step's note, C0..C6 (the step plays) */
            Step *st = &tp->s[x->ui.sel];
            const int cur = st->note < 24 ? 0 : st->note > 96 ? 72 : st->note - 24;
            st->note = (uint8_t)(24 + enc_index(norm, cur, 73));
            st->f |= ST_GATE;
            note_name(st->note, b);
            ui_message(x, b, 0, 1200);
        }
        break;
    }
    x->displayRev++;
}

float ui_knobGet(Bl *x, int which)
{
    if (x->ui.encMode[which] == x->ui.mode && x->ui.encPos[which] >= 0.f) return x->ui.encPos[which];   /* where it stands */
    const TrackPat *tp = bl_edit(x);
    const Sound *s = &x->p.snd[x->ui.track];
    switch (which == 1 || x->ui.mode == MD_LFO ? x->ui.mode : MD_NORMAL) {
    case MD_PLAYMODE: return tp->playMode / (float)(PM_COUNT - 1);
    case MD_TRANSP: return (tp->transp + 24) / 48.f;
    case MD_SCALE: return tp->scale / (float)(NUM_SCALES - 1);
    case MD_ARP: return tp->arp / (float)(ARP_COUNT - 1);
    case MD_LAST: return (tp->len - 1) / 63.f;
    case MD_LFO: return which == 0 ? s->v[SP_MOD_WAVE] / 4.f : s->v[SP_MOD_DEST] / 3.f;
    case MD_BANK: return x->ui.bank / (float)(NUM_BANKS - 1);
    case MD_PATTERN: return (x->nextPat != 0xff && x->nextPat / 16 == x->ui.bank ? x->nextPat % 16 : x->patSlot % 16) / 15.f;
    default:
        if (which == 0) { const int len = track_len(x); return len > 1 ? x->ui.sel / (float)(len - 1) : 0.f; }
        return clampf((tp->s[x->ui.sel].note - 24) / 72.f, 0, 1);
    }
}

/* ---------------------------------------------------------------- MIDI notes */
void ui_midiNote(Bl *x, uint8_t ch, uint8_t note, uint8_t vel)
{
    int t;
    /* the gate/duck's trigger: any note on its channel, or a note in the lowest octave (C-2..B-2) on any channel */
    if (ch == (uint8_t)x->p.g[G_GD_CH] || note < 12) {
        if (vel) fx_duckTrigger(x);
        return;
    }
    t = ch == 0 ? x->ui.track : ch < NUM_TRACKS ? ch : -1;
    if (t < 0) return;
    if (vel) {
        Voice *v = &x->v[t];
        const int slide = x->ui.held[t] > 0;
        if (x->ui.held[t] < 255) x->ui.held[t]++;
        x->ui.lastNote[t] = note;
        voice_noteOn(v, &x->p.snd[t], (float)note, vel / 127.f, vel > 100, slide, 1, 1);
        if (x->seq.recording && t == x->ui.track) seq_recordNote(x, (uint8_t)t, note, vel);
    } else {
        if (x->ui.held[t]) x->ui.held[t]--;
        if (!x->ui.held[t]) voice_noteOff(&x->v[t]);
    }
}
