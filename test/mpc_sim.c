/* LXRXXL ZYNTH: the plugin driven the way MPC drives it, through the VST2 wrapper only (no engine internals): MPC's
 * transport and song position, notes from a track, screen taps and Q-Link turns as parameter sets, the display read as
 * MPC reads its pictures, the project saved and loaded as a chunk. Exit 1 on failure. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "params.h"
#include "param_ids.h"

typedef struct AEffect AEffect;
typedef intptr_t (*cb)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
struct AEffect { int32_t magic; intptr_t (*d)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
 void *p; void (*setP)(AEffect *, int32_t, float); float (*getP)(AEffect *, int32_t);
 int32_t np, npar, ni, no, flags; intptr_t r1, r2; int32_t a, b, c; float io; void *obj, *user; int32_t uid, ver;
 void (*pr)(AEffect *, float **, float **, int32_t); void *pdr; char f[56]; };
typedef struct { int32_t type, byteSize, deltaFrames, flags, noteLength, noteOffset; unsigned char m[4]; char x[4]; } ME;
typedef struct { int32_t n; intptr_t r; void *ev[2]; } EV;
extern AEffect *VSTPluginMain(cb);

static double ti[16];
static int playing;
static double tempo = 120.0, ppq;
static int fails;
static int automated;   /* audioMasterAutomate calls: the host is told what moved */
#define CHECK(c, ...) do { printf("%s ", (c) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(c)) fails++; } while (0)
static const char GL[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-+/";

static intptr_t host(AEffect *e, int32_t op, int32_t i, intptr_t v, void *p, float o)
{
    if (op == 7) {   /* audioMasterGetTime, as MPC answers it */
        memset(ti, 0, sizeof ti);
        ti[1] = 44100.0; ti[3] = ppq; ti[4] = tempo;
        ((int32_t *)&ti[8])[5] = (1 << 10) | (1 << 9) | (playing ? 2 : 0);
        return (intptr_t)ti;
    }
    if (op == 0) automated++;
    return 0;
}

static double run(AEffect *a, double sec)
{
    float L[256], R[256], *o[2] = {L, R};
    double e = 0;
    int k, i, blocks = (int)(sec * 44100 / 256);
    if (blocks < 1) blocks = 1;
    for (k = 0; k < blocks; k++) {
        a->pr(a, 0, o, 256);
        for (i = 0; i < 256; i++) {
            if (!(L[i] == L[i]) || fabsf(L[i]) > 1.f) { printf("bad sample %f\n", L[i]); fails++; return 0; }
            e += L[i] * L[i] + R[i] * R[i];
        }
        if (playing) ppq += 256.0 * tempo / 60.0 / 44100.0;
    }
    return sqrt(e / (blocks * 512.0));
}

static void note(AEffect *a, int status, int n, int v)
{
    ME m = {1, sizeof(ME), 0, 0, 0, 0, {(unsigned char)status, (unsigned char)n, (unsigned char)v, 0}};
    EV ev = {1, 0, {&m, 0}};
    a->d(a, 25, 0, 0, &ev, 0);
}
/* a screen tap on a key: MPC toggles the button's value */
static void tap(AEffect *a, int p) { a->setP(a, p, a->getP(a, p) > 0.5f ? 0.f : 1.f); run(a, 0.02); }
static int opt(AEffect *a, int p, int n) { return (int)lroundf(a->getP(a, p) * (n - 1)); }

/* the display as MPC shows it: the three pictures */
static void display(AEffect *a, char *s)
{
    int i;
    for (i = 0; i < 3; i++) s[i] = GL[opt(a, P_DISP1 + i, 40)];
    s[3] = 0;
}

int main(void)
{
    AEffect *a = VSTPluginMain(host);
    double r, r1;
    char d[4];
    a->d(a, 12, 0, 1, 0, 0);   /* effMainsChanged on */
    CHECK(a->npar == P_COUNT, "%d parameters", a->npar);
    r = run(a, 0.5);
    CHECK(r < 1e-5, "silent while MPC is stopped and nothing is played (%.5f)", r);

    /* the display: MPC's tempo, then 97 */
    run(a, 0.3);
    display(a, d);
    CHECK(!strcmp(d, "120"), "the display shows MPC's tempo: '%s'", d);
    CHECK(opt(a, P_DOT3, 2) == 1, "its last dot: following MPC");
    tempo = 97;
    run(a, 0.3);
    display(a, d);
    CHECK(!strcmp(d, " 97"), "and follows it: '%s'", d);
    tempo = 120;

    /* a new instance is on the factory project: MPC's play plays pattern A1 */
    playing = 1; ppq = 0;
    r = run(a, 3.0);
    CHECK(r > 0.02, "MPC play: the factory pattern A1 plays (rms %.4f)", r);
    playing = 0;
    run(a, 1.0);
    r = run(a, 1.0);
    CHECK(r < 0.03, "MPC stop: it stops (only the decays left: %.4f)", r);

    /* the panel's PLAY with MPC stopped */
    tap(a, P_PLAY);
    r = run(a, 2.0);
    CHECK(r > 0.02, "PLAY with MPC stopped: it plays at MPC's tempo (rms %.4f)", r);
    tap(a, P_PLAY);
    run(a, 2.0);
    r = run(a, 0.5);
    CHECK(r < 0.01, "PLAY again stops it (%.5f)", r);

    /* a note from the track plays the selected track */
    note(a, 0x90, 36, 100);
    r = run(a, 0.3);
    note(a, 0x80, 36, 0);
    CHECK(r > 0.01, "a note plays the selected track (rms %.4f)", r);
    run(a, 2.0);

    /* turning CUTOFF: the display shows its value for a moment, then the tempo again */
    a->setP(a, P_CUTOFF, 0.75f);
    run(a, 0.2);
    display(a, d);
    CHECK(!strcmp(d, " 75"), "turning CUTOFF shows its value: '%s'", d);
    run(a, 2.0);
    display(a, d);
    CHECK(!strcmp(d, "120"), "then the tempo again: '%s'", d);

    /* TRACK 2: the knobs show track 2's sound, and the MODULES page's track number follows */
    {
        const float c1 = a->getP(a, P_CUTOFF);
        int before = automated;
        tap(a, P_TRK2);
        run(a, 0.3);
        CHECK(opt(a, P_TRACK, 8) == 1, "TRACK 2 selects track 2");
        CHECK(fabsf(a->getP(a, P_CUTOFF) - c1) > 0.01f, "the knobs show its sound (cutoff %.2f -> %.2f)", c1, a->getP(a, P_CUTOFF));
        CHECK(automated > before, "and the host is told they moved (%d reports)", automated - before);
        tap(a, P_TRK_NEXT);
        CHECK(opt(a, P_TRACK, 8) == 2, "the MODULES page's TRACK tap: the next track");
        /* the TRACK selector, as MPC moves it: a wheel click is the read-back value -/+ 1/100, a Q-Link tick 1/128 */
        a->setP(a, P_TRACK, a->getP(a, P_TRACK) - 0.01f);
        CHECK(opt(a, P_TRACK, 8) == 1, "the wheel backwards: TRACK 2 (%d)", opt(a, P_TRACK, 8) + 1);
        a->setP(a, P_TRACK, a->getP(a, P_TRACK) - 1.f / 128);
        CHECK(opt(a, P_TRACK, 8) == 0, "a Q-Link backwards: TRACK 1 (%d)", opt(a, P_TRACK, 8) + 1);
        a->setP(a, P_TRACK, a->getP(a, P_TRACK) + 0.01f);
        CHECK(opt(a, P_TRACK, 8) == 1, "the wheel forwards: TRACK 2 (%d)", opt(a, P_TRACK, 8) + 1);
        a->setP(a, P_GD_CH, a->getP(a, P_GD_CH) - 0.01f);
        CHECK(opt(a, P_GD_CH, 16) == 8, "the duck's MIDI CH backwards: 10 -> 9 (%d)", opt(a, P_GD_CH, 16) + 1);
        a->setP(a, P_GD_CH, a->getP(a, P_GD_CH) + 1.f / 128);
        CHECK(opt(a, P_GD_CH, 16) == 9, "and forwards: 10 (%d)", opt(a, P_GD_CH, 16) + 1);
        tap(a, P_TRK1);
        /* the mixer's LEVEL 1 is track 1's LEVEL; the readout names it */
        {
            char rd[64] = "";
            a->setP(a, P_MIX_LVL1, 0.5f);
            run(a, 0.3);
            CHECK(fabsf(a->getP(a, P_LEVEL) - 0.5f) < 0.01f, "MIXER LEVEL 1 moves track 1's LEVEL (%.2f)", a->getP(a, P_LEVEL));
            a->d(a, 7, P_READOUT, 0, rd, 0);   /* effGetParamDisplay */
            CHECK(strstr(rd, "T1") && strstr(rd, "LEVEL"), "the readout: '%s'", rd);
            a->setP(a, P_MIX_LVL1, 0.8f);
        }
    }

    /* the VOICE selector: VOICE >, VOICE <, the wheel and a Q-Link step it both ways; the sound follows */
    {
        const int orig = opt(a, P_VOICE, 16);
        int v0;
        float rr;
        a->setP(a, P_VOICE, 5.f / 15);
        v0 = opt(a, P_VOICE, 16);
        tap(a, P_V_NEXT);
        CHECK(opt(a, P_VOICE, 16) == (v0 + 1) % 16, "VOICE > (%d -> %d)", v0, opt(a, P_VOICE, 16));
        tap(a, P_V_PREV);
        CHECK(opt(a, P_VOICE, 16) == v0, "VOICE < (%d)", opt(a, P_VOICE, 16));
        a->setP(a, P_VOICE, a->getP(a, P_VOICE) + 0.01f);
        CHECK(opt(a, P_VOICE, 16) == v0 + 1, "the wheel forwards: the next voice (%d)", opt(a, P_VOICE, 16));
        a->setP(a, P_VOICE, a->getP(a, P_VOICE) - 1.f / 128);
        CHECK(opt(a, P_VOICE, 16) == v0, "a Q-Link backwards: back (%d)", opt(a, P_VOICE, 16));
        a->setP(a, P_VOICE, 2.f / 15);   /* FM6 */
        a->setP(a, P_V3, 0.6f);
        note(a, 0x90, 60, 100);
        rr = (float)run(a, 0.4);
        note(a, 0x80, 60, 0);
        CHECK(rr > 0.01, "FM6 plays (rms %.4f)", rr);
        {
            char rd[96] = "";
            a->d(a, 7, P_READOUT, 0, rd, 0);
            CHECK(strstr(rd, "FM6") != NULL, "the readout names FM6's slider: '%s'", rd);
        }
        a->setP(a, P_VOICE, (float)orig / 15);
        run(a, 1.0);
    }

    /* step keys: a new line on an empty pattern (BANK A, PATTERN 8), MPC's play plays it */
    tap(a, P_SHIFT); tap(a, P_S16); tap(a, P_S8);
    run(a, 0.05);
    playing = 1; ppq = 0;
    r = run(a, 1.0);
    CHECK(r < 0.005, "PATTERN 8 is empty (%.5f)", r);
    playing = 0;
    run(a, 0.5);
    tap(a, P_S1); tap(a, P_S5); tap(a, P_S9); tap(a, P_S13);
    a->setP(a, P_NOTE, 0.5f);   /* the last step's note: C3 */
    run(a, 0.05);
    playing = 1; ppq = 0;
    r = run(a, 2.0);
    CHECK(r > 0.02, "four steps from the step keys play (rms %.4f)", r);
    playing = 0;
    run(a, 1.0);

    /* SHIFT + PLAY MODE: BACK/NOTE steps the mode both ways (MPC's wheel and Q-Link steps), SELECT/GATE stays put */
    {
        const float sel0 = a->getP(a, P_SEL);
        tap(a, P_SHIFT); tap(a, P_S6);
        display(a, d);
        CHECK(!strcmp(d, "FWD"), "SHIFT + PLAY MODE: '%s'", d);
        a->setP(a, P_NOTE, a->getP(a, P_NOTE) + 0.01f); run(a, 0.05); display(a, d);
        CHECK(!strcmp(d, "REV"), "the wheel forwards: '%s'", d);
        a->setP(a, P_NOTE, a->getP(a, P_NOTE) + 1.f / 128); run(a, 0.05); display(a, d);
        CHECK(!strcmp(d, "PNG"), "a Q-Link forwards: '%s'", d);
        a->setP(a, P_NOTE, a->getP(a, P_NOTE) - 0.01f); run(a, 0.05); display(a, d);
        CHECK(!strcmp(d, "REV"), "the wheel backwards: '%s'", d);
        a->setP(a, P_NOTE, a->getP(a, P_NOTE) - 1.f / 128); run(a, 0.05); display(a, d);
        CHECK(!strcmp(d, "FWD"), "a Q-Link backwards: '%s'", d);
        CHECK(fabsf(a->getP(a, P_SEL) - sel0) < 1e-4f, "SELECT/GATE did not move (%.3f -> %.3f)", sel0, a->getP(a, P_SEL));
        tap(a, P_SHIFT);
        /* the data knobs are endless encoders: turning SELECT/GATE to another step leaves BACK/NOTE where it stands */
        {
            int k2;
            float n0;
            a->setP(a, P_NOTE, a->getP(a, P_NOTE) + 0.01f); run(a, 0.05);
            n0 = a->getP(a, P_NOTE);
            for (k2 = 0; k2 < 6; k2++) { a->setP(a, P_SEL, a->getP(a, P_SEL) + 0.01f); run(a, 0.05); }
            CHECK(fabsf(a->getP(a, P_NOTE) - n0) < 1e-5f, "SELECT/GATE leaves BACK/NOTE where it stands (%.4f -> %.4f)", n0, a->getP(a, P_NOTE));
            a->setP(a, P_NOTE, 1.f); run(a, 0.05);
            n0 = a->getP(a, P_NOTE);
            a->setP(a, P_NOTE, 1.f); run(a, 0.05);   /* at the end of its travel: MPC sends the end again */
            CHECK(a->getP(a, P_NOTE) >= n0, "BACK/NOTE still turns at the end of its travel");
        }
        /* outside a function: SELECT/GATE walks the steps one by one, both ways */
        {
            char d0[8];
            int n0, n1, n2;
            a->setP(a, P_SEL, 0.f); run(a, 0.05);
            a->setP(a, P_SEL, a->getP(a, P_SEL) + 1.f / 128); run(a, 0.05); display(a, d0);
            n0 = atoi(strchr(d0, 'S') ? strchr(d0, 'S') + 1 : d0);
            a->setP(a, P_SEL, a->getP(a, P_SEL) + 0.01f); run(a, 0.05); display(a, d);
            n1 = atoi(strchr(d, 'S') ? strchr(d, 'S') + 1 : d);
            a->setP(a, P_SEL, a->getP(a, P_SEL) - 1.f / 128); run(a, 0.05); display(a, d);
            n2 = atoi(strchr(d, 'S') ? strchr(d, 'S') + 1 : d);
            CHECK(n0 == 2 && n1 == 3 && n2 == 2, "SELECT/GATE one step at a time, both ways (%d %d %d, %s)", n0, n1, n2, d0);
        }
        run(a, 2.0);
    }

    /* SHIFT + ACC: the step keys set accents; SHIFT leaves */
    tap(a, P_SHIFT); tap(a, P_S1);
    display(a, d);
    CHECK(!strcmp(d, "ACC"), "SHIFT + ACC: '%s'", d);
    tap(a, P_S5);
    CHECK(a->getP(a, P_S5) > 0.5f, "an accent lights its key");
    tap(a, P_SHIFT);
    display(a, d);
    CHECK(strcmp(d, "ACC"), "SHIFT leaves the function ('%s')", d);

    /* the duck: on, its channel 10 */
    tap(a, P_GD_ON);
    a->setP(a, P_GD_DEPTH, 1.f);
    run(a, 0.1);
    note(a, 0x90, 36, 100);
    r1 = run(a, 0.1);
    note(a, 0x99, 36, 100);   /* a kick on channel 10 */
    r = run(a, 0.04);
    note(a, 0x89, 36, 0);
    note(a, 0x80, 36, 0);
    CHECK(r < r1 * 0.4, "a note on channel 10 ducks the bass (%.4f -> %.4f)", r1, r);
    tap(a, P_GD_ON);
    run(a, 1.0);

    /* the project: saved as MPC saves it, loaded into a new instance */
    {
        void *chunk = NULL;
        intptr_t len = a->d(a, 23, 0, 0, &chunk, 0);   /* effGetChunk */
        AEffect *b = VSTPluginMain(host);
        char *copy;
        void *chunk2 = NULL;
        intptr_t len2;
        CHECK(len > 1000 && chunk, "the chunk: %ld bytes", (long)len);
        copy = malloc((size_t)len);
        memcpy(copy, chunk, (size_t)len);
        b->d(b, 12, 0, 1, 0, 0);
        b->d(b, 24, 0, len, copy, 0);                  /* effSetChunk */
        len2 = b->d(b, 23, 0, 0, &chunk2, 0);
        CHECK(len2 == len && !memcmp(copy, chunk2, (size_t)len), "it loads back the same");
        playing = 1; ppq = 0;
        r = run(b, 2.0);
        CHECK(r > 0.02, "and the loaded instance plays the pattern (rms %.4f)", r);
        playing = 0;
        b->d(b, 1, 0, 0, 0, 0);
        free(copy);
    }

    /* SHIFT + MUTE: RESET (the factory project) */
    tap(a, P_SHIFT); tap(a, P_MUTE);
    playing = 1; ppq = 0;
    r = run(a, 2.0);
    CHECK(r > 0.02, "RESET: pattern A1 again (rms %.4f)", r);
    playing = 0;
    run(a, 1.0);
    a->d(a, 1, 0, 0, 0, 0);   /* effClose */
    printf(fails ? "FAILED (%d)\n" : "PASSED\n", fails);
    return fails ? 1 : 0;
}
