/* LXRXXL ZYNTH -- the sequencer (from LXRXXL BAZZLINE): LXRXXL's LXR-02 style step sequencer (64 patterns in 4 banks of 16, each track with
 * its own length up to 64 steps in 4 bars, MPC's grid and transport), with the Bassline (DB-01) step functions: per
 * step gate, note, accent, slide, MOD (the modulator on that step) and PITCH ENV (the pitch envelope on that step), and
 * per track PLAY MODE, TRANSP, SCALE and ARPEGG.
 *
 * Timing: positions are in 16ths. Following MPC (the default) the position is MPC's song position (ppqPos x 4), so the
 * steps sit on MPC's grid (mpc-vst-plugins docs/MIDI_TIMING.md); otherwise the instance's own tempo runs the clock.
 * bl_render() splits its block at every step and gate, so notes start on their exact frame. */
#include <math.h>
#include <string.h>
#include "bl.h"

/* the scales (semitones from C); the first is chromatic: no quantising */
static const uint16_t SCALES[NUM_SCALES] = {
    0xFFF,                                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HMN: harmonic minor */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),   /* PMJ: pentatonic major */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),  /* PMN: pentatonic minor */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),   /* BLU */
};
static const char *const SCALE_NAMES[NUM_SCALES] = {"CHR", "MAJ", "MIN", "DOR", "PHR", "LYD", "MIX", "LOC", "HMN",
                                                    "PMJ", "PMN", "BLU"};

uint16_t seq_scaleMask(uint8_t scale) { return SCALES[scale < NUM_SCALES ? scale : 0]; }

const char *seq_scaleName(uint8_t s) { return SCALE_NAMES[s < NUM_SCALES ? s : 0]; }

/* the nearest scale note at or below (root C) */
uint8_t seq_quantise(uint8_t note, uint8_t scale, uint8_t root)
{
    int n = note, k;
    const uint16_t m = SCALES[scale < NUM_SCALES ? scale : 0];
    for (k = 0; k < 12; k++, n--) {
        const int pc = ((n - root) % 12 + 12) % 12;
        if (m & (1u << pc)) return (uint8_t)(n < 0 ? 0 : n);
    }
    return note;
}

/* the note d scale degrees above note (the arpeggio) */
static int scale_up(int note, int degrees, uint8_t scale)
{
    const uint16_t m = SCALES[scale < NUM_SCALES ? scale : 0];
    int n = note;
    if (scale == 0) {   /* chromatic: a major chord (degrees 0, 2, 4, 12) */
        return note + (degrees == 2 ? 4 : degrees == 4 ? 7 : degrees >= 12 ? 12 : 0);
    }
    while (degrees > 0) {
        n++;
        if (m & (1u << (n % 12))) degrees--;
    }
    return n;
}

int seq_hasFlag(const TrackPat *tp, uint8_t flag)
{
    int i;
    for (i = 0; i < tp->len && i < NUM_STEPS; i++)
        if ((tp->s[i].f & ST_GATE) && (tp->s[i].f & flag)) return 1;
    return 0;
}

static uint8_t pattern_length(Bl *x)
{
    uint8_t t, m = 1;
    const Pattern *p = bl_pat(x);
    for (t = 0; t < NUM_TRACKS; t++)
        if (p->t[t].len > m) m = p->t[t].len;
    return m;
}

void seq_loadPattern(Bl *x, uint8_t pat)
{
    if (pat >= NUM_PATTERNS) return;
    x->patSlot = pat;
    x->displayRev++;
}

void seq_queuePattern(Bl *x, uint8_t pat)
{
    if (pat >= NUM_PATTERNS) return;
    if (!x->seq.running) { seq_loadPattern(x, pat); x->nextPat = 0xff; return; }
    x->nextPat = pat;
}

static double per_frame(Bl *x) { return bl_tempo(x) / 60.0 / SR * 4.0; }   /* 16ths per frame */

/* the step a track plays at pattern step k */
static uint8_t step_of(Bl *x, int t, const TrackPat *tp, uint32_t k)
{
    const uint32_t L = tp->len ? tp->len : 16;
    switch (tp->playMode) {
    case PM_REV: return (uint8_t)(L - 1 - k % L);
    case PM_PONG: {
        uint32_t q;
        if (L < 2) return 0;
        q = k % (2 * L - 2);
        return (uint8_t)(q < L ? q : 2 * L - 2 - q);
    }
    case PM_RND: return (uint8_t)(bl_rand(&x->seq.rng) % L);
    default: return (uint8_t)(k % L);
    }
    (void)t;
}

static void fire_step(Bl *x, uint32_t n)
{
    int t;
    uint32_t rel;
    const uint8_t patLen = pattern_length(x);
    if (n > x->seq.patStart && (n - x->seq.patStart) % patLen == 0) {
        if (x->nextPat != 0xff) {
            seq_loadPattern(x, x->nextPat);
            x->nextPat = 0xff;
        }
        x->seq.patStart = n;
    }
    rel = n - x->seq.patStart;
    for (t = 0; t < NUM_TRACKS; t++) {
        const TrackPat *tp = bl_trackPat(x, t);
        const uint8_t s = step_of(x, t, tp, rel);
        const Step *st = &tp->s[s];
        const uint8_t wasSliding = x->seq.slideHeld[t];
        x->seq.pos16[t] = s;
        x->seq.slideHeld[t] = 0;
        if (!(st->f & ST_GATE) || (x->p.mutes & (1u << t))) {
            /* a rest: a slide into it holds the note half a step longer, as on the 303 */
            if (wasSliding) x->seq.gateOff[t] = n + 0.5;
            else if (x->seq.gateOff[t] < 0) voice_noteOff(&x->v[t]);
            continue;
        }
        {
            int note = st->note + tp->transp;
            const int acc = (st->f & ST_ACC) != 0;
            const int penv = seq_hasFlag(tp, ST_PENV) ? (st->f & ST_PENV) != 0 : 1;
            const int mod = seq_hasFlag(tp, ST_MOD) ? (st->f & ST_MOD) != 0 : 1;
            if (tp->scale) note = seq_quantise((uint8_t)(note < 0 ? 0 : note > 127 ? 127 : note), tp->scale, 0);
            if (tp->arp != ARP_OFF) {
                static const int updown[6] = {0, 1, 2, 3, 2, 1};
                const uint32_t c = x->seq.cnt[t]++;
                int d = 0;
                switch (tp->arp) {
                case ARP_UP: d = (int)(c % 4); break;
                case ARP_DOWN: d = 3 - (int)(c % 4); break;
                case ARP_UPDOWN: d = updown[c % 6]; break;
                default: d = (int)(bl_rand(&x->seq.rng) % 4); break;
                }
                /* root, 3rd, 5th, octave of the track's scale (chromatic: a major chord) */
                note = scale_up(note, d == 3 ? __builtin_popcount(SCALES[tp->scale < NUM_SCALES ? tp->scale : 0]) : d * 2, tp->scale);
            }
            if (note < 0) note = 0;
            if (note > 127) note = 127;
            x->ui.lastNote[t] = (uint8_t)note;
            {
                const uint8_t vb = bl_vel(x, bl_trackSrc(x, t))[s];   /* 0: as 0.1.0: 100, accented 127 */
                const float vel = vb ? vb / 127.f : acc ? 1.f : 0.8f;
                voice_noteOn(&x->v[t], &x->p.snd[t], (float)note, vel, acc, wasSliding, penv, mod);
            }
            if (st->f & ST_SLIDE) { x->seq.slideHeld[t] = 1; x->seq.gateOff[t] = -1; }
            else x->seq.gateOff[t] = n + 0.5;
        }
    }
    x->displayRev++;
}

static void begin_at(Bl *x, double pos)
{
    int t;
    const uint8_t patLen = pattern_length(x);
    x->seq.pos = pos;
    x->seq.n = (uint32_t)ceil(pos - 1e-9);
    x->seq.patStart = x->seq.n - (x->seq.n % patLen);   /* MPC's bar 1 is step 1 */
    for (t = 0; t < NUM_TRACKS; t++) {
        x->seq.gateOff[t] = -1;
        x->seq.slideHeld[t] = 0;
        x->seq.cnt[t] = 0;
    }
}

void seq_start(Bl *x)
{
    x->seq.running = 1;
    x->seq.freeRun = (uint8_t)!(x->p.bpm == 0 && x->seq.hostValid && x->seq.hostPlaying);
    begin_at(x, x->seq.freeRun ? 0.0 : x->seq.hostPpq * 4.0 + x->seq.frames * per_frame(x));
    x->displayRev++;
}

void seq_stop(Bl *x)
{
    int t;
    x->seq.running = 0;
    x->seq.freeRun = 0;
    for (t = 0; t < NUM_TRACKS; t++) {
        voice_noteOff(&x->v[t]);
        x->seq.gateOff[t] = -1;
        x->seq.slideHeld[t] = 0;
    }
    if (x->nextPat != 0xff) { seq_loadPattern(x, x->nextPat); x->nextPat = 0xff; }
    x->displayRev++;
}

/* MPC's transport and tempo, once per host block (engine.c) */
void seq_hostTime(Bl *x, int playing, double ppq, double tempo)
{
    const int was = x->seq.hostPlaying;
    x->seq.hostValid = 1;
    if (tempo > 0) x->seq.hostTempo = tempo;
    x->seq.hostPpq = ppq;
    x->seq.hostPlaying = (uint8_t)(playing != 0);
    x->seq.frames = 0;
    if (x->p.bpm != 0) return;   /* its own tempo: MPC's transport doesn't drive it */
    if (playing && !was) {
        if (x->seq.running) seq_stop(x);
        seq_start(x);
    } else if (!playing && was) {
        if (!x->seq.freeRun && x->seq.running) seq_stop(x);
    } else if (playing && x->seq.running && !x->seq.freeRun) {
        /* a loop or a locate: back to MPC's position */
        const double p = ppq * 4.0;
        if (p < x->seq.pos - 0.05 || p > x->seq.pos + 4.0) begin_at(x, p);
    }
}

static int following(Bl *x) { return x->p.bpm == 0 && x->seq.hostValid && !x->seq.freeRun && x->seq.hostPlaying; }

/* frames until the next step or gate, from the current position */
double seq_nextEventIn(Bl *x)
{
    int t;
    double next;
    const double pf = per_frame(x);
    if (!x->seq.running || pf <= 0) return 1e9;
    next = (double)x->seq.n;
    for (t = 0; t < NUM_TRACKS; t++)
        if (x->seq.gateOff[t] >= 0 && x->seq.gateOff[t] < next) next = x->seq.gateOff[t];
    next = (next - x->seq.pos) / pf;
    return next < 0 ? 0 : next;
}

/* the clock over frames; everything due at the new position fires (bl_render calls it at each event's frame) */
void seq_advance(Bl *x, int frames, int at)
{
    int t;
    double p1;
    (void)at;
    if (!x->seq.running) { x->seq.frames += (uint32_t)frames; return; }
    x->seq.frames += (uint32_t)frames;
    if (following(x)) p1 = x->seq.hostPpq * 4.0 + x->seq.frames * per_frame(x);
    else p1 = x->seq.pos + frames * per_frame(x);
    if (p1 < x->seq.pos) p1 = x->seq.pos;
    x->seq.pos = p1;
    for (t = 0; t < NUM_TRACKS; t++)
        if (x->seq.gateOff[t] >= 0 && x->seq.gateOff[t] <= p1 + 1e-6) {
            x->seq.gateOff[t] = -1;
            voice_noteOff(&x->v[t]);
        }
    while ((double)x->seq.n <= p1 + 1e-6 && x->seq.running) {
        fire_step(x, x->seq.n);
        x->seq.n++;
    }
}

/* ---------------------------------------------------------------- recording */
void seq_recordNote(Bl *x, uint8_t track, uint8_t note, uint8_t vel)
{
    TrackPat *tp = &bl_pat(x)->t[track];
    uint8_t s;
    if (x->seq.running) {
        /* the nearest 16th */
        const uint32_t nq = (uint32_t)floor(x->seq.pos + 0.5);
        const uint32_t L = tp->len ? tp->len : 16;
        s = (uint8_t)((nq >= x->seq.patStart ? nq - x->seq.patStart : 0) % L);
    } else {
        /* step entry: at the selected step, then the next */
        s = x->ui.sel;
        x->ui.sel = (uint8_t)((x->ui.sel + 1) % (tp->len ? tp->len : 16));
        x->ui.bar = (uint8_t)(x->ui.sel / 16);
    }
    tp->s[s].f = (uint8_t)((tp->s[s].f & ~ST_ACC) | ST_GATE | (vel >= 112 ? ST_ACC : 0));
    tp->s[s].note = note;
    bl_vel(x, track)[s] = vel > 127 ? 127 : vel;   /* the key's velocity, as played */
    x->displayRev++;
}

/* RND PATT (SHIFT + step 11): STEP RANDOM on the track, at the STEP RANDOM amount */
void seq_randomTrack(Bl *x, uint8_t track)
{
    rnd_steps(x, x->rnd2.seqAmt ? x->rnd2.seqAmt * 10 : 50, track);
}
