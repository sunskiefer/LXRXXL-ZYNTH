/* LXRXXL ZYNTH -- engine tests (x86, under valgrind by test/run_tests.sh):
 *   - every voice plays (each of the sixteen on a track, by MIDI): sound, no NaN, no clipping;
 *   - VOICE, V1-V6 and each voice's memory; VOICE < >; SOUND RANDOM and STEP RANDOM;
 *   - the worst case's time (eight tracks, the heaviest voices, unison and FM);
 *   - the six demo patterns play on MPC's transport: sound, no NaN, no clipping run-away, steps on the grid;
 *   - every key, every parameter and the encoders, in every mode, with random values (no crash, values in range);
 *   - the state chunk round-trips exactly; a damaged chunk is refused;
 *   - MIDI notes play the selected track, notes on the duck's channel duck the mix;
 *   - the delay: HOLD loops, REVERSE and TAPE/DIGITAL run clean; the display shows the tempo.
 * With an argument (a folder) it writes each demo pattern to <folder>/A<n>.wav to listen to. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "engine.h"
#include "bl.h"
#include "param_ids.h"
#include "param_map.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const mpc_engine_t *E;

static void set(void *in, const char *k, const char *v) { E->set_param(in, k, v); }
static void setf(void *in, const char *k, float v) { char b[32]; snprintf(b, sizeof b, "%g", v); E->set_param(in, k, b); }
static float getf(void *in, const char *k) { char b[64] = ""; E->get_param(in, k, b, sizeof b); return (float)atof(b); }

/* render seconds of audio with MPC playing at tempo from ppq; returns peak, rms in *rms */
static float play(void *in, double *ppq, double tempo, double secs, int playing, double *rms, FILE *wav)
{
    int16_t buf[256];
    int blocks = (int)(secs * 44100 / 128), b, i;
    float peak = 0;
    double sum = 0;
    long n = 0;
    for (b = 0; b < blocks; b++) {
        char ht[96];
        snprintf(ht, sizeof ht, "%d %.9f %.6f", playing, *ppq, tempo);
        set(in, "host_time", ht);
        E->render(in, buf, 128);
        for (i = 0; i < 256; i++) {
            const float v = buf[i] / 32768.f;
            if (fabsf(v) > peak) peak = fabsf(v);
            sum += v * v;
            n++;
        }
        if (wav) fwrite(buf, 2, 256, wav);
        if (playing) *ppq += 128.0 / 44100.0 * tempo / 60.0;
    }
    *rms = sqrt(sum / n);
    return peak;
}

static void wav_header(FILE *f, int frames)
{
    const int data = frames * 4;
    const int hdr[] = {0x46464952, 36 + data, 0x45564157, 0x20746d66, 16, 0x00020001, 44100, 44100 * 4, 0x00100004,
                       0x61746164, data};
    fwrite(hdr, 4, 11, f);
}

int main(int argc, char **argv)
{
    void *in, *in2;
    Bl *x;
    double ppq = 0, rms;
    float peak;
    int p, i, k;
    char key[32];
    E = mpc_engine();
    in = E->create(NULL);
    CHECK(in != NULL, "create");
    x = (Bl *)in;   /* Inst starts with the Bl */

    /* ---- the demo patterns, on MPC's transport */
    for (p = 0; p < 6; p++) {
        FILE *wav = NULL;
        char path[600];
        int16_t dummy;
        (void)dummy;
        seq_loadPattern(x, (uint8_t)p);
        ppq = 0;
        if (argc > 1) {
            snprintf(path, sizeof path, "%s/A%d.wav", argv[1], p + 1);
            wav = fopen(path, "wb");
            if (wav) wav_header(wav, (int)(8.0 * 44100 / 128) * 128);
        }
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);   /* MPC stopped */
        peak = play(in, &ppq, 120, 8.0, 1, &rms, wav);
        if (wav) fclose(wav);
        printf("A%d: peak %.3f rms %.4f\n", p + 1, peak, rms);
        CHECK(rms > 0.01, "pattern A%d is silent (rms %f)", p + 1, rms);
        CHECK(peak < 0.999, "pattern A%d clips (peak %f)", p + 1, peak);
        CHECK(x->seq.running, "the sequencer follows MPC's play");
        play(in, &ppq, 120, 0.1, 0, &rms, NULL);   /* MPC stops */
        CHECK(!x->seq.running, "the sequencer stops with MPC");
        play(in, &ppq, 120, 2.0, 0, &rms, NULL);
    }
    seq_loadPattern(x, 0);

    /* ---- the grid: step 5 fires exactly on beat 2 (ppq 1.0) */
    {
        double q = 0.75 - 128.0 / 44100.0 * 2.0;
        int fired = -1, b2;
        uint32_t before;
        play(in, &q, 120, 0.01, 1, &rms, NULL);
        before = x->seq.n;
        for (b2 = 0; b2 < 200 && fired < 0; b2++) {
            play(in, &q, 120, 128.0 / 44100.0, 1, &rms, NULL);
            if (x->seq.n >= 5 && before < 5) fired = b2;
        }
        CHECK(fired >= 0 && fabs(q - 1.0) < 2 * 128.0 / 44100.0 * 2.0, "step 5 at beat 2 (ppq now %.4f)", q);
        play(in, &q, 120, 0.1, 0, &rms, NULL);
    }

    /* ---- the display: the tempo, MPC's dot */
    play(in, &ppq, 97, 2.0, 0, &rms, NULL);
    CHECK((int)getf(in, "disp2") == (int)(strchr(GLYPHS, '9') - GLYPHS) && (int)getf(in, "disp3") == (int)(strchr(GLYPHS, '7') - GLYPHS),
          "the display shows 97 (%g %g)", getf(in, "disp2"), getf(in, "disp3"));
    CHECK(getf(in, "dot3") == 1.f, "the last dot: MPC's tempo");

    /* ---- every key, every parameter, every mode, random values */
    srand(7);
    for (i = 0; i < 20000; i++) {
        const int r = rand() % P_COUNT;
        if (PARAM_MAP[r].kind == PK_BUTTON) set(in, PARAM_KEYS[r], "1");
        else if (PARAM_MAP[r].nopts) {
            snprintf(key, sizeof key, "%d", rand() % (PARAM_MAP[r].nopts + 2) - 1);
            set(in, PARAM_KEYS[r], key);
        } else setf(in, PARAM_KEYS[r], (rand() % 1200) / 1000.f - 0.1f);
        if (i % 50 == 0) {
            peak = play(in, &ppq, 120 + (i % 7) * 20, 0.05, (i / 500) & 1, &rms, NULL);
            CHECK(peak == peak && peak <= 1.f, "audio after random edits");
        }
        if (i % 97 == 0)
            for (k = 0; k < P_COUNT; k++) {
                char b[128] = "";
                char k2[48];
                E->get_param(in, PARAM_KEYS[k], b, sizeof b);
                snprintf(k2, sizeof k2, "%s_on", PARAM_KEYS[k]);
                E->get_param(in, k2, b, sizeof b);
            }
    }
    for (k = 0; k < P_COUNT; k++) {
        const float v = getf(in, PARAM_KEYS[k]);
        if (PARAM_MAP[k].kind == PK_BUTTON) continue;
        if (PARAM_MAP[k].nopts) CHECK(v >= 0 && v <= PARAM_MAP[k].nopts - 1, "%s option %g", PARAM_KEYS[k], v);
        else CHECK(v >= 0 && v <= 1, "%s value %g", PARAM_KEYS[k], v);
    }
    /* leave every mode */
    for (i = 0; i < 3 && x->ui.mode != MD_NORMAL; i++) set(in, "shift", "1");
    if (x->ui.shift) set(in, "shift", "1");
    CHECK(x->ui.mode == MD_NORMAL && !x->ui.shift, "SHIFT leaves a function");

    /* ---- the state round-trip */
    {
        static char buf[1 << 20];
        static char buf2[1 << 20];
        int len = E->get_param(in, "state", buf, sizeof buf);
        CHECK(len > 1000 && len < (int)sizeof buf, "state length %d", len);
        in2 = E->create(NULL);
        set(in2, "state", buf);
        E->get_param(in2, "state", buf2, sizeof buf2);
        CHECK(!strcmp(buf, buf2), "the state round-trips");
        CHECK(!memcmp(&((Bl *)in2)->p, &x->p, sizeof(Project)), "the project round-trips");
        buf[40] = 'z';
        ((Bl *)in2)->p.bpm = 77;
        set(in2, "state", buf);
        CHECK(((Bl *)in2)->p.bpm == 77, "a damaged state is refused");
        E->destroy(in2);
    }

    /* ---- RESET, MIDI notes, the duck */
    set(in, "shift", "1");
    set(in, "mute", "1");
    CHECK(x->patSlot == 0 && x->p.pats[0].t[0].s[0].f == (ST_GATE | ST_ACC), "SHIFT + MUTE: the factory project");
    play(in, &ppq, 120, 0.5, 0, &rms, NULL);
    {
        uint8_t on[3] = {0x90, 36, 100}, off[3] = {0x80, 36, 0};
        float r1, r2;
        set(in, "trk1", "1");
        E->midi(in, on, 3);
        peak = play(in, &ppq, 120, 0.3, 0, &rms, NULL);
        CHECK(rms > 0.01, "a MIDI note plays the selected track (rms %f)", rms);
        r1 = (float)rms;
        /* the duck: on, a note on its channel (10) */
        set(in, "gd_on", "1");
        setf(in, "gd_depth", 1.f);
        setf(in, "gd_mode", 1);
        {
            uint8_t duck[3] = {0x99, 36, 100};
            E->midi(in, duck, 3);
        }
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);
        r2 = (float)rms;
        CHECK(r2 < r1 * 0.3f, "a note on the duck's channel ducks (%f -> %f)", r1, r2);
        E->midi(in, off, 3);
        set(in, "gd_on", "1");
    }

    /* ---- the delay: HOLD, ADD, REVERSE, TAPE and DIGITAL, a tapped sync */
    setf(in, "dly_wet", 0.5f);
    setf(in, "dly_fb", 1.f);
    for (k = 0; k < 2; k++) {
        setf(in, "dly_mode", (float)k);
        set(in, "play", "1");
        play(in, &ppq, 120, 2.0, 0, &rms, NULL);
        set(in, "d_hold", "1");
        set(in, "d_add", "1");
        set(in, "d_rev", "1");
        setf(in, "dly_time", 0.1f + 0.4f * k);
        peak = play(in, &ppq, 120, 3.0, 0, &rms, NULL);
        CHECK(peak == peak && peak < 1.f && rms > 0.001, "the delay held, reversed (%s): peak %f rms %f", k ? "digital" : "tape", peak, rms);
        set(in, "d_hold", "1");
        set(in, "d_rev", "1");
        set(in, "d_tap", "1");
        play(in, &ppq, 120, 0.3, 0, &rms, NULL);
        set(in, "d_tap", "1");
        CHECK(x->p.dlySync == 2 && x->p.dlyTap > 0.25f && x->p.dlyTap < 0.35f, "taps set the delay's period (%f)", x->p.dlyTap);
        set(in, "stop", "1");
    }
    setf(in, "dly_fb", 1.f);
    peak = play(in, &ppq, 120, 20.0, 0, &rms, NULL);
    CHECK(peak < 1.f, "self-oscillating feedback stays bounded (%f)", peak);

    /* ---- the MIXER: levels, mutes, solos, sends, master, the readout */
    {
        float full, r;
        char b[128] = "";
        set(in, "stop", "1");
        set(in, "state", "");   /* ignored: a bad chunk changes nothing */
        set(in, "shift", "1"); set(in, "mute", "1"); set(in, "shift", "1");   /* RESET: the factory project */
        seq_loadPattern(x, 0);
        x->p.mutes = 0; x->p.solos = 0;
        setf(in, "dly_wet", 0.f);
        setf(in, "dly_fb", 0.f);
        ppq = 0;
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);
        play(in, &ppq, 120, 4.0, 1, &rms, NULL); full = (float)rms;
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        for (k = 1; k <= 8; k++) { snprintf(key, sizeof key, "mute%d", k); set(in, key, "1"); }
        CHECK(x->p.mutes == 0xff, "MUTE 1-8 mute every track (%02x)", x->p.mutes);
        ppq = 0;
        play(in, &ppq, 120, 4.0, 1, &rms, NULL); r = (float)rms;
        CHECK(r < 1e-4f, "all muted: silence (%f, full %f)", r, full);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        for (k = 1; k <= 8; k++) { snprintf(key, sizeof key, "mute%d", k); set(in, key, "1"); }
        set(in, "solo1", "1");
        CHECK(x->p.solos == 1, "SOLO 1 (%02x)", x->p.solos);
        ppq = 0;
        play(in, &ppq, 120, 4.0, 1, &rms, NULL); r = (float)rms;
        E->get_param(in, "solo1_on", b, sizeof b);
        CHECK(atof(b) > 0, "SOLO 1 lights");
        CHECK(r > 0.001f && r < full, "track 1 soloed: only its bass (%f < %f)", r, full);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        set(in, "solo1", "1");
        setf(in, "mix_master", 0.f);
        ppq = 0;
        play(in, &ppq, 120, 2.0, 1, &rms, NULL);
        CHECK(rms < 1e-4, "MASTER down: silence (%f)", rms);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        setf(in, "mix_master", 0.8f);
        setf(in, "mix_lvl1", 0.25f);
        CHECK(fabsf(x->p.snd[0].v[SP_LEVEL] - 0.25f) < 1e-4f && fabsf(getf(in, "level") * 0 + getf(in, "mix_lvl1") - 0.25f) < 1e-4f,
              "the mixer's LEVEL 1 is track 1's LEVEL");
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "T1") && strstr(b, "LEVEL"), "the readout names the last control ('%s')", b);
        setf(in, "cutoff", 0.5f);
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "CUTOFF") != NULL, "the readout follows the knob ('%s')", b);
        setf(in, "mix_snd2", 0.f);
        CHECK(x->p.send[1] == 0.f, "DLY SEND 2 (%f)", x->p.send[1]);
        setf(in, "mix_snd2", 1.f);

    }

    /* ---- VOICE RESET: the selected voice back to the default sound; SHIFT + it: MUTE ALL, again: all back */
    {
        set(in, "trk2", "1");
        setf(in, "cutoff", 0.99f);
        set(in, "v_reset", "1");
        CHECK(fabsf(x->p.snd[1].v[SP_CUTOFF] - SOUND_DEFS[SP_CUTOFF].def) < 1e-6f, "VOICE RESET: track 2's cutoff back (%f)", x->p.snd[1].v[SP_CUTOFF]);
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_FM6, "VOICE RESET keeps the voice (%g)", x->p.snd[1].v[SP_VOICE]);
        set(in, "shift", "1"); set(in, "v_reset", "1");
        CHECK(x->p.mutes == 0xff, "SHIFT + VOICE RESET mutes every track (%02x)", x->p.mutes);
        set(in, "shift", "1"); set(in, "v_reset", "1");
        CHECK(x->p.mutes == 0, "and again: every track back (%02x)", x->p.mutes);
        set(in, "trk1", "1");
    }

    /* ---- every voice plays, by MIDI on track 1 */
    {
        uint8_t on[3] = {0x90, 57, 100}, off[3] = {0x80, 57, 0};
        int ty;
        set(in, "trk1", "1");
        set(in, "shift", "1"); set(in, "mute", "1");   /* the factory project */
        x->p.mutes = 0;
        setf(in, "dly_wet", 0.f);
        for (ty = 0; ty < NUM_ENGINES; ty++) {
            snprintf(key, sizeof key, "%d", ty);
            set(in, "voice", key);
            setf(in, "vca_env", 0.f);
            setf(in, "cutoff", 0.9f);
            E->midi(in, on, 3);
            peak = play(in, &ppq, 120, 0.6, 0, &rms, NULL);
            E->midi(in, off, 3);
            printf("voice %-10s peak %.3f rms %.4f\n", ENG_DEFS[ty].name, peak, rms);
            CHECK(rms > 0.005 && peak == peak && peak < 0.999f, "voice %s plays (peak %f rms %f)", ENG_DEFS[ty].name, peak, rms);
            play(in, &ppq, 120, 0.8, 0, &rms, NULL);
        }
    }
    /* ---- VOICE, V1-V6 and the voice memory; VOICE < > */
    {
        char b[128] = "";
        set(in, "trk2", "1");
        set(in, "voice", "2");   /* FM6 */
        setf(in, "v1", 0.9f);
        set(in, "voice", "11");  /* GRAIN */
        CHECK(fabsf(x->p.snd[1].v[SP_V1] - ENG_DEFS[ENG_GRAIN].def[0]) < 1e-5f, "GRAIN starts at its defaults (%f)", x->p.snd[1].v[SP_V1]);
        setf(in, "v1", 0.1f);
        set(in, "voice", "2");
        CHECK(fabsf(x->p.snd[1].v[SP_V1] - 0.9f) < 1e-5f, "back on FM6: its V1 as left (%f)", x->p.snd[1].v[SP_V1]);
        CHECK(fabsf(x->p.snd[1].mem[ENG_GRAIN][0] - 0.1f) < 1e-5f, "GRAIN's V1 remembered (%f)", x->p.snd[1].mem[ENG_GRAIN][0]);
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "VOICE") && strstr(b, "FM6"), "the readout names the voice ('%s')", b);
        setf(in, "v3", 0.5f);
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "FM6 MOD INDEX") != NULL, "the readout names the voice's slider ('%s')", b);
        set(in, "v_next", "1");
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_FM4, "VOICE > (%g)", x->p.snd[1].v[SP_VOICE]);
        set(in, "v_prev", "1"); set(in, "v_prev", "1"); set(in, "v_prev", "1"); set(in, "v_prev", "1");
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_BAZZ, "VOICE < round to BAZZ (%g)", x->p.snd[1].v[SP_VOICE]);
        CHECK((int)getf(in, "voice") == ENG_BAZZ, "MPC reads the voice (%g)", getf(in, "voice"));
        set(in, "trk1", "1");
    }
    /* ---- the randomizers */
    {
        Project before = x->p;
        int changed = 0, t, v;
        set(in, "rnd_amt", "10");
        set(in, "rnd", "1");
        for (t = 0; t < NUM_TRACKS; t++) {
            CHECK(x->p.snd[t].v[SP_VOICE] == before.snd[t].v[SP_VOICE], "SOUND RANDOM keeps the voice (T%d)", t + 1);
            CHECK(x->p.snd[t].v[SP_LEVEL] == before.snd[t].v[SP_LEVEL], "SOUND RANDOM keeps the level (T%d)", t + 1);
            CHECK(x->p.snd[t].v[SP_CUTOFF] >= 0.3f - 1e-6f, "the cutoff stays open enough (T%d %f)", t + 1, x->p.snd[t].v[SP_CUTOFF]);
            for (v = 0; v < SP_COUNT; v++) changed += x->p.snd[t].v[v] != before.snd[t].v[v];
            for (v = 0; v < ENG_SLIDERS; v++)
                CHECK(x->p.snd[t].v[SP_V1 + v] == x->p.snd[t].mem[(int)x->p.snd[t].v[SP_VOICE]][v], "the sliders and the memory agree");
        }
        CHECK(changed > 100, "SOUND RANDOM changes the sounds (%d)", changed);
        before = x->p;
        set(in, "shift", "1");
        set(in, "rnd", "1");
        changed = 0;
        for (t = 1; t < NUM_TRACKS; t++) for (v = 0; v < SP_COUNT; v++) changed += x->p.snd[t].v[v] != before.snd[t].v[v];
        CHECK(changed == 0, "SHIFT + SOUND RANDOM: the selected track only (%d)", changed);
        set(in, "srnd_amt", "5");
        set(in, "srnd", "1");
        {
            int gates = 0;
            for (t = 0; t < NUM_TRACKS; t++) for (v = 0; v < 16; v++) gates += (bl_pat(x)->t[t].s[v].f & ST_GATE) != 0;
            CHECK(gates >= 8 * 3 && gates <= 8 * 9, "STEP RANDOM at 50 %%: a line on every track (%d steps)", gates);
        }
        peak = play(in, &ppq, 120, 4.0, 1, &rms, NULL);
        CHECK(peak == peak && peak < 1.f && rms > 0.005, "the random project plays (peak %f rms %f)", peak, rms);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        set(in, "shift", "1"); set(in, "mute", "1");   /* the factory project again */
    }
    /* ---- the worst case's time: eight tracks of the heaviest voices, unison, FM, the swarm */
    {
        static const int heavy[8] = {ENG_MACRO, ENG_PHYS, ENG_FM6, ENG_GRAIN, ENG_WHEEL, ENG_WAVE, ENG_VOICE, ENG_TRIO};
        int t;
        struct timespec t0, t1;
        double us;
        for (t = 0; t < NUM_TRACKS; t++) {
            Sound *s = &x->p.snd[t];
            sound_setVoice(s, heavy[t]);
            s->v[SP_SW_MODE] = 1; s->v[SP_SPREAD] = 0.6f; s->v[SP_SW_MIX] = 0.8f;
            s->v[SP_FM] = 0.4f; s->v[SP_SUB] = 0.3f; s->v[SP_NOISE] = 0.1f; s->v[SP_VCA_DEC] = 0.f;
            s->v[SP_V2] = 1.f;   /* GRAIN: dense */
            voice_noteOn(&x->v[t], s, 48.f + t * 3, 0.8f, 0, 0, 0, 1);
        }
        setf(in, "dly_wet", 0.5f);
        set(in, "gd_on", "1");
        clock_gettime(CLOCK_MONOTONIC, &t0);
        {
            int16_t b[256];
            int k2;
            for (k2 = 0; k2 < 1000; k2++) E->render(in, b, 128);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        us = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / 1000.0 / 1000.0;
        printf("worst case: %.1f us per 128-frame block (this machine)\n", us);
        for (t = 0; t < NUM_TRACKS; t++) voice_noteOff(&x->v[t]);
        set(in, "gd_on", "1");
        set(in, "shift", "1"); set(in, "mute", "1");
    }

    /* ---- presets: a sound and a performance saved to the storage and loaded back */
    {
        char dir[256], path[400], b[128] = "";
        struct stat st;
        void *ip;
        Bl *y;
        double q = 0;
        int w;
        snprintf(dir, sizeof dir, "/tmp/zy_presets_%d", (int)getpid());
        ip = E->create(dir);
        y = (Bl *)ip;
        CHECK(!strcmp(y->dataDir, dir), "the preset folder is the data folder ('%s')", y->dataDir);
        set(ip, "trk3", "1");
        setf(ip, "cutoff", 0.123f);
        set(ip, "pr_sound", "1");
        set(ip, "pr_slot", "4");
        set(ip, "pr_load", "1");
        E->get_param(ip, "readout", b, sizeof b);
        CHECK(strstr(b, "SOUND 05") && strstr(b, "EMPTY"), "an empty slot says so ('%s')", b);
        set(ip, "pr_save", "1");
        E->get_param(ip, "readout", b, sizeof b);
        CHECK(strstr(b, "TAP SAVE AGAIN") != NULL, "SAVE asks for a second tap ('%s')", b);
        set(ip, "pr_save", "1");
        snprintf(path, sizeof path, "%s/Sounds/SOUND 05.zysound", dir);
        for (w = 0; w < 200 && stat(path, &st) != 0; w++) usleep(10000);
        CHECK(stat(path, &st) == 0 && st.st_size > 100, "SOUND 05 written (%s)", path);
        setf(ip, "cutoff", 0.9f);
        set(ip, "trk5", "1");
        set(ip, "pr_load", "1");   /* a sound loads at once, onto the selected track (5) */
        for (w = 0; w < 200 && y->pr.loading; w++) { play(ip, &q, 120, 0.01, 0, &rms, NULL); usleep(5000); }
        CHECK(fabsf(y->p.snd[4].v[SP_CUTOFF] - 0.123f) < 1e-4f, "SOUND 05 loaded on track 5 (cutoff %f)", y->p.snd[4].v[SP_CUTOFF]);
        CHECK(fabsf(y->p.snd[2].v[SP_CUTOFF] - 0.9f) < 1e-4f, "track 3 untouched (%f)", y->p.snd[2].v[SP_CUTOFF]);
        E->get_param(ip, "readout", b, sizeof b);
        CHECK(strstr(b, "LOADED SOUND 05 ON T5") != NULL, "the readout: '%s'", b);
        /* a performance: everything */
        set(ip, "pr_perf", "1");
        set(ip, "pr_slot", "0");
        setf(ip, "mix_master", 0.33f);
        set(ip, "pr_save", "1"); set(ip, "pr_save", "1");
        snprintf(path, sizeof path, "%s/Performances/PERF 01.zyperf", dir);
        for (w = 0; w < 200 && stat(path, &st) != 0; w++) usleep(10000);
        CHECK(stat(path, &st) == 0 && st.st_size > 1000, "PERF 01 written");
        setf(ip, "mix_master", 0.7f);
        y->p.snd[4].v[SP_CUTOFF] = 0.5f;
        set(ip, "pr_load", "1");
        CHECK(y->pr.armed == PR_LOAD && !y->pr.loading, "LOAD PERF asks for a second tap");
        set(ip, "pr_load", "1");
        for (w = 0; w < 200 && y->pr.loading; w++) { play(ip, &q, 120, 0.01, 0, &rms, NULL); usleep(5000); }
        CHECK(fabsf(y->p.master - 0.33f) < 1e-4f && fabsf(y->p.snd[4].v[SP_CUTOFF] - 0.123f) < 1e-4f,
              "PERF 01 loaded (master %f, T5 cutoff %f)", y->p.master, y->p.snd[4].v[SP_CUTOFF]);
        E->get_param(ip, "pr_sound_on", b, sizeof b);
        CHECK(atof(b) == 0, "PERF lit, SOUND not");
        /* a load still in flight when the instance goes away */
        set(ip, "pr_load", "1"); set(ip, "pr_load", "1");
        E->destroy(ip);
        usleep(100000);
        snprintf(path, sizeof path, "rm -rf '%s'", dir);
        if (system(path)) {}
    }

    E->destroy(in);
    printf(fails ? "engine_test: %d FAILED\n" : "engine_test: PASSED\n", fails);
    return fails != 0;
}
