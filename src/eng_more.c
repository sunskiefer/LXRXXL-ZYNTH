/* LXRXXL ZYNTH -- WHEEL (a tonewheel organ with percussion, key click and a rotary speaker), VOICE (formants),
 * GRAIN (a granular cloud of single-cycle waves) and NOISE (coloured, crackling, digital and dust noises, tuned by
 * resonators). All written for LXRXXL ZYNTH. */
#include <string.h>
#include "eng_dsp.h"

/* ================================================================ WHEEL */
static const float BARS[9] = {0.5f, 1.5f, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f};   /* 16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' */

void wheel_render(WheelSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    float lv[9], sum = 0.f;
    uint32_t inc[9];
    const int perc = zone(p[3], 5);
    const float percRatio = perc == 1 || perc == 3 ? 2.f : 3.f;
    const float percDec = expf(-1.f / ((perc >= 3 ? 0.18f : 0.5f) * SR));
    const float clickDec = expf(-1.f / (0.004f * SR));
    const float ls = p[5];
    const int lesOn = ls >= 0.02f;
    const float depth = ls <= 0.5f ? ls * 2.f : (ls - 0.5f) * 2.f * 0.6f + 0.4f;
    const float tH = ls > 0.5f ? 6.7f : 0.8f, tD = ls > 0.5f ? 5.8f : 0.67f;
    const float splitA = 1.f - expf(-TWO_PI_F * 800.f / SR);
    int b, k;
    lv[0] = p[0]; lv[1] = p[0] * 0.7f;
    lv[2] = p[1]; lv[3] = p[1] * 0.8f;
    lv[4] = p[2] * 0.8f; lv[5] = p[2] * 0.7f; lv[6] = p[2] * 0.5f; lv[7] = p[2] * 0.5f; lv[8] = p[2] * 0.6f;
    for (b = 0; b < 9; b++) {
        float f = in->freq * BARS[b];
        while (f > 5900.f) f *= 0.5f;   /* the top tonewheels fold back an octave */
        inc[b] = ph_inc(f);
        sum += lv[b];
    }
    if (in->trig) {
        if (perc) s->perc = 1.f;
        s->click = p[4];
        if (!s->noise) s->noise = 0x1B873593u;
        if (s->ph[0] == 0) for (b = 0; b < 9; b++) s->ph[b] = (uint32_t)(b * 0x1C71C71Cu);
    }
    /* the rotors speed up and slow down: the horn quickly, the drum slowly (per block) */
    s->spdH += (tH - s->spdH) * (1.f - expf(-(float)n / (0.8f * SR)));
    s->spdD += (tD - s->spdD) * (1.f - expf(-(float)n / (3.0f * SR)));
    {
        const float norm = 1.f / (0.6f + 0.55f * sum);
        for (k = 0; k < n; k++) {
            float x = 0.f;
            for (b = 0; b < 9; b++) {
                s->ph[b] += inc[b];
                if (lv[b] > 0.f) x += lv[b] * esin(s->ph[b]);
            }
            x *= norm;
            if (s->perc > 0.0005f) {   /* the percussion: the 8' wheel's 2nd or 3rd harmonic, decaying */
                x += 0.6f * s->perc * esin((uint32_t)((uint64_t)s->ph[2] * (uint64_t)percRatio));
                s->perc *= percDec;
            }
            if (s->click > 0.0005f) {
                const float w = brand(&s->noise);
                s->clickLp += 0.35f * (w - s->clickLp);
                x += (w - s->clickLp) * s->click * 0.9f;
                s->click *= clickDec;
            }
            if (lesOn) {
                float lo, hi, d, fr, a, bb;
                int di, r0, r1;
                s->split += splitA * (x - s->split);
                lo = s->split;
                hi = x - lo;
                s->rotH += s->spdH / SR;
                s->rotD += s->spdD / SR;
                if (s->rotH >= 1.f) s->rotH -= 1.f;
                if (s->rotD >= 1.f) s->rotD -= 1.f;
                /* the horn: Doppler (a moving delay) and loudness; the drum: loudness */
                s->lsl[s->lw] = hi;
                d = (0.0015f + 0.0006f * depth * esinf(s->rotH)) * SR;
                di = (int)d;
                fr = d - (float)di;
                r0 = (s->lw - di) & (LESLIE_LEN - 1);
                r1 = (r0 - 1) & (LESLIE_LEN - 1);
                hi = s->lsl[r0] + fr * (s->lsl[r1] - s->lsl[r0]);
                s->lw = (s->lw + 1) & (LESLIE_LEN - 1);
                a = 1.f - 0.35f * depth * (0.5f - 0.5f * esinf(s->rotH + 0.25f));
                bb = 1.f - 0.25f * depth * (0.5f + 0.5f * esinf(s->rotD));
                x = hi * a + lo * bb;
            }
            out[k] = x;
        }
    }
}

/* ================================================================ VOICE: a glottal pulse through three formants */
static const float VOWELS[5][3] = {
    {730.f, 1090.f, 2440.f},   /* A */
    {530.f, 1840.f, 2480.f},   /* E */
    {270.f, 2290.f, 3010.f},   /* I */
    {570.f, 840.f, 2410.f},    /* O */
    {300.f, 870.f, 2240.f},    /* U */
};
static const float VGAIN[3] = {1.f, 0.55f, 0.3f};
static const float VBW[3] = {80.f, 95.f, 130.f};

void voice_fmt_render(VoiceSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const float vp = clampe(p[0], 0, 1) * 3.999f;
    const int vi = (int)vp;
    const float vf = vp - (float)vi;
    const float shift = exp2f((p[1] - 0.5f) * 1.2f);
    const float breath = p[2];
    const float vibD = p[3] * 0.6f;
    const float wide = 1.6f - 1.3f * p[4];
    const float toneA = 1.f - expf(-TWO_PI_F * in->freq * (1.5f + 30.f * p[5] * p[5]) / SR);
    SvfCo c[3];
    int j, k;
    if (in->trig) { s->vibAmt = 0.f; if (!s->noise) s->noise = 0x7FEB352Du; }
    for (j = 0; j < 3; j++) {
        const float f = (VOWELS[vi][j] + vf * (VOWELS[vi + 1][j] - VOWELS[vi][j])) * shift;
        c[j] = svf_co(f, f / (VBW[j] * wide));
    }
    /* the vibrato comes in after the note starts */
    s->vibAmt += (1.f - s->vibAmt) * (1.f - expf(-(float)n / (0.35f * SR)));
    for (k = 0; k < n; k++) {
        float src, x = 0.f;
        float f;
        uint32_t inc;
        s->vibPh += 5.5f / SR;
        if (s->vibPh >= 1.f) s->vibPh -= 1.f;
        f = in->freq * semis(vibD * s->vibAmt * esinf(s->vibPh));
        inc = ph_inc(f);
        s->ph += inc;
        src = saw_blep(ph_unit(s->ph), (float)inc / PH_ONE);
        s->lp += toneA * (src - s->lp);          /* the glottis's slope */
        src = s->lp * (1.f - breath * 0.7f) + breath * brand(&s->noise) * 0.8f;
        for (j = 0; j < 3; j++) x += VGAIN[j] * c[j].k * svf_bp(c[j], &s->s1[j], &s->s2[j], src, NULL);
        out[k] = x * 4.4f;
    }
}

/* ================================================================ GRAIN */
#define GW_N 16
#define GW_LEN 256
#define GW_MIPS 3
static float GWAVES[GW_MIPS][GW_N][GW_LEN + 1];
static const int GW_HARM[GW_MIPS] = {64, 16, 4};

static float gw_amp(int w, int h)
{
    const float hf = (float)h;
    switch (w) {
    case 0: return h == 1;
    case 1: return (h & 1) ? 1.f / (hf * hf * hf) : 0.f;
    case 2: return (h & 1) ? ((h >> 1) & 1 ? -1.f : 1.f) / (hf * hf) : 0.f;
    case 3: return (h & 1) ? 1.f / hf : 0.f;
    case 4: return 1.f / hf;
    case 5: return 1.f / powf(hf, 0.7f);
    case 6: return h == 1 || h == 2 || h == 3 || h == 4 || h == 6 || h == 8 ? 0.8f / sqrtf(hf) : 0.f;
    case 7: return expf(-0.5f * (hf - 7.f) * (hf - 7.f) / 4.f) + 0.6f * expf(-0.5f * (hf - 10.f) * (hf - 10.f) / 4.f) + (h == 1 ? 0.4f : 0.f);
    case 8: return expf(-0.5f * (hf - 2.5f) * (hf - 2.5f)) + 0.5f * expf(-0.5f * (hf - 21.f) * (hf - 21.f) / 6.f);
    case 9: return h == 1 ? 1.f : h == 3 ? 0.6f : h == 5 ? 0.5f : h == 9 ? 0.35f : h == 13 ? 0.25f : 0.f;
    case 10: return h == 1 ? 0.5f : ((h & 1) && h >= 3 && h <= 9) ? 0.8f : 0.f;
    case 11: return sinf(3.14159265f * hf * 0.25f) / hf;
    case 12: return h % 3 == 1 ? 1.f / sqrtf(hf) : 0.f;
    case 13: { uint32_t r = 0x12345u + (uint32_t)h * 2654435761u; return frand(&r) / sqrtf(hf); }
    case 14: return h == 1 || h == 2 || h == 4 || h == 8 || h == 16 || h == 32 ? 1.f / sqrtf(hf) : 0.f;
    default: return h >= 8 && h <= 16 ? 0.7f : h == 1 ? 0.4f : 0.f;
    }
}

void grain_tables_init(void)
{
    int m, w, i, h;
    for (m = 0; m < GW_MIPS; m++)
        for (w = 0; w < GW_N; w++) {
            float peak = 1e-6f;
            for (i = 0; i < GW_LEN; i++) {
                float v = 0.f;
                for (h = 1; h <= GW_HARM[m]; h++) {
                    const float a = gw_amp(w, h);
                    if (a != 0.f) v += a * sinf(TWO_PI_F * (float)h * (float)i / GW_LEN);
                }
                GWAVES[m][w][i] = v;
                if (fabsf(v) > peak) peak = fabsf(v);
            }
            for (i = 0; i < GW_LEN; i++) GWAVES[m][w][i] /= peak;
            GWAVES[m][w][GW_LEN] = GWAVES[m][w][0];
        }
}

static inline float gw_read(int m, float pos, uint32_t ph)
{
    const float pp = pos * (GW_N - 1.001f);
    const int w = (int)pp;
    const float wf = pp - (float)w;
    const uint32_t i = ph >> 24;
    const float fr = (float)(ph & 0xFFFFFFu) * (1.f / 16777216.f);
    const float *a = GWAVES[m][w], *b = GWAVES[m][w + 1];
    const float va = a[i] + fr * (a[i + 1] - a[i]);
    const float vb = b[i] + fr * (b[i + 1] - b[i]);
    return va + wf * (vb - va);
}

void grain_render(GrainSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const float size = 0.015f + p[0] * p[0] * 0.5f;            /* seconds */
    const float rate = 2.f + p[1] * p[1] * 150.f;               /* grains a second */
    const float spray = p[3];
    const float spread = p[4];
    const float lpA = 1.f - expf(-TWO_PI_F * 200.f * exp2f(p[5] * 7.f) / SR);
    const float overlap = rate * size;
    const float norm = 1.f / sqrtf(overlap < 1.f ? 1.f : overlap);
    int k, g;
    if (!s->rnd) s->rnd = 0x3C6EF372u;
    for (k = 0; k < n; k++) {
        float x = 0.f;
        s->next -= 1.f;
        if (s->next <= 0.f) {   /* a new grain */
            s->next += SR / rate * (0.7f + 0.6f * frand(&s->rnd));
            for (g = 0; g < MAX_GRAINS; g++) if (!s->g[g].on) break;
            if (g < MAX_GRAINS) {
                Grain *gr = &s->g[g];
                float semi = brand(&s->rnd) * spread * 12.f;
                float f;
                int mip;
                if (spread > 0.66f) semi = 12.f * floorf(semi / 12.f + 0.5f);
                f = in->freq * semis(semi);
                gr->inc = ph_inc(f);
                gr->ph = xrand(&s->rnd);
                gr->pos = clampe(p[2] + spray * brand(&s->rnd) * 0.5f, 0.f, 1.f);
                gr->t = 0.f;
                gr->dt = 256.f / (size * SR);
                mip = f * 64.f < SR * 0.5f ? 0 : f * 16.f < SR * 0.5f ? 1 : 2;
                gr->on = (uint8_t)(1 + mip);
            }
        }
        for (g = 0; g < MAX_GRAINS; g++) {
            Grain *gr = &s->g[g];
            int wi;
            float wfr;
            if (!gr->on) continue;
            wi = (int)gr->t;
            wfr = gr->t - (float)wi;
            x += (ENG_HANN[wi] + wfr * (ENG_HANN[wi + 1] - ENG_HANN[wi])) * gw_read(gr->on - 1, gr->pos, gr->ph);
            gr->ph += gr->inc;
            gr->t += gr->dt;
            if (gr->t >= 256.f) gr->on = 0;
        }
        x *= norm;
        s->lp += lpA * (x - s->lp);
        out[k] = s->lp * 1.5f;
    }
}

/* ================================================================ NOISE */
void noise_render(NoiseSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const int type = zone(p[0], 5);
    const float col = p[1];
    const float lpA = 1.f - expf(-TWO_PI_F * 200.f * exp2f((col < 0.5f ? col : 0.5f) * 13.f) / SR);
    const float hpA = 1.f - expf(-TWO_PI_F * 20.f * exp2f((col > 0.5f ? col - 0.5f : 0.f) * 17.2f) / SR);
    const float reso = p[2], harm = p[3];
    const float q = 1.f + reso * reso * 120.f;
    const float dens = p[4];
    const float grit = p[5];
    const float levels = exp2f(15.f - grit * 12.f);
    const float holdHz = SR * exp2f(-grit * 4.f);
    SvfCo c[3];
    float g[3];
    int j, k;
    for (j = 0; j < 3; j++) {
        const float f = in->freq * (float)(j + 1);
        c[j] = svf_co(f, q);
        g[j] = sqrtf(SR * q / (2.f * (f > 20.f ? f : 20.f))) * 0.3f;
        if (g[j] > 60.f) g[j] = 60.f;
    }
    if (!s->rnd) s->rnd = 0x85EBCA6Bu;
    if (!s->lfsr) s->lfsr = 0x4001u;
    if (s->crack == 0.f && s->b[5] == 0.f) { s->crack = 0.3f; s->b[5] = 0.1f; }
    for (k = 0; k < n; k++) {
        float w = brand(&s->rnd), x, y;
        switch (type) {
        case 1:   /* pink (Paul Kellet's filter) */
            s->b[0] = 0.99886f * s->b[0] + w * 0.0555179f;
            s->b[1] = 0.99332f * s->b[1] + w * 0.0750759f;
            s->b[2] = 0.96900f * s->b[2] + w * 0.1538520f;
            s->b[3] = 0.86650f * s->b[3] + w * 0.3104856f;
            s->b[4] = 0.55000f * s->b[4] + w * 0.5329522f;
            x = (s->b[0] + s->b[1] + s->b[2] + s->b[3] + s->b[4] + w * 0.5362f) * 0.2f;
            break;
        case 2: {   /* crackle: a chaotic map */
            const float a = 1.5f + 0.49f * dens;
            const float yv = fabsf(a * s->crack - s->b[5] - 0.05f);
            s->b[5] = s->crack;
            s->crack = yv > 2.f ? 0.3f : yv;
            x = (s->crack - 0.5f) * 1.6f;
            break;
        }
        case 3: {   /* digital: a short LFSR clocked by DENSITY */
            s->holdPh += 500.f * exp2f(dens * 6.f) / SR;
            if (s->holdPh >= 1.f) {
                const uint32_t fb = (s->lfsr ^ (s->lfsr >> 6)) & 1u;
                s->holdPh -= floorf(s->holdPh);
                s->lfsr = (s->lfsr >> 1) | (fb << 14);
            }
            x = (s->lfsr & 1u) ? 0.7f : -0.7f;
            break;
        }
        case 4:   /* dust: random clicks, DENSITY a second */
            x = frand(&s->rnd) < 5.f * exp2f(dens * 10.f) / SR ? brand(&s->rnd) * 2.5f : 0.f;
            break;
        default: x = w; break;   /* white */
        }
        s->lp += lpA * (x - s->lp);
        x = s->lp;
        s->hp += hpA * (x - s->hp);
        x = col > 0.5f ? x - s->hp : x;
        /* the resonators, tuned to the note (and its 2nd and 3rd harmonics) */
        y = g[0] * c[0].k * svf_bp(c[0], &s->s1[0], &s->s2[0], x, NULL);
        if (harm > 0.001f) {
            y += harm * g[1] * c[1].k * svf_bp(c[1], &s->s1[1], &s->s2[1], x, NULL) * 0.8f;
            y += harm * g[2] * c[2].k * svf_bp(c[2], &s->s1[2], &s->s2[2], x, NULL) * 0.6f;
        }
        x = x * (1.f - reso) + y * reso;
        if (grit > 0.005f) {
            s->dust += holdHz / SR;
            if (s->dust >= 1.f) { s->dust -= floorf(s->dust); s->hold = floorf(x * levels + 0.5f) / levels; }
            x = s->hold;
        }
        out[k] = clampe(x * 1.6f, -2.f, 2.f) * 0.9f;
    }
}
