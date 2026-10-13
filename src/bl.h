/* LXRXXL ZYNTH -- one instance: the project in RAM, eight synth tracks, the sequencer, the delay, the gate/duck and
 * the front panel.
 *
 * A variation of LXRXXL BAZZLINE (itself a variation of LXRXXL, the open-source LXR engine inspired by the LXR-02):
 * the same LXR-02 style step sequencer, panel and Force integration, and the same voice chain (FM, sub, noise, swarm,
 * drive, filter, VCA), with the raw tone of each track made by one of sixteen voice engines picked per track (eng.h). */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ids.h"
#include "eng.h"

#define NUM_TRACKS    8
#define NUM_STEPS     64
#define NUM_BANKS     4
#define NUM_PATTERNS  (NUM_BANKS * 16)
#define SR            44100.0f

/* ---------------------------------------------------------------- the project (what MPC's project keeps) */
enum { ST_GATE = 1, ST_ACC = 2, ST_SLIDE = 4, ST_MOD = 8, ST_PENV = 16 };

typedef struct {
    uint8_t f;      /* ST_* flags */
    uint8_t note;   /* MIDI note */
} Step;

enum { PM_FWD, PM_REV, PM_PONG, PM_RND, PM_COUNT };
enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RND, ARP_COUNT };
#define NUM_SCALES 12

typedef struct {
    Step s[NUM_STEPS];
    uint8_t len;        /* 1..64 */
    uint8_t playMode;   /* PM_* */
    int8_t transp;      /* -24..+24 semitones */
    uint8_t scale;      /* 0 = chromatic (no quantising) */
    uint8_t arp;        /* ARP_* */
    uint8_t pad[3];
} TrackPat;

typedef struct {
    TrackPat t[NUM_TRACKS];
} Pattern;

typedef struct {
    float v[SP_COUNT];  /* 0..1, or an option's index; v[SP_V1..SP_V20]: the twenty controls of the voice in use */
    float full[NUM_ENGINES][SP_COUNT];   /* each voice's whole sound as last left on this track (its home sound until
                                            then): switching back finds it; LEVEL and PAN stay the track's */
} Sound;

/* STEP RANDOM: the harmony of a pattern (every track generated in it shares it) and each track's settings */
typedef struct {
    uint8_t valid;      /* 0: none yet (the first STEP RANDOM makes one) */
    uint8_t root;       /* 0..11, C = 0 */
    uint8_t mode;       /* HARM_MODES index */
    uint8_t prog;       /* PROGRESSIONS index */
    uint8_t color;      /* the chords: triads, sevenths, sus 2, sus 4, add 9 */
    uint8_t pad[3];
    uint32_t seed;
} Harmony;

enum { RS_AUTO, RS_CHORD, RS_ARP, RS_LEAD, RS_BASS, RS_HITS, RS_COUNT };
enum { RV_FLAT, RV_GROOVE, RV_HUMAN, RV_SWELL, RV_COUNT };
typedef struct {
    uint8_t lenOpt;     /* the length STEP RANDOM writes: 4 8 12 16 24 32 48 64 steps */
    uint8_t vel;        /* RV_*: how it sets the steps' velocities */
    uint8_t style;      /* RS_*: what the track plays (AUTO: from its voice) */
    uint8_t pad;
} RndSet;

typedef struct {
    Sound snd[NUM_TRACKS];
    Pattern pats[NUM_PATTERNS];
    float g[G_COUNT];   /* the delay and the gate/duck */
    float bpm;          /* 0 = follow MPC's tempo and transport, else the instance's own tempo */
    uint8_t links;      /* bit t: track t plays the steps of the track before it */
    uint8_t mutes;      /* bit t: track t is muted */
    uint8_t gdOn;       /* the gate/duck is on */
    uint8_t dlySync;    /* the delay: 0 free (ms), 1 MPC's beat, 2 tapped */
    float dlyTap;       /* the tapped period in seconds (dlySync 2) */
    uint8_t pad[4];
    /* the MIXER */
    float send[NUM_TRACKS];   /* each track's delay send 0..1 */
    float master;             /* 0..1, 0.8 = unity */
    uint8_t solos;            /* bit t: track t soloed */
    uint8_t pad2[3];
    /* 0.2.0: STEP RANDOM's harmonies and settings, and every step's velocity (0: as before: 100, or 127 accented) */
    Harmony harm[NUM_PATTERNS];
    RndSet rset[NUM_TRACKS];
    uint8_t vel[NUM_PATTERNS][NUM_TRACKS][NUM_STEPS];
    uint8_t starter[NUM_TRACKS][NUM_ENGINES];   /* each track's starter sound per voice (the VOICE page's SOUND) */
    uint8_t pad3[(4 - (NUM_TRACKS * NUM_ENGINES) % 4) % 4];
} Project;

/* ---------------------------------------------------------------- the voice (voice.c) */
#define BBD_LEN 2048
#define PM_LEN 4096                  /* FM LVL on any voice: the tone through a delay line swung by a sine */
typedef struct {
    Eng eng[2];                      /* the voice's engine; the second one: UNISON's detuned copy */
    uint32_t phSub, phPm;            /* the sub oscillator; FM LVL's modulator */
    uint32_t noise;
    float pm[PM_LEN];
    int pmW;
    uint8_t trig;                    /* a new note for the engine (not a slide) */
    /* pitch */
    float note, target, glide;       /* the playing note (semitones, with slide), where it slides to, the slide rate */
    float penv;                      /* the pitch envelope 0..1 */
    /* envelopes */
    float aenv, fenv;                /* amp, filter */
    uint8_t aStage, fStage;          /* 0 off, 1 attack, 2 decay */
    /* the VOICE page's play row: the modulation envelope (attack, decay) and the vibrato */
    float menv;
    uint8_t menvStage;
    float vibPh, vibAmt;
    uint8_t gate;
    float accent;                    /* this note's accent 0..1 */
    float sweep;                     /* the accent sweep's charge */
    float vel;
    /* modulator */
    double lfoPh;
    float lfoHold, lfoPrev, lfoNext, lfoOut;
    uint32_t lfoRnd;
    uint8_t modOn;                   /* this note takes the modulator (MOD steps) */
    /* filter (2x oversampled) */
    float s1, s2, d1, d2, d3, d4;    /* the Polivoks SVF's two integrators; the diode ladder's four stages */
    float toneZ, dcX;
    float cutSm, resSm;
    /* swarm */
    float bbd[BBD_LEN];
    int bbdW;
    double swPh[2];
    float bbdLp;
    uint8_t active;                  /* sounding (or a tail still running) */
    uint32_t idle;                   /* frames of silence */
} Voice;

/* ---------------------------------------------------------------- the delay and the gate/duck (fx.c) */
#define DLY_LEN 65536                /* 1.48 s at 44.1 kHz: 800 ms plus room for the tape's slew */
typedef struct {
    float *buf[2];
    int w;
    float rd;                        /* the read distance in frames (slewed: tape mode bends pitch) */
    float rdOld, xfade;              /* digital mode: the old tap while crossfading to a new time */
    float lpz[2], hpz[2];
    double wowPh;
    uint8_t hold, add, rev;
    float holdLen;                   /* HOLD: the frozen loop's length */
    int revPos;                      /* REVERSE: position in the reversed window */
    float wetSm, fbSm, inSm;
    /* gate/duck */
    float gdEnv;
    uint32_t gdHold;
    uint8_t gdStage;                 /* 0 idle, 1 attack, 2 hold, 3 release */
    /* tap */
    double lastTap;
    int taps;
} Fx;

/* ---------------------------------------------------------------- the front panel modes (ui.c) */
enum Mode {
    MD_NORMAL, MD_ACC, MD_SLIDE, MD_MOD, MD_PENV, MD_LAST, MD_PLAYMODE, MD_TRANSP, MD_SCALE, MD_ARP, MD_LFO,
    MD_BANK, MD_PATTERN, MD_RNDSET
};

typedef struct Bl {
    Project p;
    uint8_t patSlot;                 /* the pattern that plays and that the keys edit (0..63) */
    uint8_t nextPat;                 /* queued for the pattern's end (0xff none) */
    Pattern clip;                    /* COPY / PASTE: one track's steps (t[0]) */
    uint8_t clipVel[NUM_STEPS];      /* and their velocities */
    uint8_t clipValid;

    Voice v[NUM_TRACKS];
    Fx fx;

    struct {                         /* the randomizers (random.c), from LXRXXL */
        uint8_t amt, seqAmt;         /* SOUND RND AMT, STEP RND AMT: 0..10 (tens of %) */
        uint32_t flash, seqFlash;    /* the keys lit until (frames) */
        uint64_t state;
        float prev[NUM_TRACKS][SP_COUNT];   /* where each value was before the last press (a press goes somewhere new) */
    } rnd2;

    struct {                         /* the sequencer (seq.c) */
        uint8_t running, recording;
        uint8_t freeRun;             /* started by the panel's PLAY while following MPC's tempo: MPC's position not used */
        double pos;                  /* position in 16ths since the start */
        uint32_t n;                  /* the absolute 16th that fires next */
        uint32_t patStart;
        uint8_t pos16[NUM_TRACKS];   /* the step each track played last */
        int8_t dir[NUM_TRACKS];      /* ping-pong direction */
        uint32_t cnt[NUM_TRACKS];    /* steps played since start (play modes, arp) */
        double gateOff[NUM_TRACKS];  /* where the playing step's gate closes (16ths), < 0 none */
        uint8_t slideHeld[NUM_TRACKS];
        float bpm;                   /* the tempo in use */
        uint8_t hostPlaying;
        int hostValid;
        double hostPpq, hostTempo;
        uint32_t frames;             /* frames since the last host time */
        uint32_t rng;
    } seq;

    struct {                         /* the front panel (ui.c) */
        uint8_t track;               /* the selected track 0..7 */
        uint8_t mode;                /* MD_* */
        uint8_t shift;               /* SHIFT latched */
        uint8_t bar;                 /* the bar the step keys show 0..3 */
        uint8_t sel;                 /* the selected step 0..63 */
        int8_t oct;                  /* note entry octave -3..+3 */
        uint8_t bank;                /* the bank the PATTERN keys pick from */
        char msg[4];                 /* a temporary display text (3 characters + dots in msgDots) */
        uint8_t msgDots;
        uint32_t msgUntil;           /* frames */
        uint32_t flash[NUM_BUTTONS]; /* a key lit until (frames) */
        uint32_t pressFlip[NUM_BUTTONS];
        uint8_t held[NUM_TRACKS];    /* MIDI: keys held per track (a key over a held one slides) */
        uint8_t lastNote[NUM_TRACKS];
        float encPos[2];             /* SELECT/GATE, BACK/NOTE: where each knob stands (< 0: at its value) */
        uint8_t encMode[2];          /* the mode it stood in */
        double lastTap;              /* the TAP key */
        int taps;
    } ui;

    uint32_t clock;                  /* frames since the instance started */
    uint32_t displayRev;
    uint32_t rnd;                    /* the RND PATT generator */
    int16_t touched;                 /* the plugin parameter last edited (the readout), -1 none, TOUCH_PRESET */
    char dataDir[512];

    struct {                         /* presets on the device's storage (preset.c) */
        uint32_t id;                 /* this instance (loads finished on a worker find it by this) */
        uint8_t kind;                /* PR_SOUND / PR_PERF */
        uint8_t slot;                /* 0..NUM_SLOTS-1 */
        uint8_t armed;               /* PR_SAVE / PR_LOAD: waiting for the second tap */
        uint8_t loading;             /* loads in flight */
        uint32_t armUntil;
        char msg[96];                /* the readout's line */
    } pr;
} Bl;
enum { PR_SOUND, PR_PERF };
enum { PR_NONE, PR_SAVE, PR_LOAD };
#define TOUCH_PRESET (-2)

/* bl.c */
void bl_init(Bl *x);
void bl_initProject(Project *p);
void init_sound(Sound *s);                    /* the default sound (every control at its default) */
void bl_factory(Bl *x);                       /* the factory project (factory.c), pattern A1, stopped */
void bl_render(Bl *x, float *L, float *R, int n);
float bl_tempo(Bl *x);                        /* the tempo in use (MPC's or the instance's own) */
const TrackPat *bl_trackPat(Bl *x, int t);    /* what track t plays: its own steps or, linked, the track's it follows */
static inline Pattern *bl_pat(Bl *x) { return &x->p.pats[x->patSlot]; }
/* a step's velocity (0: the default) in the playing pattern */
static inline uint8_t *bl_vel(Bl *x, int t) { return x->p.vel[x->patSlot][t]; }
static inline TrackPat *bl_edit(Bl *x) { return &x->p.pats[x->patSlot].t[x->ui.track]; }   /* what the keys edit */
int bl_trackSrc(Bl *x, int t);                /* the track whose steps track t plays (itself, or the one it is linked to) */

/* voice.c */
void voice_init(Voice *v);
void voice_noteOn(Voice *v, const Sound *s, float note, float vel, int accent, int slide, int penv, int mod);
void voice_noteOff(Voice *v);
void voice_render(Voice *v, const Sound *s, float bpm, float *L, float *R, int n);   /* adds into L/R */
void voice_kill(Voice *v);
void voice_attach(Voice *v);                  /* the Mutable engines' state (once, at the instance's start) */
void voice_detach(Voice *v);

/* sound.c: the voice selector and each voice's sliders */
void sound_setVoice(Sound *s, int type);      /* change the voice: the sliders go to (and come back from) its memory */
void sound_setSlider(Sound *s, int k, float v);   /* V1..V6: the voice in use's slider (and its memory) */
void sound_resetVoice(Sound *s, int type);    /* every slider of that voice back to its default */
void sound_sync(Sound *s);                    /* v[] written into the voice in use's memory */
void sound_home(int type, float *v);          /* a voice's home sound: the chain set so the voice sounds as itself */
void sound_starter(Sound *s, int type, int idx);   /* a voice's starter sound (0: the home sound) */
const char *sound_starterName(int type, int idx);
#define NUM_STARTER_SOUNDS 8

/* random.c */
const char *rnd_sounds(Bl *x, int pct, int track);   /* SOUND RANDOM: one track, or every track (track < 0); the
                                                        character it went for (one track) */
/* seqgen.c */
void rnd_steps(Bl *x, int pct, int track);    /* STEP RANDOM: one track, or every track (track < 0) */
void rnd_newHarmony(Bl *x);                   /* a new harmony for the playing pattern */
Harmony *rnd_harmony(Bl *x);                  /* the playing pattern's harmony (made when it has none) */
const char *rnd_modeName(int mode, int full);
const char *rnd_styleName(int style);
int rnd_styleOf(Bl *x, int track);
void rnd_harmonyText(Bl *x, char *buf, int n);  /* "C MINOR   I-VI-III-VII   SEVENTHS" */
void rnd_harmonyNudge(Bl *x, int what, int to); /* 0: the root, 1: the mode, 2: a new progression and colour */            /* the style a track plays (AUTO resolved from its voice) */
extern const uint8_t RND_LENGTHS[8];
#define NUM_HARM_MODES 7

/* fx.c */
void fx_init(Fx *f);
void fx_free(Fx *f);
void fx_render(Bl *x, float *L, float *R, const float *sL, const float *sR, int n);   /* s: the delay sends */
void fx_duckTrigger(Bl *x);
float fx_delaySeconds(Bl *x);
int fx_syncFactorIndex(float knob);

/* seq.c */
void seq_start(Bl *x);
void seq_stop(Bl *x);
void seq_hostTime(Bl *x, int playing, double ppq, double tempo);
void seq_advance(Bl *x, int frames, int at);  /* the clock over frames, firing steps at their frame (at: block offset) */
double seq_nextEventIn(Bl *x);                /* frames to the next step or gate, from now (large = none) */
void seq_loadPattern(Bl *x, uint8_t pat);
void seq_queuePattern(Bl *x, uint8_t pat);
void seq_recordNote(Bl *x, uint8_t track, uint8_t note, uint8_t vel);
void seq_randomTrack(Bl *x, uint8_t track);
int seq_hasFlag(const TrackPat *tp, uint8_t flag);
uint8_t seq_quantise(uint8_t note, uint8_t scale, uint8_t root);
uint16_t seq_scaleMask(uint8_t scale);
const char *seq_scaleName(uint8_t s);

/* ui.c */
void ui_init(Bl *x);
void ui_press(Bl *x, int b);
int ui_led(Bl *x, int b);
void ui_knob(Bl *x, int which, float norm);   /* SELECT/GATE (0) and BACK/NOTE (1) */
float ui_knobGet(Bl *x, int which);
void ui_soundTouched(Bl *x, int sp);          /* a sound knob moved: the display shows its value */
void ui_globalTouched(Bl *x, int g);
void ui_message(Bl *x, const char *text, uint8_t dots, int ms);
void ui_display(Bl *x, char out[4], uint8_t *dots);   /* what the display shows now */
void ui_midiNote(Bl *x, uint8_t ch, uint8_t note, uint8_t vel);
void ui_readout(Bl *x, char *buf, int n);
void ui_touch(Bl *x, int param);              /* a parameter was edited: the readout shows it */     /* the readout: the last edited parameter, its name and value */

/* preset.c */
void preset_init(Bl *x);
void preset_forget(Bl *x);
void preset_poll(Bl *x);                      /* at each audio block: a finished load comes in */
void preset_press(Bl *x, int b);              /* B_PR_SOUND / PERF / SAVE / LOAD */
void preset_slot(Bl *x, int slot);
int preset_led(Bl *x, int b);

/* store.c */
char *store_serialize(Bl *x, int *len);
int store_deserialize(Bl *x, const char *text);

/* shared helpers */
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline uint32_t bl_rand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
