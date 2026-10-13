/* LXRXXL ZYNTH -- the two randomizers, from LXRXXL's modded panel (the same keys, knobs and colours).
 *
 * SOUND RANDOM (the green key, its knob 0-100 % in tens): every sound control of every track (with SHIFT: the selected
 * track only) moves by a random amount of up to that share of its range, up or down, each on its own draw; a
 * selector changes to another option with that chance. A value never lands where it is, nor back where it was before
 * the last press. What would silence a track keeps where it can sound: the cutoff keeps at least 30 %, the attack at
 * most 60 %, the release at most 70 %; the voice, the level, the pan, the modulator's sync and the swarm's mode are
 * left alone (the voice stays the one you picked).
 *
 * STEP RANDOM (the yellow key, its knob in tens): a new line on every track of the playing pattern (with SHIFT: the
 * selected track), an euclidean rhythm per track: about n % of the track's steps are notes (half to all of it, drawn
 * per track), spread evenly, turned by a random rotation, with accents and slides here and there; the notes stay in
 * the track's scale (a chromatic track gets a minor pentatonic) around where its notes already were. */
#include <math.h>
#include <string.h>
#include <time.h>
#include "bl.h"
#include "param_map.h"

static uint64_t next(Bl *x)
{
    uint64_t s = x->rnd2.state;
    if (!s) s = ((uint64_t)time(NULL) << 32) ^ (uint64_t)(uintptr_t)x ^ 0x9E3779B97F4A7C15ull ^ x->clock;
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    x->rnd2.state = s;
    return s * 0x2545F4914F6CDD1Dull;
}
static uint32_t below(Bl *x, uint32_t n) { return n ? (uint32_t)((next(x) >> 33) % n) : 0; }
static float unit(Bl *x) { return (float)(next(x) >> 40) / (float)(1u << 24); }   /* 0..1 */

static int skip(int sp)
{
    return sp == SP_VOICE || sp == SP_LEVEL || sp == SP_PAN || sp == SP_MOD_SYNC || sp == SP_SW_MODE;
}

static void limits(int sp, float *lo, float *hi)
{
    *lo = 0.f;
    *hi = 1.f;
    switch (sp) {
    case SP_CUTOFF: *lo = 0.3f; break;
    case SP_VCA_ATK: *hi = 0.6f; break;
    case SP_VCA_REL: *hi = 0.7f; break;
    case SP_VCF_ATK: *hi = 0.7f; break;
    case SP_RESO: *hi = 0.85f; break;
    case SP_DRIVE: *hi = 0.7f; break;
    case SP_NOISE: *hi = 0.5f; break;
    case SP_PITCH_AMT: *hi = 0.45f; break;
    default: break;
    }
}

/* one landing: up or down by up to pct % of the range, reflected at the ends, never where it is or just was */
static float land(Bl *x, float cur, float prev, float lo, float hi, int pct)
{
    const float span = pct / 100.f;
    int tries;
    float v = cur;
    if (hi <= lo) return lo;
    for (tries = 0; tries < 12; tries++) {
        float d = (0.02f + unit(x) * (span - 0.02f > 0 ? span - 0.02f : 0.f));
        if (below(x, 2)) d = -d;
        v = cur + d;
        while (v < lo || v > hi) v = v < lo ? 2.f * lo - v : 2.f * hi - v;
        if (fabsf(v - cur) > 0.005f && fabsf(v - prev) > 0.005f) return v;
    }
    return v;
}

void rnd_sounds(Bl *x, int pct, int track)
{
    int t, sp;
    if (pct <= 0) return;
    if (pct > 100) pct = 100;
    for (t = 0; t < NUM_TRACKS; t++) {
        Sound *s = &x->p.snd[t];
        if (track >= 0 && t != track) continue;
        for (sp = 0; sp < SP_COUNT; sp++) {
            const int n = SOUND_DEFS[sp].nopts;
            const float was = s->v[sp];
            if (skip(sp)) continue;
            if (n) {   /* a selector: another option, with the knob's chance */
                if (unit(x) * 100.f < (float)pct) {
                    int o = (int)below(x, (uint32_t)(n - 1));
                    if (o >= (int)was) o++;
                    s->v[sp] = (float)o;
                }
            } else {
                float lo, hi;
                limits(sp, &lo, &hi);
                s->v[sp] = land(x, was, x->rnd2.prev[t][sp], lo, hi, pct);
            }
            x->rnd2.prev[t][sp] = was;
        }
        sound_sync(s);
    }
    x->displayRev++;
}

void rnd_steps(Bl *x, int pct, int track)
{
    int t;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    for (t = 0; t < NUM_TRACKS; t++) {
        TrackPat *tp = &bl_pat(x)->t[t];
        const int len = tp->len ? tp->len : 16;
        const uint8_t scale = tp->scale ? tp->scale : 10;   /* chromatic: minor pentatonic */
        uint64_t old = 0, now = 0;
        int tries, i, base = 0, cnt = 0;
        if (track >= 0 && t != track) continue;
        /* where this track's notes are: their average (48 when it has none) */
        for (i = 0; i < len; i++) if (tp->s[i].f & ST_GATE) { base += tp->s[i].note; cnt++; if (i < 64) old |= (uint64_t)1 << i; }
        base = cnt ? base / cnt : 48;
        for (tries = 0; tries < 8; tries++) {
            const float share = pct / 100.f * (0.5f + 0.5f * unit(x));
            int k = (int)(share * (float)len + 0.5f);
            const int rot = (int)below(x, (uint32_t)len);
            if (pct > 0 && k == 0) k = 1;
            if (k > len) k = len;
            now = 0;
            for (i = 0; i < len; i++) {
                const int src = (i + len - rot) % len;
                if (k && (src * k) % len < k) now |= (uint64_t)1 << i;
            }
            if (now != old || k == 0 || k == len) break;   /* a new line every press */
        }
        for (i = 0; i < NUM_STEPS; i++) {
            Step *st = &tp->s[i];
            if (i < len && (now >> i & 1)) {
                int note = base + (int)below(x, 15) - 7;
                if (below(x, 6) == 0) note += below(x, 2) ? 12 : -12;
                note = note < 12 ? 12 : note > 108 ? 108 : note;
                st->note = seq_quantise((uint8_t)note, scale, 0);
                st->f = ST_GATE;
                if (below(x, 5) == 0) st->f |= ST_ACC;
                if (below(x, 8) == 0) st->f |= ST_SLIDE;
            } else st->f = 0;
        }
    }
    x->displayRev++;
}
