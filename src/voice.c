/* LXRXXL ZYNTH -- one track's voice: the voice engine picked with VOICE, then LXRXXL BAZZLINE's chain:
 *
 *   ENGINE (one of sixteen, eng.h; UNISON: a detuned second copy) -> FM LVL (a phase modulation of any tone)
 *     + SUB (-1 or -2 oct, SQR / SAW / TRI) + NOISE -> SWARM (BBD | unison) -> DRIVE (pre) -> FILTER (Polivoks 12 dB SVF |
 *     diode ladder, LP/BP, 2x oversampled) -> DRIVE (post) -> TONE (BAZZ) -> VCA -> level, pan
 *
 * with an amp envelope (VCA ATK, VCA ENV; fully down: held while the gate is, then RELEASE), a filter envelope (VCF ATK /
 * VCF DEC, amount VCF ENV), a pitch envelope (ENV/CV LVL, PITCH DEC), accent (AMOUNT, the 303-style SWEEP), slide
 * (SLIDE TIME) and a modulator (TRI SAW SQR S&H NOISE, free or on the tempo, to PITCH CUTOFF DRIVE or the voice's
 * WAVE: its main timbre slider).
 *
 * FM LVL on any voice: delaying a tone by d(t) = (I / 2 pi f) (1 - sin(2 pi r f t)) shifts every harmonic k by k I sin(.),
 * exactly a phase modulation of the tone's own phase at index I and ratio r; BAZZ does it in its oscillator, as in
 * LXRXXL BAZZLINE. The sub oscillator reads the LXR's band-limited tables (third_party/lxr02open wavetable.c). */
#include <math.h>
#include <string.h>
#include "bl.h"
#include "wavetable.h"

#define CR 8                 /* control rate: pitch, filter and envelope coefficients every 8 frames (0.18 ms) */
#define TWO_PI 6.283185307f

static const float FM_RATIOS[] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.5f, 4.f, 5.f, 6.f, 7.f, 8.f};
#define NUM_RATIOS ((int)(sizeof FM_RATIOS / sizeof FM_RATIOS[0]))
/* the modulator on the tempo: 4 bars .. 1/32 (in beats), and the display's names for them */
static const float LFO_BEATS[] = {16, 8, 4, 2, 1, 0.5f, 1.f / 3, 0.25f, 1.f / 6, 0.125f};
const char *const LFO_DIV_NAMES[] = {"4BR", "2BR", "1BR", "1/2", "1/4", "1/8", "8T", "16", "16T", "32"};
#define NUM_LFO_BEATS ((int)(sizeof LFO_BEATS / sizeof LFO_BEATS[0]))

float voice_fmRatio(float v) { int i = (int)lroundf(clampf(v, 0, 1) * (NUM_RATIOS - 1)); return FM_RATIOS[i]; }
float voice_lfoBeats(float v) { int i = (int)lroundf(clampf(v, 0, 1) * (NUM_LFO_BEATS - 1)); return LFO_BEATS[i]; }
float voice_lfoHz(float v) { return 0.05f * exp2f(clampf(v, 0, 1) * 9.6f); }
float voice_relSeconds(float v) { return 0.004f * powf(1000.f, clampf(v, 0, 1)); }   /* RELEASE: 4 ms .. 4 s */

/* ---------------------------------------------------------------- the LXR tables (the sub oscillator) */
#define PH_PER_CYCLE 4294967296.0

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

/* ---------------------------------------------------------------- shapes */
static inline float ftanh(float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

static inline float shape(int curve, float x, float g)
{
    x *= g;
    switch (curve) {
    case 1: return x > 1.f ? 1.f : x < -1.f ? -1.f : x;                         /* HARD */
    case 2: return sinf(x * 1.5707963f) * (x > 4.f || x < -4.f ? 0.9f : 1.f); /* FOLD */
    default: return ftanh(x);                                                   /* SOFT */
    }
}

static inline float coef(float seconds) { return 1.f - expf(-1.f / (seconds * SR / CR)); }   /* one-pole, per CR frames */

/* ---------------------------------------------------------------- notes */
void voice_init(Voice *v)
{
    void *m0 = v->eng[0].mi, *m1 = v->eng[1].mi;
    memset(v, 0, sizeof *v);
    v->eng[0].mi = m0;
    v->eng[1].mi = m1;
    v->eng[0].type = v->eng[1].type = 0xff;   /* set up at the first note */
    v->noise = 0x9E3779B9u;
    v->lfoRnd = 0x2545F491u;
    v->note = v->target = 48;
    eng_tables_init();
}

void voice_attach(Voice *v)
{
    if (!v->eng[0].mi) v->eng[0].mi = mi_new();
    if (!v->eng[1].mi) v->eng[1].mi = mi_new();
}

void voice_detach(Voice *v)
{
    mi_free(v->eng[0].mi);
    mi_free(v->eng[1].mi);
    v->eng[0].mi = v->eng[1].mi = NULL;
}

void voice_kill(Voice *v)
{
    const uint32_t n = v->noise, r = v->lfoRnd;
    voice_init(v);
    v->noise = n;
    v->lfoRnd = r;
}

void voice_noteOn(Voice *v, const Sound *s, float note, float vel, int accent, int slide, int penv, int mod)
{
    const int legato = slide && v->gate && v->aStage;
    v->target = note;
    v->vel = vel;
    v->modOn = (uint8_t)mod;
    if (legato) {
        /* a slide: no new envelopes, the pitch glides (the 303's and the DB-01's slide) */
        v->glide = coef(0.005f * powf(120.f, s->v[SP_SLIDE]));
        v->accent = accent ? 1.f : v->accent;
        return;
    }
    v->note = note;
    v->glide = 0;
    v->gate = 1;
    v->trig = 1;
    v->accent = accent ? 1.f : 0.f;
    if (accent) v->sweep = clampf(v->sweep + 0.25f + 0.5f * s->v[SP_ACC_SWEEP], 0, 1.5f);
    v->aStage = 1;
    v->fStage = 1;
    v->fenv = 0;
    v->menvStage = 1;
    v->menv = 0;
    v->vibAmt = 0;
    if (penv) v->penv = 1.f;
    if (!v->active) v->phSub = v->phPm = 0;
    v->active = 1;
    v->idle = 0;
    if (s->v[SP_MOD_WAVE] >= 3.f && s->v[SP_MOD_SYNC] >= 1.f) v->lfoPh = 0;   /* a tempo S&H / noise restarts with the note */
}

void voice_noteOff(Voice *v) { v->gate = 0; }

/* ---------------------------------------------------------------- the modulator */
static inline float lfo_step(Voice *v, int wave, double inc)
{
    double ph = v->lfoPh + inc;
    const int wrapped = ph >= 1.0;
    if (wrapped) ph -= floor(ph);
    v->lfoPh = ph;
    switch (wave) {
    case 0: return (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph);   /* TRI */
    case 1: return (float)(2 * ph - 1);                           /* SAW */
    case 2: return ph < 0.5 ? 1.f : -1.f;                         /* SQR */
    case 3:                                                       /* S&H */
        if (wrapped) v->lfoHold = (bl_rand(&v->lfoRnd) & 0xffff) / 32767.5f - 1.f;
        return v->lfoHold;
    default:                                                      /* NOISE: smooth random */
        if (wrapped) { v->lfoPrev = v->lfoNext; v->lfoNext = (bl_rand(&v->lfoRnd) & 0xffff) / 32767.5f - 1.f; }
        return v->lfoPrev + (float)ph * (v->lfoNext - v->lfoPrev);
    }
}

/* ---------------------------------------------------------------- render */
void voice_render(Voice *v, const Sound *s, float bpm, float *L, float *R, int n)
{
    int i, k;
    float gain, panL, panR;
    const int type = (int)clampf(s->v[SP_VOICE], 0, NUM_ENGINES - 1);
    if (!v->active) return;
    if (v->eng[0].type != type) { eng_reset(&v->eng[0], type); eng_reset(&v->eng[1], type); }
    {
        const float lv = s->v[SP_LEVEL];
        const float pan = clampf(s->v[SP_PAN], 0, 1);
        gain = 1.25f * lv * lv;
        panL = cosf(pan * 1.5707963f) * 1.41421356f;
        panR = sinf(pan * 1.5707963f) * 1.41421356f;
        if (panL > 1.f) panL = 1.f;
        if (panR > 1.f) panR = 1.f;
    }
    const int wave = (int)s->v[SP_WAVE];
    const int curve = (int)s->v[SP_CURVE];
    const int post = s->v[SP_DRIVE_POS] >= 1.f;
    const int diode = s->v[SP_FMODEL] >= 1.f;
    const int bp = s->v[SP_FMODE] < 1.f;
    const int unison = s->v[SP_SW_MODE] >= 1.f;
    const int modWave = (int)s->v[SP_MOD_WAVE], modDest = (int)s->v[SP_MOD_DEST];
    const float ratio = voice_fmRatio(s->v[SP_FM_RATIO]);
    const float fmIdx = s->v[SP_FM] * s->v[SP_FM] * 6.f;
    const float subLvl = s->v[SP_SUB], noiseLvl = s->v[SP_NOISE] * 0.8f;
    const float spread = s->v[SP_SPREAD];
    const float swMix = s->v[SP_SW_MIX] * clampf(spread * 4.f, 0, 1);   /* DETUNE at zero: the swarm is out */
    const float driveK = s->v[SP_DRIVE];
    const float accAmt = s->v[SP_ACC_AMT];
    const float lfoLvl = s->v[SP_LFO_LVL];
    const double lfoInc = (s->v[SP_MOD_SYNC] >= 1.f ? (bpm > 0 ? bpm : 120.f) / 60.f / voice_lfoBeats(s->v[SP_LFO_RATE])
                                                     : voice_lfoHz(s->v[SP_LFO_RATE])) / SR;
    /* envelope times */
    const float aAtk = coef(0.0005f * powf(4000.f, s->v[SP_VCA_ATK]));
    /* VCA ENV fully down: the VCA follows the gate (held), then RELEASE; else the note decays on its own */
    const float sus = clampf(s->v[SP_SUSTAIN], 0, 1);   /* ADSR: the level held while the key is */
    const float aDec = expf(-1.f / (0.015f * powf(400.f, s->v[SP_VCA_DEC]) * SR / CR));
    const float aRel = expf(-1.f / (voice_relSeconds(s->v[SP_VCA_REL]) * 0.25f * SR / CR));
    const float fAtk = coef(0.0005f * powf(4000.f, s->v[SP_VCF_ATK]));
    const float fDecT = 0.01f * powf(400.f, s->v[SP_VCF_DEC]);
    const float fDecN = expf(-1.f / (fDecT * SR / CR));
    const float fDecA = expf(-1.f / ((fDecT < 0.2f ? fDecT : 0.2f) * SR / CR));   /* accented: the 303's short decay */
    const float fSus = clampf(s->v[SP_VCF_SUS], 0, 1);
    const float fRel = expf(-1.f / (voice_relSeconds(s->v[SP_VCF_REL]) * 0.25f * SR / CR));
    /* the play row (V13..V20) */
    const float *pr = &s->v[SP_V1];
    const float octS = (float)(eng_step(type, PR_OCT, pr[PR_OCT]) - 2) * 12.f + (pr[PR_FINE] - 0.5f) * 2.f;
    const float vibD = pr[PR_VIB] * pr[PR_VIB] * 0.8f;
    const float mAtk = coef(0.001f + 2.f * pr[PR_ENV_ATK] * pr[PR_ENV_ATK]);
    const float mDec = expf(-1.f / ((0.01f + 5.f * pr[PR_ENV_DEC] * pr[PR_ENV_DEC]) * SR / CR));
    const float envA = (pr[PR_ENV_A] - 0.5f) * 2.f, envB = (pr[PR_ENV_B] - 0.5f) * 2.f, velA = (pr[PR_VEL] - 0.5f) * 2.f;
    const int tA = ENG_DEFS[type].target[0], tB = ENG_DEFS[type].target[1], tV = ENG_DEFS[type].target[2];
    const float pDec = expf(-1.f / (0.005f * powf(200.f, s->v[SP_PITCH_DEC]) * SR / CR));
    const float pAmt = 48.f * s->v[SP_PITCH_AMT] * s->v[SP_PITCH_AMT];
    const float sweepDec = expf(-1.f / (0.6f * SR / CR));
    const float envOct = s->v[SP_VCF_ENV] * 6.f;
    const float baseOct = log2f(30.f) + s->v[SP_CUTOFF] * 9.2f;
    const float keyTrack = s->v[SP_KEYTRACK];
    const float reso = s->v[SP_RESO];
    const float *sl = &s->v[SP_V1];
    const float toneV = eng_tone(type, sl);
    const int toneOn = toneV < 0.995f;
    const float toneA = 1.f - expf(-TWO_PI * 500.f * powf(40.f, toneV) / SR);
    const float subOct = eng_sub_octave(type, sl);
    const float bazzOct = type == ENG_BAZZ ? (float)(eng_step(type, 2, sl[2]) - 2) * 12.f : 0.f;
    const int pmOn = type != ENG_BAZZ && fmIdx > 0.f;
    const double swInc = 0.05 * pow(120.0, s->v[SP_SW_RATE]) / SR;
    const int timbre = ENG_DEFS[type].timbre;

    /* control-rate state */
    float freq = 0, g2 = 0, kRes = 0, drvG = 1, ampNow = 0;
    int tiSub = 0;
    float out[CR], o[CR], o2[CR];
    EngIn in;
    memset(&in, 0, sizeof in);

    for (i = 0; i < n; i += CR) {
        const int m = n - i < CR ? n - i : CR;
        /* ---- envelopes */
        if (v->aStage == 1) {
            v->aenv += aAtk * (1.25f - v->aenv);
            if (v->aenv >= 1.f) { v->aenv = 1.f; v->aStage = 2; }
            if (!v->gate) v->aStage = 2;   /* let go during the attack: the release from here */
        } else if (v->aStage == 2) {
            if (v->gate) v->aenv = sus + (v->aenv - sus) * aDec;   /* DECAY to SUSTAIN, held there */
            else v->aenv *= sus > 0.001f || aDec > aRel ? aRel : aDec;   /* RELEASE (no sustain: the faster) */
            if (v->aenv < 1e-5f) { v->aenv = 0; v->aStage = 0; }
        }
        if (v->fStage == 1) { v->fenv += fAtk * (1.25f - v->fenv); if (v->fenv >= 1.f) { v->fenv = 1.f; v->fStage = 2; } }
        else if (v->fStage == 2) {   /* the filter's ADSR: no sustain, it decays on (as 0.1.0); else held, then released */
            const float fd = v->accent > 0.5f ? fDecA : fDecN;
            if (v->gate) v->fenv = fSus + (v->fenv - fSus) * fd;
            else v->fenv *= fSus > 0.001f ? fRel : fd;
        }
        /* the modulation envelope (attack, decay) and the vibrato's fade-in */
        if (v->menvStage == 1) { v->menv += mAtk * (1.02f - v->menv); if (v->menv >= 1.f) { v->menv = 1.f; v->menvStage = 2; } }
        else if (v->menvStage == 2) v->menv *= mDec;
        v->vibAmt += (1.f - v->vibAmt) * (CR / (0.3f * SR));
        v->vibPh += 5.5f * CR / SR;
        if (v->vibPh >= 1.f) v->vibPh -= 1.f;
        v->penv *= pDec;
        v->sweep *= sweepDec;
        if (v->glide > 0) {
            v->note += v->glide * (v->target - v->note);
            if (fabsf(v->target - v->note) < 0.001f) { v->note = v->target; v->glide = 0; }
        } else v->note = v->target;
        /* ---- the modulator (one value per CR frames) */
        {
            const float lv = lfo_step(v, modWave, lfoInc * CR) * lfoLvl * (v->modOn ? 1.f : 0.f);
            float pitch = v->note + pAmt * v->penv + octS;
            float cut = baseOct + keyTrack * (v->note - 48.f) / 12.f
                        + envOct * v->fenv * (1.f + accAmt * v->accent)
                        + accAmt * (0.8f * v->accent + 1.2f * v->sweep);
            float drv = driveK;
            int j;
            for (j = 0; j < ENG_SLIDERS; j++) in.p[j] = sl[j];
            if (envA != 0.f && tA >= 0) in.p[tA] = clampf(in.p[tA] + envA * v->menv, 0, 1);
            if (envB != 0.f && tB >= 0) in.p[tB] = clampf(in.p[tB] + envB * v->menv, 0, 1);
            if (velA != 0.f && tV >= 0) in.p[tV] = clampf(in.p[tV] + velA * (v->vel - 0.5f), 0, 1);
            if (vibD > 0.f) pitch += vibD * v->vibAmt * sinf(TWO_PI * v->vibPh);
            switch (modDest) {
            case 0: pitch += 12.f * lv; break;
            case 1: cut += 4.f * lv; break;
            case 2: drv = clampf(drv + 0.5f * lv, 0, 1); break;
            default: if (timbre >= 0) in.p[timbre] = clampf(in.p[timbre] + 0.5f * lv, 0, 1); break;
            }
            freq = 440.f * exp2f((pitch - 69.f) / 12.f);
            if (freq > 18000.f) freq = 18000.f;
            {
                const float subNote = pitch + bazzOct - 12.f * (subOct > 1.5f ? 2.f : 1.f);
                tiSub = subNote < 0.f ? 0 : subNote >= 132.f ? 10 : (int)(subNote / 12.f);
            }
            in.note = pitch;
            in.freq = freq;
            {
                /* the filter: at 2x the rate. Polivoks: g = tan(pi fc / fs2); diode ladder: a one-pole per stage */
                float fc = exp2f(clampf(cut, 4.3f, 14.1f));
                if (fc > 16000.f) fc = 16000.f;
                if (!diode) {
                    g2 = tanf(3.14159265f * fc / (2.f * SR));
                    kRes = 2.f * (1.f - 0.985f * reso);
                } else {
                    g2 = 1.f - expf(-TWO_PI * fc / (2.f * SR));
                    kRes = reso * 4.3f * (fc > 6000.f ? 6000.f / fc * 0.5f + 0.5f : 1.f);
                }
            }
            drvG = 1.f + 30.f * drv * drv;
            ampNow = v->aenv * (0.75f + 0.25f * v->vel) * (1.f + accAmt * v->accent * 0.9f);
            in.vel = v->vel;
            in.accent = v->accent;
            in.env = v->aenv;
            in.fmIdx = fmIdx;
            in.fmRatio = ratio;
            in.bpm = bpm;
            in.gate = v->gate;
            in.trig = v->trig;
            in.wave = (uint8_t)wave;
            v->trig = 0;
        }
        /* ---- the engine (and its UNISON copy) */
        eng_render(&v->eng[0], &in, o, m);
        if (unison && swMix > 0.f) {
            EngIn u = in;
            u.note = in.note + spread * 0.25f;
            u.freq = in.freq * exp2f(spread * 0.25f / 12.f);
            eng_render(&v->eng[1], &u, o2, m);
            for (k = 0; k < m; k++) o[k] = o[k] * (1.f - 0.35f * swMix) + o2[k] * 0.6f * swMix;
        }
        /* ---- audio */
        {
            const uint32_t incSub = (uint32_t)(freq * exp2f(bazzOct / 12.f) / subOct / SR * PH_PER_CYCLE);
            const uint32_t incPm = (uint32_t)(freq * ratio / SR * PH_PER_CYCLE);
            float pmD = 0.f;
            float swD[2] = {0, 0};
            const float polG = 1.f / (1.f + kRes * g2 + g2 * g2);
            if (pmOn) {   /* the delay's swing, in frames: I / (2 pi f), kept inside the line */
                pmD = fmIdx / (TWO_PI * (freq > 1.f ? freq : 1.f)) * SR;
                if (pmD > PM_LEN / 2 - 4) pmD = PM_LEN / 2 - 4;
            }
            if (!unison && swMix > 0.f) {
                int t;
                for (t = 0; t < 2; t++) {   /* the BBD's modulated delay times, per CR frames */
                    double sp = v->swPh[t] + swInc * CR * (t ? 1.13 : 0.87);
                    sp -= floor(sp);
                    v->swPh[t] = sp;
                    swD[t] = (0.006f + 0.0025f * t + spread * 0.006f * (0.5f + 0.5f * sinf(TWO_PI * (float)sp))) * SR;
                }
            }
            for (k = 0; k < m; k++) {
                float x = o[k];
                if (pmOn) {
                    const float d = 1.f + pmD * (1.f - rd_sine(v->phPm));
                    const int di = (int)d;
                    const float fr = d - (float)di;
                    const int r0 = (v->pmW - di) & (PM_LEN - 1), r1 = (r0 - 1) & (PM_LEN - 1);
                    v->pm[v->pmW] = x;
                    v->pmW = (v->pmW + 1) & (PM_LEN - 1);
                    x = v->pm[r0] + fr * (v->pm[r1] - v->pm[r0]);
                    v->phPm += incPm;
                }
                if (subLvl > 0.f) {
                    const int sw = type == ENG_BAZZ ? 0 : wave;   /* BAZZ: the DB-01's square sub */
                    x += subLvl * rd1024(sw == 0 ? recTable[tiSub] : sw == 1 ? sawTable[tiSub] : triTable[tiSub], v->phSub);
                }
                if (noiseLvl > 0.f) {
                    v->noise ^= v->noise << 13; v->noise ^= v->noise >> 17; v->noise ^= v->noise << 5;
                    x += noiseLvl * ((int32_t)v->noise * (1.f / 2147483648.f));
                }
                v->phSub += incSub;
                /* the BBD swarm: two modulated taps of a short delay, darkened */
                if (!unison && swMix > 0.f) {
                    int t;
                    float wet = 0;
                    v->bbd[v->bbdW] = x;
                    for (t = 0; t < 2; t++) {
                        const int di = (int)swD[t];
                        const float fr = swD[t] - di;
                        const int r0 = (v->bbdW - di) & (BBD_LEN - 1), r1 = (r0 - 1) & (BBD_LEN - 1);
                        wet += v->bbd[r0] + fr * (v->bbd[r1] - v->bbd[r0]);
                    }
                    v->bbdW = (v->bbdW + 1) & (BBD_LEN - 1);
                    v->bbdLp += 0.55f * (0.5f * wet - v->bbdLp);
                    x = x * (1.f - 0.45f * swMix) + ftanh(v->bbdLp * 1.2f) * 0.9f * swMix;
                }
                /* drive (pre) */
                if (!post) x = shape(curve, x, drvG) * (curve == 0 ? 1.f : 0.9f);
                /* the filter, twice per frame */
                {
                    int os;
                    float y = 0, xin[2];
                    xin[0] = 0.5f * (v->dcX + x);
                    xin[1] = x;
                    v->dcX = x;
                    for (os = 0; os < 2; os++) {
                        float lp, bpo;
                        if (!diode) {
                            const float hp = (xin[os] - (kRes + g2) * v->s1 - v->s2) * polG;
                            const float b = g2 * hp + v->s1;
                            v->s1 = g2 * hp + b;
                            lp = g2 * b + v->s2;
                            v->s2 = g2 * b + lp;
                            v->s1 = v->s1 / (1.f + 0.08f * fabsf(v->s1));   /* the Polivoks's integrators clip: its grit */
                            bpo = b;
                        } else {
                            const float u = ftanh(1.4f * (xin[os] - kRes * v->d4));
                            v->d1 += g2 * (u - v->d1);
                            v->d2 += g2 * (v->d1 - v->d2);
                            v->d3 += g2 * (v->d2 - v->d3);
                            v->d4 += g2 * (v->d3 - v->d4);
                            lp = v->d4 * (1.f + 0.3f * kRes) * 0.75f;
                            bpo = (v->d2 - v->d4) * (1.2f + 0.2f * kRes);
                        }
                        y += bp ? bpo : lp;
                    }
                    x = y * 0.5f;
                }
                if (post) x = shape(curve, x, drvG) * (curve == 0 ? 1.f : 0.9f);
                if (toneOn) { v->toneZ += toneA * (x - v->toneZ); x = v->toneZ; }
                out[k] = x * ampNow;
            }
        }
        for (k = 0; k < m; k++) {
            L[i + k] += out[k] * gain * panL;
            R[i + k] += out[k] * gain * panR;
        }
    }
    /* asleep once silent (the swarm's short tail included) */
    if (v->aStage == 0) {
        v->idle += (uint32_t)n;
        if (v->idle > BBD_LEN * 2) {
            v->active = 0;
            v->s1 = v->s2 = v->d1 = v->d2 = v->d3 = v->d4 = 0;
            v->eng[0].type = v->eng[1].type = 0xff;   /* the next note starts the engine from rest */
        }
    } else v->idle = 0;
}
