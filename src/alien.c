/* LXRXXL ZYNTH -- CHAOS and SHIFT (0.2.0): what makes it sound from somewhere else.
 *
 * CHAOS: one Lorenz attractor (dx = 10 (y - x), dy = x (28 - z) - y, dz = x y - 8/3 z), the "butterfly", read by each
 * track from its own angle, so the eight tracks never move together. WILD blends it with a calm orbit (two slow
 * sines): at the left the motion is almost regular, at the right it never repeats. RATE is how fast it runs. Each
 * track's value is CHAOS (the front's depth) x its MIXER amount, sent to PITCH, CUTOFF, TIMBRE, PAN or ALL.
 *
 * SHIFT: a send bus (each track's SHIFT SEND on the MIXER) through a frequency shifter: a Hilbert transformer (two
 * chains of four first-order all-passes, Olli Niemitalo's coefficients) gives the signal and its 90-degree twin,
 * multiplied with a quadrature oscillator: every partial moves by the same number of Hz (up or down), so harmonic
 * sounds turn inharmonic. RING multiplies with the oscillator alone (ring modulation). FEEDBACK sends the result back
 * in: the endless barber-pole rise or fall. SHIFT MIX is the bus's level back into the mix. */
#include <math.h>
#include <string.h>
#include "bl.h"

static const float HA[4] = {0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f};
static const float HB[4] = {0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f};

void alien_init(Alien *a)
{
    memset(a, 0, sizeof *a);
    a->lx = 0.1; a->ly = 0.0; a->lz = 20.0;
}

static inline float clamp1(float v) { return v < -1.f ? -1.f : v > 1.f ? 1.f : v; }

void alien_chaos(Bl *x, int n)
{
    Alien *a = &x->al;
    const float *g = x->p.g;
    const float depth = g[G_CHAOS_DEPTH];
    const int dest = (int)(g[G_CHAOS_DEST] + 0.5f);
    const double tps = 0.02 * pow(2.0, g[G_CHAOS_RATE] * 8.0);   /* attractor time per second: 0.02 .. 5 */
    const float wild = g[G_CHAOS_WILD];
    double left = tps * n / 44100.0;
    int t;
    /* the attractor: Euler steps of at most 0.004 */
    while (left > 1e-9) {
        const double h = left > 0.004 ? 0.004 : left;
        const double dx = 10.0 * (a->ly - a->lx), dy = a->lx * (28.0 - a->lz) - a->ly, dz = a->lx * a->ly - 8.0 / 3.0 * a->lz;
        a->lx += h * dx; a->ly += h * dy; a->lz += h * dz;
        left -= h;
    }
    if (!(a->lx == a->lx) || fabs(a->lx) > 1e3) { a->lx = 0.1; a->ly = 0; a->lz = 20; }   /* never: but never stuck */
    a->ph += tps * 0.5 * n / 44100.0;
    if (a->ph > 1e6) a->ph -= 1e6;
    {
        const float xn = (float)(a->lx / 20.0), yn = (float)(a->ly / 27.0), zn = (float)((a->lz - 25.0) / 20.0);
        const float k = 1.f - expf(-(float)n / (44100.f * 0.03f));   /* 30 ms smoothing */
        for (t = 0; t < NUM_TRACKS; t++) {
            const float an = 6.2831853f * (float)t / NUM_TRACKS;
            const float lor = clamp1(cosf(an) * xn + sinf(an) * yn * 0.8f + (t & 1 ? -0.35f : 0.35f) * zn);
            const float calm = 0.7f * sinf((float)(6.2831853 * a->ph * (1.0 + 0.13 * t)) + an)
                               + 0.3f * sinf((float)(6.2831853 * a->ph * 0.37) + 2.f * an);
            const float v = (1.f - wild) * calm + wild * lor;
            a->sm[t] += k * (v - a->sm[t]);
            x->v[t].chaos = a->sm[t] * depth * x->p.chaosAmt[t];
            x->v[t].chaosDest = (uint8_t)dest;
        }
    }
}

/* the Hilbert pair of one channel: in -> (i, q), q 90 degrees behind */
static inline void hilbert(Alien *a, int c, float in, float *i, float *q)
{
    int s;
    float xa = in, xb = in;
    for (s = 0; s < 4; s++) {
        const float a2 = HA[s] * HA[s], b2 = HB[s] * HB[s];
        float *xs = a->ap[c][0][s], *ys = a->apy[c][0][s];
        const float ya = a2 * (xa + ys[1]) - xs[1];
        xs[1] = xs[0]; xs[0] = xa; ys[1] = ys[0]; ys[0] = ya; xa = ya;
        xs = a->ap[c][1][s]; ys = a->apy[c][1][s];
        {
            const float yb = b2 * (xb + ys[1]) - xs[1];
            xs[1] = xs[0]; xs[0] = xb; ys[1] = ys[0]; ys[0] = yb; xb = yb;
        }
    }
    *i = a->qd[c];   /* chain A delayed by one sample (Niemitalo's arrangement) */
    a->qd[c] = xa;
    *q = xb;
}

/* SHIFT FREQ: the centre 0, down to the left, up to the right, 0 .. +-2047 Hz (fine near the centre: barber pole) */
float alien_shiftHz(float v)
{
    const float d = (v - 0.5f) * 2.f;
    return fabsf(d) < 0.01f ? 0.f : (d < 0 ? -1.f : 1.f) * (exp2f(fabsf(d) * 11.f) - 1.f);
}

static inline float softc(float v) { return v / (1.f + fabsf(v) * 0.5f); }

void alien_shift(Bl *x, const float *inL, const float *inR, float *L, float *R, int n)
{
    Alien *a = &x->al;
    const float *g = x->p.g;
    const float mix = g[G_SHIFT_MIX];
    const float fbk = g[G_SHIFT_FB] * 0.92f;
    const int ring = g[G_SHIFT_MODE] >= 1.f;
    const float hz = alien_shiftHz(g[G_SHIFT_FREQ]);
    const double inc = hz / 44100.0;
    int k;
    if (mix <= 0.0005f) { a->fb[0] = a->fb[1] = 0; return; }
    for (k = 0; k < n; k++) {
        const float ph = (float)(6.283185307179586 * a->sph);
        const float co = cosf(ph), si = sinf(ph);
        int c;
        float o[2];
        a->sph += inc;
        if (a->sph >= 1.0) a->sph -= 1.0;
        if (a->sph < 0.0) a->sph += 1.0;
        for (c = 0; c < 2; c++) {
            const float in = (c ? inR[k] : inL[k]) + fbk * a->fb[c];
            if (ring) o[c] = in * co;
            else {
                float i, q;
                hilbert(a, c, in, &i, &q);
                o[c] = i * co + q * si;   /* up (down with a negative frequency): q lags i by 90 degrees */
            }
            a->fb[c] = softc(o[c]);
        }
        L[k] += o[0] * mix;
        R[k] += o[1] * mix;
    }
}
