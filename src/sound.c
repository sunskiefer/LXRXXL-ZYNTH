/* LXRXXL ZYNTH -- a track's sound: the voice it plays and everything around it.
 *
 * Each track remembers a whole sound per voice: the voice's six sliders AND the chain (filter, drive, envelopes, sub,
 * noise, FM, LFO, swarm, accent...). Switching FM6 -> GRAIN -> FM6 finds the FM6 sound as it was left; a voice not yet
 * touched on a track starts from its HOME sound: the chain set so the voice sounds as itself (the sustained voices
 * held by the gate, the plucked and struck ones left to ring, BAZZ as BAZZLINE's bass). LEVEL and PAN stay the track's
 * (the mixer's), whatever the voice. VOICE RESET puts the voice in use back to its home sound.
 *
 * v[] always holds the sound in use (what the panel, MPC and the presets see); full[voice] keeps each voice's. */
#include <string.h>
#include "bl.h"
#include "param_map.h"

typedef struct { uint8_t sp; float v; } Set;
#define END {255, 0}

/* what every home sound starts from: a clean chain (the voice as it comes out of its engine) */
static const Set CLEAN[] = {
    {SP_DRIVE, 0}, {SP_SPREAD, 0}, {SP_VCA_DEC, 0}, {SP_VCF_ENV, 0}, {SP_RESO, 0}, {SP_CUTOFF, 1.f},
    {SP_FM, 0}, {SP_SUB, 0}, {SP_NOISE, 0}, {SP_LFO_RATE, .4f}, {SP_LFO_LVL, 0}, {SP_PITCH_AMT, 0},
    {SP_WAVE, 1}, {SP_FMODE, 1}, {SP_FM_RATIO, .25f}, {SP_DRIVE_POS, 0}, {SP_CURVE, 0},
    {SP_ACC_AMT, .3f}, {SP_ACC_SWEEP, 0}, {SP_SLIDE, .3f}, {SP_VCF_ATK, 0}, {SP_VCF_DEC, .45f}, {SP_VCA_ATK, 0},
    {SP_PITCH_DEC, .3f}, {SP_FMODEL, 0}, {SP_KEYTRACK, .5f}, {SP_SW_MODE, 0}, {SP_SW_RATE, .3f}, {SP_SW_MIX, .6f},
    {SP_MOD_WAVE, 0}, {SP_MOD_DEST, 1}, {SP_MOD_SYNC, 0}, {SP_VCA_REL, .3f}, {SP_SUSTAIN, 1.f}, END
};

/* and what each voice's home changes from it (SUSTAIN 1: held while the key is; SUSTAIN 0: rings and decays over DECAY) */
static const Set HOME[NUM_ENGINES][13] = {
    [ENG_ANALOG]    = {{SP_CUTOFF, .72f}, {SP_VCF_ENV, .3f}, {SP_RESO, .2f}, {SP_VCA_ATK, .05f}, {SP_VCA_REL, .35f}, END},
    [ENG_TRIO]      = {{SP_CUTOFF, .8f}, {SP_VCA_ATK, .3f}, {SP_VCA_REL, .5f}, {SP_LFO_LVL, .08f}, {SP_MOD_DEST, 0}, END},
    [ENG_FM6]       = {{SP_VCA_REL, .45f}, END},
    [ENG_FM4]       = {{SP_VCA_REL, .35f}, END},
    [ENG_PHASE]     = {{SP_CUTOFF, .9f}, {SP_VCA_REL, .35f}, END},
    [ENG_WAVE]      = {{SP_VCA_ATK, .25f}, {SP_VCA_REL, .55f}, {SP_CUTOFF, .85f}, END},
    [ENG_CHIP]      = {{SP_VCA_REL, .12f}, END},
    [ENG_MACRO]     = {{SP_VCA_REL, .3f}, END},
    [ENG_WHEEL]     = {{SP_VCA_REL, .18f}, END},
    [ENG_VOICE]     = {{SP_VCA_ATK, .15f}, {SP_VCA_REL, .4f}, END},
    [ENG_PHYS]      = {{SP_VCA_REL, .6f}, END},
    [ENG_GRAIN]     = {{SP_VCA_ATK, .35f}, {SP_VCA_REL, .65f}, END},
    [ENG_NOISE]     = {{SP_CUTOFF, .75f}, {SP_RESO, .45f}, {SP_VCA_REL, .3f}, {SP_FMODE, 0}, END},
    [ENG_LXR_TONE]  = {{SP_VCA_REL, .3f}, END},
    [ENG_LXR_METAL] = {{SP_VCA_DEC, .7f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .5f}, END},
    /* BAZZLINE's bass as BAZZLINE starts it */
    [ENG_BAZZ]      = {{SP_DRIVE, .15f}, {SP_VCA_DEC, .45f}, {SP_SUSTAIN, 0}, {SP_VCF_ENV, .5f}, {SP_RESO, .55f}, {SP_CUTOFF, .35f},
                       {SP_ACC_AMT, .5f}, {SP_ACC_SWEEP, .35f}, {SP_KEYTRACK, .5f}, {SP_VCA_REL, .2f}, END},
};

static int keep(int sp) { return sp == SP_VOICE || sp == SP_LEVEL || sp == SP_PAN; }

static int cur(const Sound *s)
{
    int t = (int)(s->v[SP_VOICE] + 0.5f);
    return t < 0 ? 0 : t >= NUM_ENGINES ? NUM_ENGINES - 1 : t;
}

void sound_home(int type, float *v)
{
    int i;
    for (i = 0; i < SP_COUNT; i++) v[i] = SOUND_DEFS[i].def;
    for (i = 0; CLEAN[i].sp != 255; i++) v[CLEAN[i].sp] = CLEAN[i].v;
    for (i = 0; i < 13 && HOME[type][i].sp != 255; i++) v[HOME[type][i].sp] = HOME[type][i].v;
    for (i = 0; i < ENG_SLIDERS; i++) v[SP_V1 + i] = ENG_DEFS[type].def[i];
    v[SP_VOICE] = (float)type;
}

static void load(Sound *s, int type)
{
    int k;
    for (k = 0; k < SP_COUNT; k++) if (!keep(k)) s->v[k] = s->full[type][k];
    s->v[SP_VOICE] = (float)type;
}

void sound_setVoice(Sound *s, int type)
{
    if (type < 0) type = 0;
    if (type >= NUM_ENGINES) type = NUM_ENGINES - 1;
    sound_sync(s);
    load(s, type);
}

void sound_setSlider(Sound *s, int k, float v)
{
    if (k < 0 || k >= ENG_SLIDERS) return;
    v = clampf(v, 0, 1);
    s->v[SP_V1 + k] = v;
    s->full[cur(s)][SP_V1 + k] = v;
}

void sound_resetVoice(Sound *s, int type)
{
    sound_home(type, s->full[type]);
    if (cur(s) == type) load(s, type);
}

/* v[] written back into the voice in use's memory (after presets, the randomizer, a knob) */
void sound_sync(Sound *s)
{
    memcpy(s->full[cur(s)], s->v, sizeof s->v);
}

void init_sound(Sound *s)
{
    int e;
    for (e = 0; e < NUM_ENGINES; e++) sound_home(e, s->full[e]);
    memcpy(s->v, s->full[ENG_ANALOG], sizeof s->v);
    s->v[SP_LEVEL] = SOUND_DEFS[SP_LEVEL].def;
    s->v[SP_PAN] = SOUND_DEFS[SP_PAN].def;
}
