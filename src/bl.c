/* LXRXXL ZYNTH -- the instance: setup, the factory project and the audio block. */
#include <math.h>
#include <string.h>
#include "bl.h"
#include "param_map.h"

float bl_tempo(Bl *x)
{
    if (x->p.bpm > 0.f) return x->p.bpm;
    return x->seq.hostTempo > 0 ? (float)x->seq.hostTempo : 120.f;
}

int bl_trackSrc(Bl *x, int t)
{
    while (t > 0 && (x->p.links & (1u << t))) t--;   /* linked: the steps of the track before it */
    return t;
}

const TrackPat *bl_trackPat(Bl *x, int t)
{
    return &bl_pat(x)->t[bl_trackSrc(x, t)];
}

void bl_initProject(Project *p)
{
    int i, t, k;
    memset(p, 0, sizeof *p);
    for (t = 0; t < NUM_TRACKS; t++) init_sound(&p->snd[t]);
    for (i = 0; i < NUM_PATTERNS; i++)
        for (t = 0; t < NUM_TRACKS; t++) {
            TrackPat *tp = &p->pats[i].t[t];
            tp->len = 16;
            for (k = 0; k < NUM_STEPS; k++) tp->s[k].note = 36;
        }
    for (i = 0; i < G_COUNT; i++) p->g[i] = GLOBAL_DEFS[i].def;
    p->bpm = 0;
    p->gdOn = 0;
    for (t = 0; t < NUM_TRACKS; t++) p->send[t] = 1.f;
    for (t = 0; t < NUM_TRACKS; t++) { p->chaosAmt[t] = 1.f; p->shiftSend[t] = 1.f; }
    for (t = 0; t < NUM_TRACKS; t++) { p->rset[t].lenOpt = 3; p->rset[t].vel = RV_GROOVE; p->rset[t].style = RS_AUTO; }
    p->master = 0.8f;
}

void bl_init(Bl *x)
{
    int t;
    float *b0 = x->fx.buf[0], *b1 = x->fx.buf[1];
    void *mi[NUM_TRACKS][2];
    char dir[512];
    for (t = 0; t < NUM_TRACKS; t++) { mi[t][0] = x->v[t].eng[0].mi; mi[t][1] = x->v[t].eng[1].mi; }
    memcpy(dir, x->dataDir, sizeof dir);
    memset(x, 0, sizeof *x);
    memcpy(x->dataDir, dir, sizeof dir);
    x->fx.buf[0] = b0;
    x->fx.buf[1] = b1;
    for (t = 0; t < NUM_TRACKS; t++) { x->v[t].eng[0].mi = mi[t][0]; x->v[t].eng[1].mi = mi[t][1]; }
    for (t = 0; t < NUM_TRACKS; t++) voice_init(&x->v[t]);
    fx_init(&x->fx);
    alien_init(&x->al);
    x->nextPat = 0xff;
    x->seq.rng = 0x51ED270Bu;
    x->rnd = 0x7F4A7C15u;
    x->seq.hostTempo = 120;
    x->touched = -1;
    x->rnd2.amt = x->rnd2.seqAmt = 5;
    bl_factory(x);
    ui_init(x);
    preset_init(x);
}

/* ---------------------------------------------------------------- audio */
static inline float soft(float v)
{
    if (v > 3.f) return 1.f;
    if (v < -3.f) return -1.f;
    return v * (27.f + v * v) / (27.f + 9.f * v * v);
}

/* a track is heard unless muted, or while any track is soloed, unless soloed */
static int audible(Bl *x, int t)
{
    if (x->p.mutes & (1u << t)) return 0;
    return !x->p.solos || (x->p.solos & (1u << t));
}

void bl_render(Bl *x, float *L, float *R, int n)
{
    int done = 0, t, i, guard = 0;
    float sL[128], sR[128], vL[128], vR[128], hL[128], hR[128];
    const int shiftOn = x->p.g[G_SHIFT_MIX] > 0.0005f;
    if (n > 128) n = 128;
    alien_chaos(x, n);
    if (shiftOn) { memset(hL, 0, sizeof hL); memset(hR, 0, sizeof hR); }
    memset(L, 0, sizeof(float) * (size_t)n);
    memset(R, 0, sizeof(float) * (size_t)n);
    memset(sL, 0, sizeof sL);
    memset(sR, 0, sizeof sR);
    while (done < n && guard++ < 4096) {
        const double d = seq_nextEventIn(x);
        int chunk = n - done;
        if (d < chunk) chunk = (int)ceil(d - 1e-9);
        if (chunk > 0) {
            const float bpm = bl_tempo(x);
            for (t = 0; t < NUM_TRACKS; t++) {
                if (!x->v[t].active) continue;
                memset(vL, 0, sizeof(float) * (size_t)chunk);
                memset(vR, 0, sizeof(float) * (size_t)chunk);
                voice_render(&x->v[t], &x->p.snd[t], bpm, vL, vR, chunk);
                if (!audible(x, t)) continue;
                {
                    const float snd = x->p.send[t];
                    for (i = 0; i < chunk; i++) {
                        L[done + i] += vL[i];
                        R[done + i] += vR[i];
                        sL[done + i] += vL[i] * snd;
                        sR[done + i] += vR[i] * snd;
                    }
                    if (shiftOn) {
                        const float hs = x->p.shiftSend[t];
                        for (i = 0; i < chunk; i++) { hL[done + i] += vL[i] * hs; hR[done + i] += vR[i] * hs; }
                    }
                }
            }
        }
        seq_advance(x, chunk, done);
        done += chunk;
    }
    fx_render(x, L, R, sL, sR, n);
    if (shiftOn) alien_shift(x, hL, hR, L, R, n);
    {
        const float m = x->p.master * 1.25f;
        for (t = 0; t < n; t++) { L[t] = soft(L[t] * 0.8f * m); R[t] = soft(R[t] * 0.8f * m); }
    }
    x->clock += (uint32_t)n;
}
