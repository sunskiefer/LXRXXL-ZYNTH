/* LXRXXL ZYNTH -- the voice engines: one per voice type, chosen per track with the VOICE selector.
 *
 * Every engine makes the raw tone of a track (one monophonic voice): the track's voice then runs it through the same
 * chain as LXRXXL BAZZLINE (FM LVL, SUB, NOISE, SWARM, DRIVE, FILTER, VCA). Each engine has six controls: the six
 * sliders of the MODULES page (V1..V6, 0..1), whose labels change with the voice.
 *
 * Engines written here (C): ANALOG, TRIO, FM6, FM4, PHASE, CHIP, WHEEL, VOICE, GRAIN, NOISE, LXR TONE, LXR METAL and
 * BAZZ (LXRXXL BAZZLINE's oscillator). Engines from Mutable Instruments (MIT, third_party/mutable, through
 * mi_bridge.cc): MACRO (Braids' macro oscillator), WAVE (Plaits' wavetable engine) and PHYS (Plaits' string and modal
 * voices). None of the FM-1 firmwares' code is used: their voice list only inspired the choice of voices. */
#pragma once
#include <stdint.h>

enum {
    ENG_ANALOG, ENG_TRIO, ENG_FM6, ENG_FM4, ENG_PHASE, ENG_WAVE, ENG_CHIP, ENG_MACRO,
    ENG_WHEEL, ENG_VOICE, ENG_PHYS, ENG_GRAIN, ENG_NOISE, ENG_LXR_TONE, ENG_LXR_METAL, ENG_BAZZ,
    NUM_ENGINES
};
#define ENG_SLIDERS 6
#define ENG_MAX_BLOCK 8          /* the voice renders its engine in blocks of at most this many frames */

/* what an engine gets for one block (the voice's control rate) */
typedef struct {
    float note;                  /* the pitch in semitones (MIDI), with glide, pitch envelope and pitch modulation */
    float freq;                  /* the same in Hz */
    float p[ENG_SLIDERS];        /* the six sliders, 0..1 (the modulator's WAVE destination already added) */
    float vel, accent;           /* this note's velocity 0..1, accent 0/1 */
    float env;                   /* the amp envelope now, 0..1 (engines with their own envelopes follow it) */
    float fmIdx, fmRatio;        /* FM LVL as a phase-modulation index and FM RATIO (BAZZ uses them inside) */
    float bpm;
    uint8_t gate;                /* the note is held */
    uint8_t trig;                /* a new note started (not a slide) since the last block */
    uint8_t wave;                /* the panel's SQR / SAW / TRI switch (0, 1, 2): BAZZ's VCO wave */
} EngIn;

/* ---------------------------------------------------------------- states of the engines written in C */
typedef struct { uint32_t ph[2]; float last[2]; } AnalogSt;
typedef struct { uint32_t ph[3]; } TrioSt;
typedef struct { uint32_t ph[6]; float fb[2]; float menv[6]; float lvl[6]; } FmSt;
typedef struct { uint32_t ph[2]; float env; float z; } PdSt;
typedef struct { uint32_t ph; uint32_t lfsr; float hold; float holdPh; float sweep; float arpPh; int arpI; float noiseOut; } ChipSt;
#define LESLIE_LEN 512
typedef struct {
    uint32_t ph[9];
    float perc, click, clickLp;
    float rotH, rotD, spdH, spdD;
    float lsl[LESLIE_LEN];
    int lw;
    float split;
    uint32_t noise;
} WheelSt;
typedef struct { uint32_t ph; float lp; float s1[4], s2[4]; float vibPh, vibAmt; uint32_t noise; float tilt; } VoiceSt;
#define MAX_GRAINS 20
typedef struct { uint32_t ph; uint32_t inc; float pos; float t, dt; uint8_t on; } Grain;
typedef struct { Grain g[MAX_GRAINS]; float next; uint32_t rnd; float lp; } GrainSt;
typedef struct { uint32_t rnd; float b[7]; float s1[3], s2[3]; float hp, lp; float dust; float hold, holdPh; uint32_t lfsr; float crack; } NoiseSt;
typedef struct { uint32_t ph, phMod; float fmEnv; float hold; float holdPh; } LxrToneSt;
typedef struct { uint32_t ph[6]; float s1, s2; uint32_t noise; float hp; } LxrMetalSt;
typedef struct { uint32_t ph, phMod; float drift, driftT; uint32_t rnd; } BazzSt;

typedef struct {
    uint8_t type;                /* ENG_* */
    uint8_t fresh;               /* reset: the next block starts the engine from rest */
    union {
        AnalogSt an; TrioSt tr; FmSt fm; PdSt pd; ChipSt ch; WheelSt wh; VoiceSt vo; GrainSt gr; NoiseSt no;
        LxrToneSt lt; LxrMetalSt lm; BazzSt bz;
    } u;
    void *mi;                    /* the Mutable engines' state (mi_bridge.cc), allocated once per engine slot */
} Eng;

/* ---------------------------------------------------------------- the voices' names and sliders (engines.c) */
typedef struct {
    const char *name;            /* the VOICE selector: the name as given ("LXR METAL") */
    const char *shortName;       /* three characters for the panel's display */
    const char *slider[ENG_SLIDERS];   /* the readout's names of V1..V6 */
    const char *label[ENG_SLIDERS];    /* the MODULES page's label windows ("MOD|INDEX": two lines) */
    float def[ENG_SLIDERS];      /* each slider's default */
    int8_t timbre;               /* the slider the modulator's WAVE destination moves */
    uint8_t steps[ENG_SLIDERS];  /* > 0: the slider picks one of this many options */
} EngDef;
extern const EngDef ENG_DEFS[NUM_ENGINES];
/* the front panel's TIMBRE and MORPH: the two sliders (V1..V6, 0-based) that change each voice the most */
extern const int8_t ENG_MACROS[NUM_ENGINES][2];

void eng_tables_init(void);                     /* the shared tables (sine, windows, grain waves): once */
void eng_reset(Eng *e, int type);               /* a new voice type, or the voice from rest */
void eng_render(Eng *e, const EngIn *in, float *out, int n);   /* writes n <= ENG_MAX_BLOCK frames */
int eng_step(int type, int slider, float v);    /* a stepped slider's option index */
void eng_value_text(int type, int slider, float v, char *buf, int n, int full);   /* the display (3 chars) / readout */
float eng_sub_octave(int type, const float *p);   /* the SUB oscillator's octave below the note (1 or 2) */
float eng_tone(int type, const float *p);       /* the post-filter TONE, 0..1 (1 = open) */

/* mi_bridge.cc: the Mutable Instruments engines (C++) */
void *mi_new(void);
void mi_free(void *m);
void mi_reset(void *m, int type);
void mi_render(void *m, int type, const EngIn *in, float *out, int n);
