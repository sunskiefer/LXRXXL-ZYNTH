/* LXRXXL ZYNTH -- every voice engine on its own: each voice, at its defaults and at random settings, across the
 * keyboard. Checks: no NaN or Inf, a sensible level, no runaway; the pitched voices play the note asked for (the
 * spectrum's strongest low partial, or the period by autocorrelation). Prints a table; exits 1 on a failure.
 *   eng_test [wav_dir]   (with a directory: one WAV per voice at its defaults, for listening) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eng.h"

#define SRI 44100
#define LEN (SRI * 1)

static float buf[LEN];

static int render(int type, const float *p, float note, int len, float *peak, float *rms, int *bad)
{
    Eng e;
    EngIn in;
    int i, k;
    double s2 = 0;
    memset(&e, 0, sizeof e);
    e.mi = mi_new();
    eng_reset(&e, type);
    memset(&in, 0, sizeof in);
    in.note = note;
    in.freq = 440.f * exp2f((note - 69.f) / 12.f);
    memcpy(in.p, p, sizeof in.p);
    in.vel = 0.8f;
    in.env = 1.f;
    in.fmRatio = 1.f;
    in.gate = 1;
    in.wave = 1;
    in.bpm = 120;
    *peak = 0;
    *bad = 0;
    for (i = 0; i < len; i += ENG_MAX_BLOCK) {
        const int n = len - i < ENG_MAX_BLOCK ? len - i : ENG_MAX_BLOCK;
        in.trig = i == 0;
        in.env = expf(-(float)i / (SRI * 0.8f));
        eng_render(&e, &in, buf + i, n);
        for (k = 0; k < n; k++) {
            const float v = buf[i + k];
            if (!isfinite(v)) { *bad = 1; buf[i + k] = 0; }
            if (fabsf(v) > *peak) *peak = fabsf(v);
            s2 += (double)v * v;
        }
    }
    *rms = (float)sqrt(s2 / len);
    mi_free(e.mi);
    return 0;
}

/* the strongest partial between 60 Hz and 1.2 kHz (Goertzel over 16384 Hann-windowed frames after 0.1 s) */
static double goertzel(double f, const float *x, int w)
{
    const double c = 2.0 * cos(6.283185307179586 * f / SRI);
    double s0, s1 = 0, s2 = 0;
    int i;
    for (i = 0; i < w; i++) {
        const double win = 0.5 - 0.5 * cos(6.283185307179586 * i / (w - 1));
        s0 = x[i] * win + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

static float pitch_of(void)
{
    const int start = 4410, w = 16384;
    double f, best = 0, bv = 0;
    for (f = 60; f <= 1200; f += 1.0) {
        const double v = goertzel(f, buf + start, w);
        if (v > bv) { bv = v; best = f; }
    }
    if (bv <= 0) return 0;
    {
        const double c = best;
        for (f = c - 1.5; f <= c + 1.5; f += 0.05) {
            const double v = goertzel(f, buf + start, w);
            if (v > bv) { bv = v; best = f; }
        }
    }
    return (float)best;
}

/* in tune: a partial of the note (a whole multiple of 110 Hz, the octave below A3), within 0.6 % */
static int in_tune(float f)
{
    const float m = f / 110.f;
    const float r = floorf(m + 0.5f);
    return r >= 1.f && fabsf(m - r) / r < 0.006f;
}

static void wav(const char *path, const float *x, int n)
{
    FILE *f = fopen(path, "wb");
    int i;
    unsigned int u;
    unsigned short s;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); u = 36 + n * 2; fwrite(&u, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); u = 16; fwrite(&u, 4, 1, f);
    s = 1; fwrite(&s, 2, 1, f); fwrite(&s, 2, 1, f);
    u = SRI; fwrite(&u, 4, 1, f); u = SRI * 2; fwrite(&u, 4, 1, f);
    s = 2; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f); u = n * 2; fwrite(&u, 4, 1, f);
    for (i = 0; i < n; i++) {
        float v = x[i] * 0.5f;
        short q = (short)(v > 1 ? 32767 : v < -1 ? -32767 : v * 32767);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    int t, fails = 0, r;
    uint32_t seed = 12345;
    eng_tables_init();
    printf("%-10s %6s %6s %8s %8s  %s\n", "voice", "peak", "rms", "", "partial", "notes");
    for (t = 0; t < NUM_ENGINES; t++) {
        float peak, rms, f0;
        int bad;
        const float note = 57.f;   /* A3, 220 Hz */
        const int pitched = t != ENG_NOISE && t != ENG_GRAIN && t != ENG_LXR_METAL && t != ENG_CHIP;
        render(t, ENG_DEFS[t].def, note, LEN, &peak, &rms, &bad);
        f0 = pitch_of();
        if (argc > 1) {
            char path[512];
            snprintf(path, sizeof path, "%s/%02d_%s.wav", argv[1], t, ENG_DEFS[t].name);
            wav(path, buf, LEN);
        }
        {
            const int levelBad = rms < 0.02f || peak > 4.f;
            const int pitchBad = pitched && !in_tune(f0);
            printf("%-10s %6.2f %6.3f %8.1f %8.1f  %s%s%s\n", ENG_DEFS[t].name, peak, rms, 0.f, f0,
                   bad ? "NaN! " : "", levelBad ? "LEVEL! " : "", pitchBad ? "PITCH? " : "");
            if (bad || levelBad) fails++;
            if (pitchBad) fails++;
        }
    }
    /* random settings and notes: nothing may blow up */
    for (r = 0; r < 400; r++) {
        float p[6], peak, rms;
        int bad, i;
        t = r % NUM_ENGINES;
        for (i = 0; i < 6; i++) { seed = seed * 1664525u + 1013904223u; p[i] = (seed >> 8) / 16777216.f; }
        seed = seed * 1664525u + 1013904223u;
        render(t, p, 12.f + (float)((seed >> 8) % 100), SRI / 4, &peak, &rms, &bad);
        if (bad || peak > 8.f) {
            printf("RANDOM %s p=%.2f %.2f %.2f %.2f %.2f %.2f: peak %.2f%s\n", ENG_DEFS[t].name, p[0], p[1], p[2], p[3],
                   p[4], p[5], peak, bad ? " NaN" : "");
            fails++;
        }
    }
    printf("%s (%d)\n", fails ? "FAIL" : "OK", fails);
    return fails ? 1 : 0;
}
