/* LXRXXL ZYNTH -- SOUND RANDOM (the green key, its knob 0-100 % in tens): a new sound for the selected track (with
 * SHIFT: every track), each track on its own draw, true to the voice it plays.
 *
 *   A  voice-aware: the voice's six sliders move by up to the amount; a slider that picks options (an algorithm, a
 *      wave, a model) changes option with a chance of 0.7 x the amount;
 *   B  characters: each press goes for a character of the voice's family and moves the chain towards it by the amount
 *      (sustained voices: PAD, LEAD, PLUCK, KEYS; struck voices (PHYS, LXR TONE, LXR METAL): RING, BLIP, BELL; BAZZ:
 *      ACID, SUB, PUNCH; any voice: HOME, its home sound), with a little random on top;
 *   C  what can get harsh (DRIVE, RESONANCE, the pitch envelope, FM, NOISE, SUB) is held low and only opens up as the
 *      amount goes up (its ceiling grows with the amount squared); the cutoff keeps the sound open.
 * The voice, the level, the pan, the modulator's sync and the swarm's mode are left alone. A value never lands where it
 * is, nor back where it was before the last press. */
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
    return sp == SP_VOICE || sp == SP_LEVEL || sp == SP_PAN || sp == SP_MOD_SYNC || sp == SP_SW_MODE ||
           (sp >= SP_V1 && sp <= SP_V20);
}

/* C: the range a value may land in at amount a (0..1) */
static void limits(int sp, float a, float *lo, float *hi)
{
    const float a2 = a * a;
    *lo = 0.f;
    *hi = 1.f;
    switch (sp) {
    case SP_DRIVE: *hi = .25f + .55f * a2; break;
    case SP_RESO: *hi = .45f + .45f * a2; break;
    case SP_PITCH_AMT: *hi = .04f + .4f * a2; break;
    case SP_FM: *hi = .08f + .5f * a2; break;
    case SP_NOISE: *hi = .08f + .4f * a2; break;
    case SP_SUB: *hi = .3f + .5f * a2; break;
    case SP_CUTOFF: *lo = .35f - .12f * a; break;
    case SP_VCA_ATK: *hi = .6f; break;
    case SP_VCA_REL: *hi = .75f; break;
    case SP_VCF_ATK: *hi = .6f; break;
    case SP_LFO_LVL: *hi = .15f + .5f * a2; break;
    default: break;
    }
}

/* B: the characters */
typedef struct { uint8_t sp; float v; } Set;
#define END {255, 0}
typedef struct { const char *name; Set s[12]; } Character;
enum { FAM_SUSTAIN, FAM_STRUCK, FAM_BASS };
static const Character SUSTAIN[] = {
    {"PAD", {{SP_VCA_DEC, .3f}, {SP_SUSTAIN, 1.f}, {SP_VCA_ATK, .4f}, {SP_VCA_REL, .62f}, {SP_CUTOFF, .62f}, {SP_VCF_ENV, .1f},
             {SP_SPREAD, .25f}, {SP_LFO_LVL, .1f}, END}},
    {"LEAD", {{SP_VCA_DEC, .35f}, {SP_SUSTAIN, .75f}, {SP_VCA_ATK, .02f}, {SP_VCA_REL, .3f}, {SP_CUTOFF, .55f}, {SP_VCF_ENV, .4f},
              {SP_RESO, .35f}, {SP_SLIDE, .3f}, END}},
    {"PLUCK", {{SP_VCA_DEC, .45f}, {SP_SUSTAIN, 0}, {SP_VCA_ATK, 0}, {SP_VCA_REL, .3f}, {SP_CUTOFF, .45f}, {SP_VCF_ENV, .5f},
               {SP_VCF_DEC, .35f}, END}},
    {"KEYS", {{SP_VCA_DEC, .55f}, {SP_SUSTAIN, .35f}, {SP_VCA_ATK, 0}, {SP_VCA_REL, .4f}, {SP_CUTOFF, .8f}, {SP_VCF_ENV, .15f},
              {SP_RESO, .05f}, END}},
};
static const Character STRUCK[] = {
    {"RING", {{SP_VCA_DEC, .5f}, {SP_SUSTAIN, 1.f}, {SP_VCA_REL, .6f}, {SP_CUTOFF, .95f}, {SP_VCF_ENV, 0}, END}},
    {"BLIP", {{SP_VCA_DEC, .35f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .2f}, {SP_CUTOFF, .8f}, {SP_VCF_ENV, .3f}, END}},
    {"BELL", {{SP_VCA_DEC, .75f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .55f}, {SP_CUTOFF, 1.f}, {SP_VCF_ENV, 0}, END}},
};
static const Character BASS[] = {
    {"ACID", {{SP_DRIVE, .25f}, {SP_VCA_DEC, .45f}, {SP_SUSTAIN, 0}, {SP_VCF_ENV, .6f}, {SP_RESO, .65f}, {SP_CUTOFF, .3f},
              {SP_ACC_AMT, .7f}, {SP_FMODEL, 1}, END}},
    {"SUB", {{SP_DRIVE, .1f}, {SP_VCA_DEC, .3f}, {SP_SUSTAIN, 1.f}, {SP_VCA_REL, .15f}, {SP_CUTOFF, .35f}, {SP_RESO, .2f},
             {SP_VCF_ENV, .2f}, {SP_FMODEL, 0}, END}},
    {"PUNCH", {{SP_DRIVE, .35f}, {SP_VCA_DEC, .35f}, {SP_SUSTAIN, 0}, {SP_VCF_ENV, .45f}, {SP_RESO, .3f}, {SP_CUTOFF, .4f},
               {SP_PITCH_AMT, .1f}, END}},
};

static int family(int type)
{
    if (type == ENG_BAZZ) return FAM_BASS;
    if (type == ENG_PHYS || type == ENG_LXR_TONE || type == ENG_LXR_METAL) return FAM_STRUCK;
    return FAM_SUSTAIN;
}

/* one landing: up or down by up to span of the range, reflected at the ends, never where it is or just was */
static float land(Bl *x, float cur, float prev, float lo, float hi, float span)
{
    int tries;
    float v = cur;
    if (hi <= lo) return lo;
    if (span < 0.03f) span = 0.03f;
    for (tries = 0; tries < 12; tries++) {
        float d = 0.02f + unit(x) * (span - 0.02f);
        if (below(x, 2)) d = -d;
        v = cur + d;
        while (v < lo || v > hi) v = v < lo ? 2.f * lo - v : 2.f * hi - v;
        if (fabsf(v - cur) > 0.005f && fabsf(v - prev) > 0.005f) return v;
    }
    return v;
}

static const char *one_track(Bl *x, int t, float a)
{
    Sound *s = &x->p.snd[t];
    const int type = (int)(s->v[SP_VOICE] + 0.5f);
    const int fam = family(type);
    const Character *list = fam == FAM_BASS ? BASS : fam == FAM_STRUCK ? STRUCK : SUSTAIN;
    const int n = fam == FAM_BASS ? 3 : fam == FAM_STRUCK ? 3 : 4;
    float target[SP_COUNT];
    const char *name;
    int sp, i;
    /* B: the character (HOME more likely at a low amount) */
    sound_home(type, target);
    if (unit(x) < 0.15f + 0.35f * (1.f - a)) name = "HOME";
    else {
        const Character *c = &list[below(x, (uint32_t)n)];
        name = c->name;
        for (i = 0; i < 12 && c->s[i].sp != 255; i++) target[c->s[i].sp] = c->s[i].v;
    }
    for (sp = 0; sp < SP_COUNT; sp++) {
        const int nopts = SOUND_DEFS[sp].nopts;
        const float was = s->v[sp];
        if (skip(sp)) continue;
        if (nopts) {   /* a selector: the character's option, or another one now and then */
            if (unit(x) < 0.5f * a) {
                int o = (int)below(x, (uint32_t)(nopts - 1));
                if (o >= (int)was) o++;
                s->v[sp] = (float)o;
            } else if (unit(x) < a) s->v[sp] = target[sp];
        } else {
            float lo, hi, v = was + (target[sp] - was) * a;   /* towards the character by the amount */
            limits(sp, a, &lo, &hi);
            v = land(x, v < lo ? lo : v > hi ? hi : v, x->rnd2.prev[t][sp], lo, hi, 0.08f + 0.25f * a);
            s->v[sp] = v;
        }
        x->rnd2.prev[t][sp] = was;
    }
    /* A: the voice's own six sliders */
    for (i = 0; i < ENG_SLIDERS; i++) {
        const int st = ENG_DEFS[type].steps[i], k = SP_V1 + i;
        if (i == PR_OCT || i == PR_FINE) continue;   /* the play row's tuning stays */
        const float was = s->v[k];
        if (st > 1) {
            if (unit(x) < 0.7f * a) {
                int o = (int)below(x, (uint32_t)(st - 1)), cur = (int)(was * st);
                if (cur >= st) cur = st - 1;
                if (o >= cur) o++;
                s->v[k] = (o + 0.5f) / (float)st;
            }
        } else s->v[k] = land(x, was, x->rnd2.prev[t][k], 0.f, 1.f, a);
        x->rnd2.prev[t][k] = was;
    }
    sound_sync(s);
    return name;
}

const char *rnd_sounds(Bl *x, int pct, int track)
{
    const char *name = "";
    int t;
    if (pct <= 0) return name;
    if (pct > 100) pct = 100;
    for (t = 0; t < NUM_TRACKS; t++) {
        if (track >= 0 && t != track) continue;
        name = one_track(x, t, pct / 100.f);
    }
    x->displayRev++;
    return track >= 0 ? name : "";
}
