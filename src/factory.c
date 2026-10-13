/* LXRXXL ZYNTH -- the factory project: a different voice on each track and six demo patterns (A1-A6).
 *   1 ACID (BAZZ, diode ladder, slides)     5 ORGAN (WHEEL, slow LESLIE)
 *   2 E.PIANO (FM6, three pairs)            6 CLOUD (GRAIN, slow attack)
 *   3 PAD (WAVE, swarm, slow modulation)    7 ARP (CHIP, a major arpeggio)
 *   4 PLUCK (PHYS, a string)                8 BELL (LXR METAL, bell character)
 * A1 everything together, A2 the pad with plucks and the cloud, A3 the organ with the acid, A4 the plucks and the
 * piano, A5 the chip and the bells, A6 the cloud and the pad alone. */
#include <string.h>
#include "bl.h"

typedef struct { uint8_t sp; float v; } Set;

#define END {255, 0}
#define VOICE(t) {SP_VOICE, (float)(t)}
static const Set SOUNDS[NUM_TRACKS][28] = {
    /* 1 ACID: LXRXXL BAZZLINE's acid line, on its own voice */
    {VOICE(ENG_BAZZ), {SP_V1, .5f / 3}, {SP_V2, 0.f}, {SP_V3, 2.5f / 5}, {SP_V4, .25f}, {SP_V5, .1f}, {SP_V6, .8f},
     {SP_WAVE, 1}, {SP_FMODEL, 1}, {SP_FMODE, 1}, {SP_CUTOFF, .28f}, {SP_RESO, .8f}, {SP_VCF_ENV, .55f},
     {SP_VCF_DEC, .36f}, {SP_VCA_DEC, .55f}, {SP_SUSTAIN, 0}, {SP_ACC_AMT, .7f}, {SP_ACC_SWEEP, .55f}, {SP_SLIDE, .33f},
     {SP_DRIVE, .3f}, {SP_KEYTRACK, .3f}, {SP_LEVEL, .72f}, END},
    /* 2 E.PIANO: FM6, algorithm 5 (three pairs), the modulators fading */
    {VOICE(ENG_FM6), {SP_V1, 4.5f / 32}, {SP_V2, 3.5f / 16}, {SP_V3, .32f}, {SP_V4, .1f}, {SP_V5, .25f}, {SP_V6, .2f},
     {SP_CUTOFF, .85f}, {SP_RESO, .1f}, {SP_VCF_ENV, 0.f}, {SP_VCA_DEC, .62f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .35f}, {SP_DRIVE, .05f},
     {SP_LEVEL, .7f}, {SP_PAN, .4f}, END},
    /* 3 PAD: WAVE through the BBD swarm, the modulator scanning the table */
    {VOICE(ENG_WAVE), {SP_V1, .2f}, {SP_V2, .4f}, {SP_V3, .6f}, {SP_V4, .5f / 4}, {SP_V5, 0.f}, {SP_V6, .1f},
     {SP_VCA_DEC, 0.f}, {SP_VCA_ATK, .5f}, {SP_VCA_REL, .65f}, {SP_CUTOFF, .62f}, {SP_RESO, .2f}, {SP_VCF_ENV, .1f},
     {SP_SPREAD, .35f}, {SP_SW_MODE, 0}, {SP_SW_MIX, .7f}, {SP_MOD_DEST, 3}, {SP_MOD_WAVE, 0}, {SP_LFO_RATE, .2f},
     {SP_LFO_LVL, .5f}, {SP_DRIVE, .05f}, {SP_LEVEL, .62f}, {SP_PAN, .6f}, END},
    /* 4 PLUCK: PHYS, a string */
    {VOICE(ENG_PHYS), {SP_V1, .25f}, {SP_V2, .25f}, {SP_V3, .6f}, {SP_V4, .6f}, {SP_V5, 0.f}, {SP_V6, .7f},
     {SP_VCA_DEC, .8f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .4f}, {SP_CUTOFF, .95f}, {SP_RESO, 0.f}, {SP_VCF_ENV, 0.f}, {SP_DRIVE, .05f},
     {SP_LEVEL, .8f}, {SP_PAN, .45f}, END},
    /* 5 ORGAN: WHEEL, the slow rotor */
    {VOICE(ENG_WHEEL), {SP_V1, .8f}, {SP_V2, .8f}, {SP_V3, .35f}, {SP_V4, 2.5f / 5}, {SP_V5, .3f}, {SP_V6, .35f},
     {SP_VCA_DEC, 0.f}, {SP_VCA_REL, .2f}, {SP_CUTOFF, .9f}, {SP_RESO, 0.f}, {SP_VCF_ENV, 0.f}, {SP_DRIVE, .15f},
     {SP_LEVEL, .62f}, END},
    /* 6 CLOUD: GRAIN, slow in and out */
    {VOICE(ENG_GRAIN), {SP_V1, .55f}, {SP_V2, .55f}, {SP_V3, .45f}, {SP_V4, .4f}, {SP_V5, .25f}, {SP_V6, .7f},
     {SP_VCA_DEC, 0.f}, {SP_VCA_ATK, .55f}, {SP_VCA_REL, .7f}, {SP_CUTOFF, .7f}, {SP_RESO, .15f}, {SP_VCF_ENV, 0.f},
     {SP_MOD_DEST, 3}, {SP_LFO_RATE, .15f}, {SP_LFO_LVL, .4f}, {SP_LEVEL, .6f}, {SP_PAN, .55f}, END},
    /* 7 ARP: CHIP, 25 % pulse, a major arpeggio */
    {VOICE(ENG_CHIP), {SP_V1, 1.5f / 7}, {SP_V2, .5f}, {SP_V3, .8f}, {SP_V4, 2.5f / 8}, {SP_V5, .55f}, {SP_V6, .5f},
     {SP_VCA_DEC, .45f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .2f}, {SP_CUTOFF, .9f}, {SP_RESO, 0.f}, {SP_VCF_ENV, 0.f}, {SP_LEVEL, .55f},
     {SP_PAN, .35f}, END},
    /* 8 BELL: LXR METAL, bell character */
    {VOICE(ENG_LXR_METAL), {SP_V1, .62f}, {SP_V2, .45f}, {SP_V3, .15f}, {SP_V4, .05f}, {SP_V5, .45f}, {SP_V6, .2f},
     {SP_VCA_DEC, .5f}, {SP_SUSTAIN, 0}, {SP_VCA_REL, .4f}, {SP_CUTOFF, 1.f}, {SP_RESO, 0.f}, {SP_VCF_ENV, 0.f}, {SP_LEVEL, .5f},
     {SP_PAN, .65f}, END},
};

/* step lists: pattern, track, step number (1-16), note, flags */
typedef struct { uint8_t pat, track, step, note, f; } Note;
#define G ST_GATE
#define A (ST_GATE | ST_ACC)
#define S (ST_GATE | ST_SLIDE)
static const Note NOTES[] = {
    /* A1: the acid on 1, piano stabs on 2, the arp on 7, bells on 8 */
    {0, 0, 1, 36, A}, {0, 0, 2, 36, G}, {0, 0, 3, 48, S}, {0, 0, 4, 39, G}, {0, 0, 5, 36, A}, {0, 0, 7, 43, S},
    {0, 0, 8, 46, G}, {0, 0, 9, 36, A}, {0, 0, 10, 48, G}, {0, 0, 12, 41, S}, {0, 0, 13, 43, A}, {0, 0, 14, 36, G},
    {0, 0, 15, 51, S}, {0, 0, 16, 48, G},
    {0, 1, 1, 63, G}, {0, 1, 4, 67, G}, {0, 1, 7, 70, G}, {0, 1, 11, 67, G}, {0, 1, 13, 65, G},
    {0, 6, 1, 72, G}, {0, 6, 5, 75, G}, {0, 6, 9, 79, G}, {0, 6, 13, 77, G},
    {0, 7, 3, 84, G}, {0, 7, 7, 87, G}, {0, 7, 11, 91, G}, {0, 7, 15, 84, G},
    /* A2: the pad held on 3, plucks on 4, the cloud on 6 */
    {1, 2, 1, 60, S}, {1, 2, 2, 60, S}, {1, 2, 3, 60, S}, {1, 2, 4, 60, S}, {1, 2, 5, 60, S}, {1, 2, 6, 60, S},
    {1, 2, 7, 60, S}, {1, 2, 8, 60, G}, {1, 2, 9, 58, S}, {1, 2, 10, 58, S}, {1, 2, 11, 58, S}, {1, 2, 12, 58, S},
    {1, 2, 13, 55, S}, {1, 2, 14, 55, S}, {1, 2, 15, 56, S}, {1, 2, 16, 56, G},
    {1, 3, 1, 48, A}, {1, 3, 3, 55, G}, {1, 3, 5, 60, G}, {1, 3, 7, 63, G}, {1, 3, 9, 46, A}, {1, 3, 11, 53, G},
    {1, 3, 13, 58, G}, {1, 3, 15, 62, G},
    {1, 5, 1, 72, S}, {1, 5, 2, 72, S}, {1, 5, 3, 72, S}, {1, 5, 4, 72, S}, {1, 5, 5, 72, S}, {1, 5, 6, 72, S},
    {1, 5, 7, 72, S}, {1, 5, 8, 72, G},
    /* A3: the organ on 5 with the acid on 1 */
    {2, 4, 1, 60, S}, {2, 4, 2, 60, G}, {2, 4, 4, 63, G}, {2, 4, 7, 67, S}, {2, 4, 8, 67, G}, {2, 4, 11, 65, G},
    {2, 4, 13, 63, S}, {2, 4, 14, 63, G},
    {2, 0, 1, 36, A}, {2, 0, 3, 36, G}, {2, 0, 4, 48, S}, {2, 0, 5, 43, G}, {2, 0, 7, 36, A}, {2, 0, 8, 46, G},
    {2, 0, 9, 36, G}, {2, 0, 11, 51, S}, {2, 0, 12, 48, G}, {2, 0, 13, 41, A}, {2, 0, 15, 43, S}, {2, 0, 16, 48, G},
    /* A4: plucks on 4, the piano on 2 */
    {3, 3, 1, 60, A}, {3, 3, 2, 67, G}, {3, 3, 3, 72, G}, {3, 3, 5, 70, G}, {3, 3, 7, 67, G}, {3, 3, 9, 63, A},
    {3, 3, 10, 67, G}, {3, 3, 11, 70, G}, {3, 3, 13, 72, G}, {3, 3, 15, 75, G},
    {3, 1, 1, 48, G}, {3, 1, 9, 46, G},
    /* A5: the chip on 7, the bells on 8 */
    {4, 6, 1, 60, G}, {4, 6, 3, 60, G}, {4, 6, 4, 63, G}, {4, 6, 6, 67, G}, {4, 6, 9, 58, G}, {4, 6, 11, 58, G},
    {4, 6, 12, 62, G}, {4, 6, 14, 65, G},
    {4, 7, 1, 72, A}, {4, 7, 5, 79, G}, {4, 7, 9, 70, A}, {4, 7, 13, 77, G},
    /* A6: the cloud on 6 and the pad on 3, held */
    {5, 5, 1, 60, S}, {5, 5, 2, 60, S}, {5, 5, 3, 60, S}, {5, 5, 4, 60, S}, {5, 5, 5, 60, S}, {5, 5, 6, 60, S},
    {5, 5, 7, 60, S}, {5, 5, 8, 60, G}, {5, 5, 9, 63, S}, {5, 5, 10, 63, S}, {5, 5, 11, 63, S}, {5, 5, 12, 63, S},
    {5, 5, 13, 63, S}, {5, 5, 14, 63, S}, {5, 5, 15, 63, S}, {5, 5, 16, 63, G},
    {5, 2, 1, 48, S}, {5, 2, 2, 48, S}, {5, 2, 3, 48, S}, {5, 2, 4, 48, S}, {5, 2, 5, 48, S}, {5, 2, 6, 48, S},
    {5, 2, 7, 48, S}, {5, 2, 8, 48, S}, {5, 2, 9, 51, S}, {5, 2, 10, 51, S}, {5, 2, 11, 51, S}, {5, 2, 12, 51, S},
    {5, 2, 13, 51, S}, {5, 2, 14, 51, S}, {5, 2, 15, 51, S}, {5, 2, 16, 51, G},
};

void bl_factory(Bl *x)
{
    int t, i;
    bl_initProject(&x->p);
    for (t = 0; t < NUM_TRACKS; t++) {
        Sound *s = &x->p.snd[t];
        for (i = 0; i < 28 && SOUNDS[t][i].sp != 255; i++) {
            if (SOUNDS[t][i].sp == SP_VOICE) sound_setVoice(s, (int)SOUNDS[t][i].v);
            else s->v[SOUNDS[t][i].sp] = SOUNDS[t][i].v;
        }
        sound_sync(s);
    }
    for (i = 0; i < (int)(sizeof NOTES / sizeof NOTES[0]); i++) {
        Step *st = &x->p.pats[NOTES[i].pat].t[NOTES[i].track].s[NOTES[i].step - 1];
        st->note = NOTES[i].note;
        st->f = NOTES[i].f;
    }
    x->patSlot = 0;
    x->nextPat = 0xff;
    if (x->seq.running) seq_stop(x);
    for (t = 0; t < NUM_TRACKS; t++) voice_kill(&x->v[t]);
    fx_init(&x->fx);
    x->displayRev++;
}
