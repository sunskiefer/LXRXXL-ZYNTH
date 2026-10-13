/* LXRXXL ZYNTH -- the delay (from LXRXXL BAZZLINE) and the gate/duck, after the whole mix.
 *
 * The delay follows the Erica Synths DIY Delay's manual (the module's DSP is firmware on its pre-programmed MCU board and
 * is not in Erica's DIY files: this is written here to behave as the manual describes, it is not Erica's code):
 *   TIME 0..800 ms; synced (TAP SYNC), TIME picks a divider or multiplier of the period (/8 /6 /4 /3 /2 x1 x2 x3 x4
 *   x6 x8), never longer than 800 ms; TAPE (the time slews, so turning it bends the pitch; darker, saturating repeats)
 *   or DIGITAL (a new time crossfades, no pitch shift); IN LVL; FEEDBACK (self-oscillates at the top); DRY/WET a
 *   crossfade; HOLD loops what is in the buffer (TIME long); ADD layers the input onto the held loop; REVERSE plays
 *   the repeats (and the held loop) backwards.
 * The gate/duck: a trigger (a MIDI note on its channel) ducks the mix by DEPTH (DUCK) or opens it by DEPTH from closed
 * (GATE), with ATTACK, HOLD and RELEASE. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bl.h"

#define FX_LEN 131072    /* 2.97 s: REVERSE reads up to twice TIME back */
#define FX_MASK (FX_LEN - 1)
#define MAX_DELAY 0.8f

static const float SYNC_FACTORS[11] = {1.f / 8, 1.f / 6, 1.f / 4, 1.f / 3, 1.f / 2, 1, 2, 3, 4, 6, 8};

void fx_init(Fx *f)
{
    float *a = f->buf[0], *b = f->buf[1];
    memset(f, 0, sizeof *f);
    f->buf[0] = a ? a : calloc(FX_LEN, sizeof(float));
    f->buf[1] = b ? b : calloc(FX_LEN, sizeof(float));
    if (f->buf[0]) memset(f->buf[0], 0, FX_LEN * sizeof(float));
    if (f->buf[1]) memset(f->buf[1], 0, FX_LEN * sizeof(float));
    f->rd = f->rdOld = 0.4f * SR;
    f->xfade = 1.f;
}

void fx_free(Fx *f)
{
    free(f->buf[0]);
    free(f->buf[1]);
    f->buf[0] = f->buf[1] = NULL;
}

int fx_syncFactorIndex(float knob) { return (int)lroundf(clampf(knob, 0, 1) * 10.f); }

/* the delay time in seconds, as TIME and the sync state say */
float fx_delaySeconds(Bl *x)
{
    const float knob = x->p.g[G_DLY_TIME];
    float t;
    if (x->p.dlySync == 0) t = knob * MAX_DELAY;
    else {
        const float period = x->p.dlySync == 2 && x->p.dlyTap > 0.f ? x->p.dlyTap : 60.f / bl_tempo(x);
        t = period * SYNC_FACTORS[fx_syncFactorIndex(knob)];
        while (t > MAX_DELAY) t *= 0.5f;   /* the module's limit: halve until it fits */
    }
    return t < 0.001f ? 0.001f : t;
}

void fx_duckTrigger(Bl *x)
{
    Fx *f = &x->fx;
    f->gdStage = 1;
}

static inline float rdbuf(const float *b, int w, float d)
{
    const int di = (int)d;
    const float fr = d - di;
    const float a = b[(w - di) & FX_MASK], c = b[(w - di - 1) & FX_MASK];
    return a + fr * (c - a);
}

static inline float sat(float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

void fx_render(Bl *x, float *L, float *R, const float *sL, const float *sR, int n)
{
    Fx *f = &x->fx;
    const float *g = x->p.g;
    const int tape = g[G_DLY_MODE] < 1.f;
    const float target = fx_delaySeconds(x) * SR;
    const float fbT = g[G_DLY_FB] * 1.08f;
    const float wetT = g[G_DLY_WET];
    const float inT = g[G_DLY_IN] * 1.6f;
    const float lpA = 1.f - expf(-6.2831853f * (tape ? 4500.f : 15000.f) / SR);
    const float hpA = 1.f - expf(-6.2831853f * 40.f / SR);
    const float atk = 1.f - expf(-1.f / (0.0005f * powf(400.f, g[G_GD_ATK]) * SR));
    const float rel = expf(-1.f / (0.01f * powf(200.f, g[G_GD_REL]) * SR));
    const uint32_t holdN = (uint32_t)(g[G_GD_HOLD] * SR);
    const float depth = g[G_GD_DEPTH];
    const int duck = g[G_GD_MODE] >= 1.f;
    const int gdOn = x->p.gdOn;
    int i, c;
    if (!f->buf[0] || !f->buf[1]) return;
    /* DIGITAL: a new time crossfades from the old tap */
    if (!tape && fabsf(target - f->rd) > 1.f && f->xfade >= 1.f) {
        f->rdOld = f->rd;
        f->rd = target;
        f->xfade = 0.f;
    }
    if (!f->hold) f->holdLen = tape ? f->rd : target;
    for (i = 0; i < n; i++) {
        float wet[2] = {0, 0};
        float d;
        f->wetSm += 0.001f * (wetT - f->wetSm);
        f->fbSm += 0.001f * (fbT - f->fbSm);
        f->inSm += 0.001f * (inT - f->inSm);
        if (tape) {
            f->rd += 0.00012f * (target - f->rd);   /* the tape's slew: a time change bends the pitch */
            f->wowPh += 0.55 / SR;
            if (f->wowPh >= 1.0) f->wowPh -= 1.0;
            d = f->rd + 6.f * sinf(6.2831853f * (float)f->wowPh);
        } else d = f->rd;
        if (f->hold) d = f->holdLen;
        if (d < 2.f) d = 2.f;
        for (c = 0; c < 2; c++) {
            const float *b = f->buf[c];
            float y;
            if (f->rev) {
                /* backwards: two heads half a window apart, each running back through its window, crossfaded */
                const float win = d;
                const float j0 = (float)f->revPos, j1 = fmodf(j0 + win * 0.5f, win);
                const float w0 = 1.f - fabsf(2.f * j0 / win - 1.f), w1 = 1.f - fabsf(2.f * j1 / win - 1.f);
                y = rdbuf(b, f->w, 2.f * j0 + 1.f) * w0 + rdbuf(b, f->w, 2.f * j1 + 1.f) * w1;
            } else if (!tape && f->xfade < 1.f && !f->hold) {
                y = rdbuf(b, f->w, f->rdOld) * (1.f - f->xfade) + rdbuf(b, f->w, d) * f->xfade;
            } else y = rdbuf(b, f->w, d);
            wet[c] = y;
        }
        if (f->rev) {
            f->revPos++;
            if (f->revPos >= (int)d) f->revPos = 0;
        }
        if (!tape && f->xfade < 1.f) f->xfade += 1.f / 2048.f;
        for (c = 0; c < 2; c++) {
            float *b = f->buf[c];
            const float in = c ? R[i] : L[i];      /* the dry mix */
            const float sin_ = c ? sR[i] : sL[i];  /* what the tracks send to the delay */
            float wr;
            if (f->hold) {
                /* HOLD: the buffer loops TIME long; ADD layers the input on it */
                const float loop = rdbuf(b, f->w, f->holdLen);
                wr = loop + (f->add ? sat(sin_ * f->inSm) * 0.7f : 0.f);
                wr = sat(wr * 0.98f);
            } else {
                float fb = wet[c] * f->fbSm;
                f->lpz[c] += lpA * (fb - f->lpz[c]);
                fb = f->lpz[c];
                f->hpz[c] += hpA * (fb - f->hpz[c]);
                fb -= f->hpz[c];
                wr = sat(sin_ * f->inSm + (tape ? sat(fb * 1.2f) * 0.85f : fb));
            }
            b[f->w] = wr;
            /* the analogue crossfader */
            if (c) R[i] = in * (1.f - f->wetSm) + wet[c] * f->wetSm;
            else L[i] = in * (1.f - f->wetSm) + wet[c] * f->wetSm;
        }
        f->w = (f->w + 1) & FX_MASK;
        /* the gate/duck */
        switch (f->gdStage) {
        case 1: f->gdEnv += atk * (1.02f - f->gdEnv); if (f->gdEnv >= 1.f) { f->gdEnv = 1.f; f->gdStage = 2; f->gdHold = holdN; } break;
        case 2: if (f->gdHold) f->gdHold--; else f->gdStage = 3; break;
        case 3: f->gdEnv *= rel; if (f->gdEnv < 1e-4f) { f->gdEnv = 0; f->gdStage = 0; } break;
        default: break;
        }
        if (gdOn) {
            const float gn = duck ? 1.f - depth * f->gdEnv : 1.f - depth * (1.f - f->gdEnv);
            L[i] *= gn;
            R[i] *= gn;
        }
    }
}
