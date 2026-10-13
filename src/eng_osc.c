/* LXRXXL ZYNTH -- the oscillator voices: ANALOG, TRIO, CHIP, PHASE, LXR TONE, LXR METAL and BAZZ.
 *
 * LXR TONE, LXR METAL and BAZZ read the LXR's band-limited tables (third_party/lxr02open wavetable.c, LXR02 Open-Source
 * license), one table per octave of the note, as the LXR's oscillator does. Everything else here is written for
 * LXRXXL ZYNTH. */
#include <string.h>
#include "eng_dsp.h"
#include "wavetable.h"

extern const float LXR_FM_RATIOS[16];

/* ================================================================ the LXR tables (as in LXRXXL BAZZLINE) */
static uint32_t wave_offset[4];   /* each table's phase so SQR, SAW, TRI and the sine start together */

static inline float rd1024(const int16_t *t, uint32_t ph)
{
    const uint32_t i = ph >> 22;
    const float fr = (float)(ph & 0x3FFFFFu) * (1.f / 4194304.f);
    const int a = t[i], b = t[(i + 1) & 1023];
    return ((float)a + fr * (float)(b - a)) * (1.f / 32767.f);
}

static inline float rd_sine(uint32_t ph)
{
    const uint32_t i = ph >> 20;   /* TABLESIZE 4096 (+1 guard) */
    const float fr = (float)(ph & 0xFFFFFu) * (1.f / 1048576.f);
    const int a = sine_table[i], b = sine_table[i + 1];
    return ((float)a + fr * (float)(b - a)) * (1.f / 32767.f);
}

void bazz_tables_init(void)
{
    const int16_t *tabs[3] = {recTable[0], sawTable[0], triTable[0]};
    int w, i;
    double ref, re = 0, im = 0;
    for (i = 0; i < TABLESIZE; i++) {
        re += sine_table[i] * cos(6.283185307 * i / TABLESIZE);
        im += sine_table[i] * sin(6.283185307 * i / TABLESIZE);
    }
    ref = atan2(im, re);
    for (w = 0; w < 3; w++) {
        double f;
        re = im = 0;
        for (i = 0; i < 1024; i++) { re += tabs[w][i] * cos(6.283185307 * i / 1024.0); im += tabs[w][i] * sin(6.283185307 * i / 1024.0); }
        f = (atan2(im, re) - ref) / 6.283185307;
        f -= floor(f);
        wave_offset[w] = (uint32_t)(f * 4294967296.0);
    }
    wave_offset[3] = 0;
}

/* wave 0 SQR, 1 SAW, 2 TRI, 3 SINE */
static inline float lxr_wave(int w, int ti, uint32_t ph)
{
    switch (w) {
    case 0: return rd1024(recTable[ti], ph + wave_offset[0]);
    case 1: return rd1024(sawTable[ti], ph + wave_offset[1]);
    case 2: return rd1024(triTable[ti], ph + wave_offset[2]);
    default: return rd_sine(ph);
    }
}

/* the LXR's table for the octave of a note (Oscillator.c freqToTableIndex), from the note */
static inline int lxr_table(float note)
{
    return note < 0.f ? 0 : note >= 132.f ? 10 : (int)(note / 12.f);
}

/* WAVE morph: SINE -> TRI -> SAW -> SQR along pos 0..1 */
static inline float lxr_morph(float pos, int ti, uint32_t ph)
{
    static const int order[4] = {3, 2, 1, 0};
    const float p = clampe(pos, 0, 1) * 2.999f;
    const int i = (int)p;
    const float fr = p - (float)i;
    const float a = lxr_wave(order[i], ti, ph);
    return fr < 0.002f ? a : a + fr * (lxr_wave(order[i + 1], ti, ph) - a);
}

/* ================================================================ ANALOG: two polyBLEP oscillators, sync */
static const float AN_INT[9] = {0, 3, 4, 5, 7, 12, 19, 24, -12};

void analog_render(AnalogSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const float w = p[0];
    const float pw = 0.5f - 0.45f * p[1];
    const float det = p[2] * 0.5f;                      /* up to 50 cents */
    const float iv = AN_INT[zone(p[3], 9)];
    const float sync = p[4];
    const float mix = p[5];
    const int synced = sync > 0.01f;
    const float f1 = in->freq;
    const float f2 = synced ? f1 * (1.f + sync * 5.f) * semis(iv) : f1 * semis(iv + det);
    const float g1 = cosf(mix * 1.5707963f) * 1.15f, g2 = sinf(mix * 1.5707963f) * 1.15f;
    const int os = synced ? 2 : 1;                       /* sync: twice the rate, then averaged */
    const uint32_t i1 = ph_inc(f1) / (uint32_t)os, i2 = ph_inc(f2) / (uint32_t)os;
    const float dt1 = (float)i1 / PH_ONE, dt2 = (float)i2 / PH_ONE;
    int k, o;
    if (in->trig && s->ph[0] == 0 && s->ph[1] == 0) s->ph[1] = 0x40000000u;
    for (k = 0; k < n; k++) {
        float acc = 0.f;
        for (o = 0; o < os; o++) {
            const uint32_t old = s->ph[0];
            float t1, t2, a, b;
            s->ph[0] += i1;
            s->ph[1] += i2;
            if (synced && s->ph[0] < old) s->ph[1] = (uint32_t)((float)s->ph[0] * (dt2 / dt1));   /* osc 1 restarts osc 2 */
            t1 = ph_unit(s->ph[0]);
            t2 = ph_unit(s->ph[1]);
            a = (1.f - w) * saw_blep(t1, dt1) + w * sq_blep(t1, dt1, pw);
            b = (1.f - w) * saw_blep(t2, dt2) + w * sq_blep(t2, dt2, pw);
            acc += g1 * a + g2 * b;
        }
        out[k] = acc * (0.5f / (float)os) * 1.3f;
    }
}

/* ================================================================ TRIO: three oscillators, Minimoog style */
void trio_render(TrioSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const float w = p[0];
    const float i2 = (float)(zone(p[1], 25) - 12), i3 = (float)(zone(p[2], 49) - 24);
    const float det = p[3] * 0.35f;
    const float l2 = p[4], l3 = p[5];
    const float norm = 1.3f / (1.f + l2 + l3);
    const uint32_t inc[3] = {ph_inc(in->freq * semis(-det)), ph_inc(in->freq * semis(i2 + det * 0.6f)),
                             ph_inc(in->freq * semis(i3 + det))};
    const float dt[3] = {(float)inc[0] / PH_ONE, (float)inc[1] / PH_ONE, (float)inc[2] / PH_ONE};
    const float lv[3] = {1.f, l2, l3};
    int k, o;
    if (in->trig && !s->ph[0] && !s->ph[1] && !s->ph[2]) { s->ph[1] = 0x55555555u; s->ph[2] = 0xAAAAAAAAu; }
    for (k = 0; k < n; k++) {
        float acc = 0.f;
        for (o = 0; o < 3; o++) {
            float t;
            s->ph[o] += inc[o];
            t = ph_unit(s->ph[o]);
            acc += lv[o] * ((1.f - w) * saw_blep(t, dt[o]) + w * sq_blep(t, dt[o], 0.5f));
        }
        out[k] = acc * norm;
    }
}

/* ================================================================ CHIP: console and computer chips */
static const int8_t ARPS[8][4] = {{0, 0, 0, 0}, {0, 12, 0, 12}, {0, 4, 7, 12}, {0, 3, 7, 12},
                                  {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 5, 7, 12}, {0, 7, 12, 7}};

void chip_render(ChipSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const int wave = zone(p[0], 7);
    const float bits = 2.f + 14.f * p[1];
    const float q = exp2f(bits - 1.f);
    const float rate = 2000.f * exp2f(p[2] * 4.4627f);   /* 2 kHz .. 44.1 kHz */
    const int arp = zone(p[3], 8);
    const float speed = (6.f + p[4] * 54.f) / SR;       /* the arpeggio: 6 .. 60 notes a second */
    const float sweep = (p[5] - 0.5f) * 2.f;           /* semitones a second: -/+ 96 */
    int k;
    if (in->trig) { s->sweep = 0.f; s->arpI = 0; s->arpPh = 0.f; if (!s->lfsr) s->lfsr = 1; }
    if (!s->lfsr) s->lfsr = 1;
    for (k = 0; k < n; k++) {
        float note = in->note, x;
        if (arp) {
            s->arpPh += speed;
            if (s->arpPh >= 1.f) { s->arpPh -= 1.f; s->arpI = (s->arpI + 1) & 3; }
            note += ARPS[arp][s->arpI];
        }
        s->sweep = clampe(s->sweep + sweep * fabsf(sweep) * 96.f / SR, -48.f, 48.f);
        note += s->sweep;
        {
            const float f = 440.f * exp2f((note - 69.f) / 12.f);
            const uint32_t inc = ph_inc(f);
            const float dt = (float)inc / PH_ONE;
            float t;
            if (wave >= 5) {   /* the noise channel: a 15-bit LFSR clocked at 8x the pitch, long (5) or short, metallic (6) */
                const uint32_t ni = (inc > 0x1FFFFFFFu ? 0x1FFFFFFFu : inc) * 8u;
                const uint64_t sum = (uint64_t)s->ph + ni;
                s->ph = (uint32_t)sum;
                if (sum >> 32) {
                    const int tap = wave == 6 ? 6 : 1;
                    const uint32_t fb = (s->lfsr ^ (s->lfsr >> tap)) & 1u;
                    s->lfsr = (s->lfsr >> 1) | (fb << 14);
                    s->noiseOut = (s->lfsr & 1u) ? 0.8f : -0.8f;
                }
                x = s->noiseOut;
            } else {
                s->ph += inc;
                t = ph_unit(s->ph);
                switch (wave) {
                case 0: x = sq_blep(t, dt, 0.125f) * 0.9f + 0.65f; break;
                case 1: x = sq_blep(t, dt, 0.25f) * 0.9f + 0.45f; break;
                case 2: x = sq_blep(t, dt, 0.5f) * 0.8f; break;
                case 3: {   /* the NES's 32-step triangle */
                    const int st = (int)(t * 32.f);
                    const int lv = st < 16 ? 15 - st : st - 16;
                    x = (float)lv / 7.5f - 1.f;
                    break;
                }
                default: x = (float)((int)(t * 16.f)) / 7.5f - 1.f; break;   /* a 16-step saw */
                }
            }
        }
        /* bits, then the sample rate */
        if (bits < 15.9f) x = floorf(x * q + 0.5f) / q;
        s->holdPh += rate / SR;
        if (s->holdPh >= 1.f || rate >= 44000.f) { s->holdPh -= floorf(s->holdPh); s->hold = x; }
        out[k] = s->hold * 0.8f;
    }
}

/* ================================================================ PHASE: Casio CZ style phase distortion */
static inline float pd_phase(int w, float t, float d)
{
    const float m = 0.5f - 0.495f * d;      /* the knee */
    const float q = 1.f - 0.985f * d;       /* the squeeze */
    switch (w) {
    case 0: return t < m ? 0.5f * t / m : 0.5f + 0.5f * (t - m) / (1.f - m);                     /* SAW */
    case 1: {                                                                                   /* SQUARE */
        const float h = t < 0.5f ? 2.f * t : 2.f * t - 1.f;
        const float u = h / q < 1.f ? h / q : 1.f;
        return t < 0.5f ? 0.5f * u : 0.5f + 0.5f * u;
    }
    case 2: return t / q < 1.f ? t / q : 1.f;                                                   /* PULSE */
    case 3: {                                                                                   /* DOUBLE SINE */
        const float h = t < 0.5f ? 2.f * t : 2.f * t - 1.f;
        const float u = h < m ? 0.5f * h / m : 0.5f + 0.5f * (h - m) / (1.f - m);
        return t < 0.5f ? u : 1.f - u;
    }
    default: {                                                                                  /* SAW PULSE */
        if (t < 0.5f) { const float h = 2.f * t; return 0.5f * (h < m ? 0.5f * h / m : 0.5f + 0.5f * (h - m) / (1.f - m)); }
        { const float h = 2.f * t - 1.f; return 0.5f + 0.5f * (h / q < 1.f ? h / q : 1.f); }
    }
    }
}

static inline float pd_sample(int w, float t, float d)
{
    if (w >= 5) {   /* the resonance waves: a cosine at a rising ratio inside a window */
        const float r = 1.f + d * 14.f;
        float win;
        if (w == 5) win = 1.f - t;
        else if (w == 6) win = 1.f - fabsf(2.f * t - 1.f);
        else win = (1.f - t) * 2.f < 1.f ? (1.f - t) * 2.f : 1.f;
        return win * (1.f - esinf(t * r + 0.25f)) - 0.5f * win;
    }
    return -esinf(pd_phase(w, t, d) + 0.25f);   /* -cos */
}

void phase_render(PdSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const int w = zone(p[0], 8);
    const float dec = expf(-1.f / ((0.02f + 3.f * p[3] * p[3]) * SR));
    const float l2 = p[5];
    const uint32_t inc1 = ph_inc(in->freq) >> 1;            /* 2x oversampled */
    const uint32_t inc2 = ph_inc(in->freq * semis(p[4] * 0.4f)) >> 1;
    const float norm = 1.f / (1.f + 0.7f * l2);
    int k;
    if (in->trig) s->env = 1.f;
    for (k = 0; k < n; k++) {
        const float d = clampe(p[1] + p[2] * s->env, 0.f, 1.f);
        float acc = 0.f;
        int o;
        for (o = 0; o < 2; o++) {
            s->ph[0] += inc1;
            acc += pd_sample(w, ph_unit(s->ph[0]), d);
            if (l2 > 0.001f) { s->ph[1] += inc2; acc += 0.7f * l2 * pd_sample(w, ph_unit(s->ph[1]), d); }
        }
        s->env *= dec;
        s->z += 0.6f * (acc * 0.5f - s->z);   /* the decimation's smoothing */
        out[k] = s->z * norm * 0.9f;
    }
}

/* ================================================================ LXR TONE: the LXR drum voice's oscillator, pitched */
void lxrtone_render(LxrToneSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    static const int WMAP[4] = {3, 2, 1, 0};   /* SINE TRI SAW SQR -> the table's order */
    const int w = WMAP[zone(p[0], 4)], fw = WMAP[zone(p[3], 4)];
    const float ratio = LXR_FM_RATIOS[zone(p[2], 16)];
    const float amt = p[1] * p[1] * 8.f;                   /* the FM index, radians */
    const float envAmt = p[4];
    const float fdec = expf(-1.f / ((0.015f + 0.6f * (1.f - envAmt) * (1.f - envAmt)) * SR));
    const float hz = SR * exp2f(-p[5] * 6.f);              /* DECIMATE: 44.1 kHz down to 690 Hz */
    const int ti = lxr_table(in->note), tiM = lxr_table(in->note + 12.f * log2f(ratio));
    const uint32_t inc = ph_inc(in->freq), incM = ph_inc(in->freq * ratio);
    const float pmScale = PH_ONE / TWO_PI_F;
    int k;
    if (in->trig) { s->fmEnv = 1.f; s->ph = 0; s->phMod = 0; }
    for (k = 0; k < n; k++) {
        const float idx = amt * (1.f - envAmt + envAmt * s->fmEnv);
        const float m = lxr_wave(fw, tiM, s->phMod);
        const float x = lxr_wave(w, ti, s->ph + (uint32_t)(int64_t)(m * idx * pmScale));
        s->ph += inc;
        s->phMod += incM;
        s->fmEnv *= fdec;
        s->holdPh += hz / SR;
        if (s->holdPh >= 1.f || p[5] < 0.005f) { s->holdPh -= floorf(s->holdPh); s->hold = x; }
        out[k] = s->hold;
    }
}

/* ================================================================ LXR METAL: the LXR's hi-hat / cymbal, pitched */
static const float METAL[4][6] = {
    {1.f, 1.4471f, 1.617f, 1.9265f, 2.5028f, 2.6637f},   /* the 808's hats */
    {1.f, 1.3420f, 1.2312f, 1.6532f, 1.9523f, 2.1523f},   /* a cymbal */
    {1.f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f},        /* a bell's modes */
    {1.f, 1.183f, 1.506f, 1.741f, 2.0f, 2.473f},          /* a gong */
};

void lxrmetal_render(LxrMetalSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const float c = clampe(p[0], 0, 1) * 2.999f;
    const int ci = (int)c;
    const float cf = c - (float)ci;
    const float spread = 0.3f + p[1] * 1.4f;
    const float fm = p[2] * p[2] * 0.9f;
    const float noise = p[3];
    const float ring = p[5];
    const SvfCo bp = svf_co(in->freq * exp2f(1.f + p[4] * 4.f), 1.4f);
    uint32_t inc[6];
    int o, k;
    for (o = 0; o < 6; o++) {
        float r = METAL[ci][o] + cf * (METAL[ci + 1 < 4 ? ci + 1 : 3][o] - METAL[ci][o]);
        r = 1.f + (r - 1.f) * spread;
        inc[o] = ph_inc(in->freq * r);
    }
    if (in->trig && !s->noise) s->noise = 0x6C8E9CF5u;
    for (k = 0; k < n; k++) {
        float sq[6], sum = 0.f, rm, x, lp;
        const float mod = (int32_t)s->ph[0] < 0 ? 1.f : -1.f;
        for (o = 0; o < 6; o++) {
            sq[o] = (int32_t)s->ph[o] < 0 ? 1.f : -1.f;
            sum += sq[o];
            s->ph[o] += o ? (uint32_t)((float)inc[o] * (1.f + fm * mod)) : inc[o];
        }
        rm = (sq[0] * sq[1] + sq[2] * sq[3] + sq[4] * sq[5]) * (1.f / 3.f);
        x = (1.f - ring) * sum * (1.f / 6.f) + ring * rm;
        x = x * (1.f - noise) + noise * brand(&s->noise);
        x = svf_bp(bp, &s->s1, &s->s2, x, &lp);
        out[k] = x * 2.f;
    }
}

/* ================================================================ BAZZ: LXRXXL BAZZLINE's oscillator */
void bazz_render(BazzSt *s, const EngIn *in, float *out, int n)
{
    const float *p = in->p;
    const int mode = zone(p[0], 3);
    const float pos = p[1];
    const float oct = (float)(zone(p[2], 5) - 2) * 12.f;
    float note;
    uint32_t inc, incMod;
    int ti, k;
    const float pmScale = PH_ONE / TWO_PI_F;
    /* DRIFT: a slow random walk of the pitch, up to 30 cents */
    if (!s->rnd) s->rnd = 0x2F6B9A13u;
    s->driftT -= (float)n;
    if (s->driftT <= 0.f) { s->driftT = SR * (0.15f + 0.3f * frand(&s->rnd)); s->drift = brand(&s->rnd); }
    note = in->note + oct + p[4] * 0.3f * s->drift;
    {
        const float f = 440.f * exp2f((note - 69.f) / 12.f);
        inc = ph_inc(f);
        incMod = ph_inc(f * in->fmRatio);
    }
    ti = lxr_table(note);
    if (in->trig && !s->ph) s->phMod = 0;
    for (k = 0; k < n; k++) {
        uint32_t pm = s->ph;
        float o;
        if (in->fmIdx > 0.f) pm += (uint32_t)(int64_t)(rd_sine(s->phMod) * in->fmIdx * pmScale);
        if (mode == 0) o = lxr_wave(in->wave == 0 ? 0 : in->wave == 1 ? 1 : 2, ti, pm);
        else if (mode == 1) o = rd_sine(pm);
        else o = lxr_morph(pos, ti, pm);
        s->ph += inc;
        s->phMod += incMod;
        out[k] = o;
    }
}
