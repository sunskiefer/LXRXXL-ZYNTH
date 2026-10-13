/* LXRXXL ZYNTH -- STEP RANDOM: a melodic step generator.
 *
 * Each pattern has a HARMONY: a key (root + mode), a chord progression (one of 24, four chords over the pattern) and a
 * chord colour (triads, sevenths, sus 2, sus 4, add 9). Every track generated in the pattern plays over it, so tracks
 * generated one at a time still fit together. 12 roots x 7 modes x 24 progressions x 5 colours: 10 080 harmonies.
 *
 * Each track plays a STYLE, by default from its voice (AUTO):
 *   BASS  (BAZZ)                              chord roots on an euclidean rhythm, fifths, octaves and approach notes,
 *                                             slides and accents: an acid or a synth bass line
 *   CHORD (WAVE, WHEEL, GRAIN, TRIO, VOICE,   one chord tone per track (track 1 the root, 2 the third, 3 the fifth...,
 *          DRONE, TEXTURE)                    so chord tracks build chords together), voice-led to the nearest octave:
 *                                             held through each chord at a low amount, stabs higher, pulses at the top
 *   ARP   (CHIP, FM4, MACRO, PHASE)           the chord's tones over one to three octaves in the order
 *                                             idx(i) = (a * i + b) mod m, a coprime with m: up, down and every
 *                                             leap-frog order in between
 *   LEAD  (ANALOG, FM6, PHYS, LXR TONE)       a contour of two sines, y(i) = A1 sin(2 pi i / P1 + p1) + A2 sin(2 pi i / P2
 *                                             + p2), read as scale degrees, landing on chord tones on the beats, with
 *                                             rests between phrases and slides between near notes
 *   HITS  (LXR METAL, NOISE, FX)              sparse euclidean hits on the high chord tones
 * The amount knob is the density. A track whose SCALE is set keeps its scale (from C) instead of the pattern's key.
 *
 * The velocities follow the track's velocity mode: FLAT (all 100), GROOVE (the beats strong, the off-beats soft),
 * HUMAN (around 100, loose) or SWELL (rising through each chord); a velocity of 112 or more is an accent. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bl.h"

const uint8_t RND_LENGTHS[8] = {4, 8, 12, 16, 24, 32, 48, 64};

static const uint8_t MODE_SCALE[NUM_HARM_MODES] = {1, 2, 3, 4, 5, 6, 8};   /* seq.c's scales */
static const char *const MODE_SHORT[NUM_HARM_MODES] = {"MAJ", "MIN", "DOR", "PHR", "LYD", "MIX", "HMN"};
static const char *const MODE_FULL[NUM_HARM_MODES] = {"MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN",
                                                      "HARMONIC MINOR"};
static const char *const STYLE_NAMES[RS_COUNT] = {"AUTO", "CHORD", "ARP", "LEAD", "BASS", "HITS"};
static const char *const COLOR_NAMES[5] = {"TRIADS", "SEVENTHS", "SUS 2", "SUS 4", "ADD 9"};
static const char *const ROOTS[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* the progressions: scale degrees (0 = I), four chords */
#define NUM_PROGS 24
static const int8_t PROG[NUM_PROGS][4] = {
    {0, 4, 5, 3}, {5, 3, 0, 4}, {0, 5, 3, 4}, {1, 4, 0, 0}, {0, 3, 4, 3}, {0, 5, 2, 6},
    {0, 3, 4, 0}, {0, 6, 5, 6}, {0, 2, 3, 4}, {3, 4, 2, 5}, {0, 4, 3, 0}, {5, 4, 3, 4},
    {0, 0, 3, 3}, {0, 0, 3, 4}, {0, 6, 3, 0}, {1, 2, 3, 4}, {0, 5, 1, 4}, {0, 2, 6, 5},
    {3, 0, 4, 0}, {0, 3, 0, 4}, {5, 1, 4, 0}, {0, 4, 5, 2}, {3, 0, 4, 5}, {0, 1, 3, 4},
};
static const char *const NUMERALS[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};

const char *rnd_modeName(int mode, int full)
{
    if (mode < 0 || mode >= NUM_HARM_MODES) mode = 0;
    return full ? MODE_FULL[mode] : MODE_SHORT[mode];
}
const char *rnd_styleName(int style) { return STYLE_NAMES[style >= 0 && style < RS_COUNT ? style : 0]; }

/* ---------------------------------------------------------------- the random numbers */
static uint64_t next(Bl *x)
{
    uint64_t s = x->rnd2.state;
    if (!s) s = ((uint64_t)time(NULL) << 32) ^ (uint64_t)(uintptr_t)x ^ 0x9E3779B97F4A7C15ull ^ x->clock;
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    x->rnd2.state = s;
    return s * 0x2545F4914F6CDD1Dull;
}
static int below(Bl *x, int n) { return n > 0 ? (int)((next(x) >> 33) % (uint64_t)n) : 0; }
static float unit(Bl *x) { return (float)(next(x) >> 40) / (float)(1u << 24); }
static int chance(Bl *x, float p) { return unit(x) < p; }

/* ---------------------------------------------------------------- the harmony */
static void new_progression(Bl *x, Harmony *h)
{
    const int was = h->prog;
    int c = below(x, 100);
    do h->prog = (uint8_t)below(x, NUM_PROGS); while (h->valid && h->prog == was);
    h->color = (uint8_t)(c < 35 ? 0 : c < 65 ? 1 : c < 77 ? 2 : c < 89 ? 3 : 4);
    h->seed = (uint32_t)next(x);
    h->valid = 1;
}

void rnd_newHarmony(Bl *x)
{
    Harmony *h = &x->p.harm[x->patSlot];
    h->root = (uint8_t)below(x, 12);
    h->mode = (uint8_t)(below(x, 3) ? below(x, 2) : below(x, NUM_HARM_MODES));   /* major and minor the most */
    new_progression(x, h);
}

Harmony *rnd_harmony(Bl *x)
{
    Harmony *h = &x->p.harm[x->patSlot];
    if (!h->valid) rnd_newHarmony(x);
    return h;
}

void rnd_harmonyText(Bl *x, char *buf, int n)
{
    const Harmony *h = rnd_harmony(x);
    char p[32] = "";
    int i;
    for (i = 0; i < 4; i++) {
        const int d = PROG[h->prog][i] % 7;
        strcat(p, NUMERALS[d]);
        if (i < 3) strcat(p, "-");
    }
    snprintf(buf, (size_t)n, "%s %s   %s   %s", ROOTS[h->root % 12], MODE_FULL[h->mode % NUM_HARM_MODES], p,
             COLOR_NAMES[h->color % 5]);
}

void rnd_harmonyNudge(Bl *x, int what, int to)
{
    Harmony *h = rnd_harmony(x);
    if (what == 0) h->root = (uint8_t)(((to % 12) + 12) % 12);
    else if (what == 1) h->mode = (uint8_t)(((to % NUM_HARM_MODES) + NUM_HARM_MODES) % NUM_HARM_MODES);
    else new_progression(x, h);
}

/* ---------------------------------------------------------------- scales, degrees, chords */
typedef struct { int n, root, pc[12]; } Scale;

static void scale_of(Scale *s, int root, uint16_t mask)
{
    int k;
    s->n = 0;
    s->root = root;
    for (k = 0; k < 12; k++) if (mask & (1u << k)) s->pc[s->n++] = k;
    if (!s->n) { s->pc[0] = 0; s->n = 1; }
}

/* the pitch of scale degree deg (any integer) above base */
static int pitch(const Scale *s, int deg, int base)
{
    const int o = deg >= 0 ? deg / s->n : -((-deg + s->n - 1) / s->n);
    return base + s->root + s->pc[deg - o * s->n] + 12 * o;
}

/* the chord on degree d as degrees */
static int chord(int d, int color, int *out)
{
    switch (color) {
    case 1: out[0] = d; out[1] = d + 2; out[2] = d + 4; out[3] = d + 6; return 4;
    case 2: out[0] = d; out[1] = d + 1; out[2] = d + 4; return 3;
    case 3: out[0] = d; out[1] = d + 3; out[2] = d + 4; return 3;
    case 4: out[0] = d; out[1] = d + 2; out[2] = d + 4; out[3] = d + 8; return 4;
    default: out[0] = d; out[1] = d + 2; out[2] = d + 4; return 3;
    }
}

int rnd_styleOf(Bl *x, int t)
{
    const int st = x->p.rset[t].style;
    if (st != RS_AUTO && st < RS_COUNT) return st;
    switch ((int)(x->p.snd[t].v[SP_VOICE] + 0.5f)) {
    case ENG_BAZZ: return RS_BASS;
    case ENG_WAVE: case ENG_WHEEL: case ENG_GRAIN: case ENG_TRIO: case ENG_VOICE: case ENG_DRONE: case ENG_TEXTURE:
        return RS_CHORD;
    case ENG_CHIP: case ENG_FM4: case ENG_MACRO: case ENG_PHASE: return RS_ARP;
    case ENG_LXR_METAL: case ENG_NOISE: case ENG_FX: return RS_HITS;
    default: return RS_LEAD;
    }
}

/* the euclidean rhythm: k of L steps, turned by rot */
static int euclid(int i, int k, int L, int rot) { return k > 0 && (((i + rot) % L) * k) % L < k; }

static int gcd(int a, int b) { while (b) { const int t = a % b; a = b; b = t; } return a; }

/* ---------------------------------------------------------------- one track */
typedef struct { int L, nCh, span; } Shape;

static int chord_at(const Shape *sh, int i) { return (i / sh->span) % sh->nCh; }

static void gen_track(Bl *x, int t, float d)
{
    TrackPat *tp = &bl_pat(x)->t[t];
    uint8_t *vel = x->p.vel[x->patSlot][t];
    const RndSet *rs = &x->p.rset[t];
    const Harmony *h = rnd_harmony(x);
    const int style = rnd_styleOf(x, t);
    const int L = RND_LENGTHS[rs->lenOpt & 7];
    Shape sh;
    Scale sc;
    int deg[NUM_STEPS], on[NUM_STEPS], slide[NUM_STEPS];
    int chords[4][4], nt[4];
    int i, c, base;
    memset(deg, 0, sizeof deg);
    memset(on, 0, sizeof on);
    memset(slide, 0, sizeof slide);
    if (tp->scale) scale_of(&sc, 0, seq_scaleMask(tp->scale));   /* the track's own scale (from C) */
    else scale_of(&sc, h->root, seq_scaleMask(MODE_SCALE[h->mode % NUM_HARM_MODES]));
    sh.L = L;
    sh.nCh = L >= 8 ? 4 : 2;
    sh.span = L / sh.nCh;
    if (sh.span < 1) sh.span = 1;
    for (c = 0; c < 4; c++) {
        const int pd = PROG[h->prog % NUM_PROGS][sh.nCh == 4 ? c : c * 2] % 7;
        nt[c] = chord(pd, h->color % 5, chords[c]);
    }
    switch (style) {
    case RS_BASS: {
        const int k = (int)(L * (0.25f + 0.55f * d) + 0.5f);
        const int rot = below(x, L);
        base = 36;
        for (i = 0; i < L; i++) {
            const int ci = chord_at(&sh, i), r = chords[ci][0];
            on[i] = euclid(i, k, L, rot) || i % sh.span == 0;
            if (!on[i]) continue;
            if (i % sh.span == 0) deg[i] = r;
            else {
                const int u = below(x, 100);
                deg[i] = u < 50 ? r : u < 68 ? r + sc.n : u < 86 ? r + 4 : chords[ci][1];
            }
            if ((i + 1) % sh.span == 0 && i + 1 < L && chance(x, 0.5f)) {   /* an approach to the next chord */
                const int nr = chords[chord_at(&sh, i + 1)][0];
                deg[i] = nr + (chance(x, 0.5f) ? 1 : -1);
            }
        }
        for (i = 0; i + 1 < L; i++) if (on[i] && on[i + 1] && deg[i] != deg[i + 1] && chance(x, 0.12f + 0.25f * d)) slide[i] = 1;
        break;
    }
    case RS_CHORD: {
        /* the track's chord tone: tracks on CHORD share the chord out between them */
        int nChord = 0, mine = 0, prev = -1000, k;
        for (k = 0; k < NUM_TRACKS; k++) if (rnd_styleOf(x, k) == RS_CHORD) { if (k == t) mine = nChord; nChord++; }
        base = 60;
        {
            const int mode = d < 0.4f ? 0 : d < 0.75f ? 1 : 2;
            const int kk = (int)(sh.span * (0.3f + 0.4f * d) + 0.5f);
            const int rot = below(x, sh.span);
            for (i = 0; i < L; i++) {
                const int ci = chord_at(&sh, i), pos = i % sh.span;
                const int ti = mine % nt[ci];
                int dg = chords[ci][ti], best = dg, bd = 1000, o;
                for (o = -2; o <= 2; o++) {   /* the octave nearest the last note: voice leading */
                    const int cand = dg + o * sc.n;
                    const int dist = prev < -100 ? abs(cand) : abs(pitch(&sc, cand, 0) - pitch(&sc, prev, 0));
                    if (dist < bd) { bd = dist; best = cand; }
                }
                deg[i] = best;
                if (pos == sh.span - 1) prev = best;
                if (mode == 0) { on[i] = 1; slide[i] = pos < sh.span - 1; }
                else if (mode == 1) on[i] = pos == 0 || euclid(pos, kk, sh.span, rot);
                else { on[i] = pos % 2 == 0 || chance(x, d - 0.5f); slide[i] = chance(x, 0.15f); }
            }
            /* the last step of every chord lets go */
            for (i = 0; i < L; i++) if (i % sh.span == sh.span - 1) slide[i] = 0;
        }
        break;
    }
    case RS_ARP: {
        const int octs = 1 + (d > 0.45f) + (d > 0.8f && chance(x, 0.5f));
        const int k = (int)(L * (0.5f + 0.5f * d) + 0.5f);
        const int rot = below(x, L);
        int a[4], b[4];
        base = (int)(x->p.snd[t].v[SP_VOICE] + 0.5f) == ENG_CHIP ? 60 : 48;
        for (c = 0; c < 4; c++) {   /* each chord its own order */
            const int m = nt[c] * octs;
            int tries = 0;
            do a[c] = 1 + below(x, m - 1 > 0 ? m - 1 : 1); while (gcd(a[c], m) != 1 && ++tries < 16);
            if (gcd(a[c], m) != 1) a[c] = 1;
            if (c > 0 && chance(x, 0.5f)) a[c] = a[0] % m ? a[0] % m : 1;   /* often the same shape */
            b[c] = below(x, m);
        }
        for (i = 0; i < L; i++) {
            const int ci = chord_at(&sh, i), m = nt[ci] * octs;
            const int idx = ((a[ci] * (i % sh.span) + b[ci]) % m + m) % m;
            on[i] = d > 0.9f || euclid(i, k, L, rot) || i % sh.span == 0;
            deg[i] = chords[ci][idx % nt[ci]] + sc.n * (idx / nt[ci]);
        }
        break;
    }
    case RS_HITS: {
        const int k = (int)(L * (0.12f + 0.3f * d) + 0.5f);
        const int rot = below(x, L);
        base = 72;
        for (i = 0; i < L; i++) {
            const int ci = chord_at(&sh, i);
            on[i] = euclid(i, k > 0 ? k : 1, L, rot);
            deg[i] = chords[ci][below(x, nt[ci])] - (chance(x, 0.3f) ? sc.n : 0);
        }
        break;
    }
    default: {   /* LEAD */
        static const int P2S[4] = {3, 5, 6, 7};
        const int P1s[4] = {L, L > 8 ? L / 2 : L, 16, 12};
        const float A1 = 2.f + 3.f * unit(x), A2 = 0.5f + 2.f * unit(x);
        const float P1 = (float)P1s[below(x, 4)], P2 = (float)P2S[below(x, 4)];
        const float p1 = 6.2831853f * unit(x), p2 = 6.2831853f * unit(x);
        const int k = (int)(L * (0.3f + 0.45f * d) + 0.5f);
        const int rot = below(x, L);
        const int centre = sc.n + below(x, 3) - 1;   /* around an octave above base */
        base = 48;
        for (i = 0; i < L; i++) {
            const float y = centre + A1 * sinf(6.2831853f * i / P1 + p1) + A2 * sinf(6.2831853f * i / P2 + p2);
            int dg = (int)lroundf(y);
            const int ci = chord_at(&sh, i);
            if (i % 4 == 0) {   /* on the beat: the nearest chord tone */
                int best = dg, bd = 1000, j, o;
                for (j = 0; j < nt[ci]; j++)
                    for (o = -2; o <= 3; o++) {
                        const int cand = chords[ci][j] + o * sc.n, dist = abs(cand - dg);
                        if (dist < bd) { bd = dist; best = cand; }
                    }
                dg = best;
            }
            deg[i] = dg;
            on[i] = euclid(i, k, L, rot) || i == 0;
            if (L >= 16 && i % 16 >= 14 && chance(x, 0.6f)) on[i] = 0;   /* a breath between phrases */
        }
        for (i = 0; i + 1 < L; i++) if (on[i] && on[i + 1] && abs(deg[i] - deg[i + 1]) <= 2 && chance(x, 0.3f)) slide[i] = 1;
        break;
    }
    }
    /* write the steps and their velocities */
    tp->len = (uint8_t)L;
    for (i = 0; i < NUM_STEPS; i++) {
        Step *st = &tp->s[i];
        if (i < L && on[i]) {
            int n = pitch(&sc, deg[i], base), v;
            while (n < 12) n += 12;
            while (n > 108) n -= 12;
            st->note = (uint8_t)n;
            st->f = ST_GATE | (slide[i] && i + 1 < L && on[i + 1] ? ST_SLIDE : 0);
            switch (rs->vel) {
            case RV_FLAT: v = 100; break;
            case RV_HUMAN: v = 96 + below(x, 41) - 20; if (v >= 112 && chance(x, 0.5f)) v = 108; break;
            case RV_SWELL: v = 60 + (62 * (i % sh.span)) / (sh.span > 1 ? sh.span - 1 : 1); break;
            default:   /* GROOVE */
                v = i % 4 == 0 ? (i % sh.span == 0 || chance(x, 0.4f) ? 118 : 104) : i % 2 == 0 ? 96 : 72 + below(x, 14);
                if (style == RS_BASS && i % 4 != 0 && chance(x, 0.15f * d)) v = 116;   /* acid accents off the beat */
                break;
            }
            v = v < 1 ? 1 : v > 127 ? 127 : v;
            vel[i] = (uint8_t)v;
            if (v >= 112) st->f |= ST_ACC;
        } else {
            st->f = 0;
            vel[i] = 0;
        }
    }
}

void rnd_steps(Bl *x, int pct, int track)
{
    int t;
    float d;
    if (pct < 10) pct = 10;   /* the amount is the density: never empty */
    if (pct > 100) pct = 100;
    d = pct / 100.f;
    rnd_harmony(x);
    for (t = 0; t < NUM_TRACKS; t++) {
        if (track >= 0 && t != track) continue;
        gen_track(x, t, d);
    }
    x->displayRev++;
}
