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
        /* 0.2.0: the gate/duck is gone: a note on channel 10 or in the lowest octave ducks nothing */
        set(in, "gd_on", "1");
        CHECK(x->p.gdOn == 1, "(the old key still toggles its flag)");
        {
            uint8_t duck[3] = {0x99, 36, 100}, low[3] = {0x90, 5, 100};
            E->midi(in, duck, 3);
            E->midi(in, low, 3);
        }
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);
        r2 = (float)rms;
        CHECK(r2 > r1 * 0.6f, "nothing ducks any more (%f -> %f)", r1, r2);
        E->midi(in, off, 3);
        {
            uint8_t low0[3] = {0x80, 5, 0};
            E->midi(in, low0, 3);
        }
        set(in, "gd_on", "1");
    }

    /* ---- CHAOS: every track its own motion; SHIFT: a partial moved by the set number of Hz */
    {
        int t, same = 0;
        float a0[NUM_TRACKS];
        setf(in, "chaos_depth", 1.f);
        setf(in, "chaos_wild", 1.f);
        setf(in, "chaos_rate", 0.7f);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        for (t = 0; t < NUM_TRACKS; t++) a0[t] = x->v[t].chaos;
        for (t = 1; t < NUM_TRACKS; t++) same += fabsf(a0[t] - a0[0]) < 1e-4f;
        CHECK(same == 0, "CHAOS: each track moves on its own (%d alike)", same);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        CHECK(fabsf(x->v[0].chaos - a0[0]) > 1e-4f, "CHAOS moves (%f -> %f)", a0[0], x->v[0].chaos);
        setf(in, "mix_chs1", 0.f);
        play(in, &ppq, 120, 0.05, 0, &rms, NULL);
        CHECK(x->v[0].chaos == 0.f, "a track's CHAOS amount at 0: still (%f)", x->v[0].chaos);
        setf(in, "mix_chs1", 1.f);
        for (t = 0; t < 5; t++) {   /* every destination, played: no NaN, no blow-up */
            uint8_t on2[3] = {0x90, 48, 100}, off2[3] = {0x80, 48, 0};
            char b2[8];
            snprintf(b2, sizeof b2, "%d", t);
            set(in, "chaos_dest", b2);
            E->midi(in, on2, 3);
            peak = play(in, &ppq, 120, 0.6, 0, &rms, NULL);
            E->midi(in, off2, 3);
            CHECK(peak == peak && peak < 1.f && rms > 0.003, "CHAOS > %s plays (peak %f rms %f)", b2, peak, rms);
        }
        setf(in, "chaos_depth", 0.f);
        play(in, &ppq, 120, 0.3, 0, &rms, NULL);
        CHECK(x->v[0].chaos == 0.f, "CHAOS at 0: nothing moves");
        /* SHIFT, measured on the bus alone: a 1 kHz sine shifted up 250 Hz comes out at 1250 Hz, not 750 */
        {
            static float inL[128], inR[128], oL[128], oR[128];
            double ph = 0, c1250r = 0, c1250i = 0, c750r = 0, c750i = 0, c1000r = 0, c1000i = 0;
            int blk, k2;
            float want = 0.5f + 0.5f * log2f(251.f) / 11.f;   /* SHIFT FREQ's knob at +250 Hz */
            alien_init(&x->al);
            setf(in, "shift_mix", 1.f);
            setf(in, "shift_fb", 0.f);
            set(in, "shift_mode", "0");
            setf(in, "shift_freq", want);
            CHECK(fabsf(alien_shiftHz(x->p.g[G_SHIFT_FREQ]) - 250.f) < 1.f, "SHIFT FREQ's scale (%f Hz)", alien_shiftHz(x->p.g[G_SHIFT_FREQ]));
            for (blk = 0; blk < 200; blk++) {
                for (k2 = 0; k2 < 128; k2++) { inL[k2] = inR[k2] = 0.5f * sinf((float)(6.283185307 * ph)); ph += 1000.0 / 44100.0; }
                memset(oL, 0, sizeof oL); memset(oR, 0, sizeof oR);
                alien_shift(x, inL, inR, oL, oR, 128);
                if (blk >= 40)
                    for (k2 = 0; k2 < 128; k2++) {
                        const double tt = (blk * 128 + k2) / 44100.0;
                        c1250r += oL[k2] * cos(6.283185307 * 1250 * tt); c1250i += oL[k2] * sin(6.283185307 * 1250 * tt);
                        c750r += oL[k2] * cos(6.283185307 * 750 * tt); c750i += oL[k2] * sin(6.283185307 * 750 * tt);
                        c1000r += oL[k2] * cos(6.283185307 * 1000 * tt); c1000i += oL[k2] * sin(6.283185307 * 1000 * tt);
                    }
            }
            {
                const double up = hypot(c1250r, c1250i), dn = hypot(c750r, c750i), st = hypot(c1000r, c1000i);
                printf("SHIFT +250 Hz on 1 kHz: 1250 %.1f, 750 %.1f, 1000 %.1f\n", up, dn, st);
                CHECK(up > 20.0 * dn && up > 20.0 * st, "SHIFT: 1 kHz up 250 Hz is 1250 Hz (1250 %.1f, 750 %.1f, 1000 %.1f)", up, dn, st);
            }
            setf(in, "shift_fb", 0.9f);
            set(in, "shift_mode", "1");
            {
                uint8_t on2[3] = {0x90, 60, 100}, off2[3] = {0x80, 60, 0};
                E->midi(in, on2, 3);
                peak = play(in, &ppq, 120, 1.0, 0, &rms, NULL);
                E->midi(in, off2, 3);
                CHECK(peak == peak && peak < 1.f && rms > 0.003, "RING with FEEDBACK plays, stays bounded (peak %f rms %f)", peak, rms);
            }
            setf(in, "shift_mix", 0.f);
            setf(in, "shift_fb", 0.f);
            set(in, "shift_mode", "0");
        }
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
        {
            float home[SP_COUNT];
            sound_home((int)x->p.snd[1].v[SP_VOICE], home);
            CHECK(fabsf(x->p.snd[1].v[SP_CUTOFF] - home[SP_CUTOFF]) < 1e-6f, "VOICE RESET: track 2's cutoff back home (%f)", x->p.snd[1].v[SP_CUTOFF]);
        }
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_FM6, "VOICE RESET keeps the voice (%g)", x->p.snd[1].v[SP_VOICE]);
        set(in, "shift", "1"); set(in, "v_reset", "1");
        CHECK(x->p.mutes == 0xff, "SHIFT + VOICE RESET mutes every track (%02x)", x->p.mutes);
        set(in, "shift", "1"); set(in, "v_reset", "1");
        CHECK(x->p.mutes == 0, "and again: every track back (%02x)", x->p.mutes);
        set(in, "trk1", "1");
    }

    /* ---- every voice's eight starter sounds play (SOUND < > on the VOICE page), at a sensible level */
    {
        uint8_t on[3] = {0x90, 52, 100}, off[3] = {0x80, 52, 0};
        int ty, si;
        char b2[96] = "";
        set(in, "trk1", "1");
        set(in, "shift", "1"); set(in, "mute", "1");   /* the factory project */
        x->p.mutes = 0;
        setf(in, "dly_wet", 0.f);
        for (ty = 0; ty < NUM_ENGINES; ty++) {
            snprintf(key, sizeof key, "%d", ty);
            set(in, "voice", key);
            for (si = 0; si < NUM_STARTER_SOUNDS; si++) {
                if (si) set(in, "snd_next", "1");
                else { set(in, "v_reset", "1"); }
                CHECK(x->p.starter[0][ty] == si, "SOUND > steps (%d)", x->p.starter[0][ty]);
                E->get_param(in, "snd_name", b2, sizeof b2);
                CHECK(b2[0] && !strcmp(b2, sound_starterName(ty, si)), "the SOUND name shows ('%s')", b2);
                E->midi(in, on, 3);
                peak = play(in, &ppq, 120, 0.7, 0, &rms, NULL);
                E->midi(in, off, 3);
                if (!(rms > 0.004 && peak < 0.999f && peak == peak))
                    printf("starter %s %s: peak %.3f rms %.4f\n", ENG_DEFS[ty].name, b2, peak, rms);
                CHECK(rms > 0.004 && peak < 0.999f && peak == peak, "%s %s plays (peak %f rms %f)", ENG_DEFS[ty].name, b2, peak, rms);
                play(in, &ppq, 120, 0.6, 0, &rms, NULL);
            }
            set(in, "snd_next", "1");
            CHECK(x->p.starter[0][ty] == 0, "SOUND > round");
        }
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
    /* ---- VOICE, V1-V6 and each voice's whole sound; home sounds; VOICE RESET; VOICE < > */
    {
        char b[128] = "";
        float home[SP_COUNT];
        set(in, "trk2", "1");
        setf(in, "level", 0.66f);
        setf(in, "pan", 0.3f);
        set(in, "voice", "2");   /* FM6 */
        setf(in, "v1", 0.9f);
        setf(in, "cutoff", 0.21f);
        setf(in, "vca_env", 0.7f);
        set(in, "voice", "11");  /* GRAIN: its home sound, not FM6's chain */
        sound_home(ENG_GRAIN, home);
        CHECK(fabsf(x->p.snd[1].v[SP_V1] - ENG_DEFS[ENG_GRAIN].def[0]) < 1e-5f, "GRAIN starts at its defaults (%f)", x->p.snd[1].v[SP_V1]);
        CHECK(fabsf(x->p.snd[1].v[SP_CUTOFF] - home[SP_CUTOFF]) < 1e-5f, "GRAIN starts with its home chain (cutoff %f)", x->p.snd[1].v[SP_CUTOFF]);
        CHECK(x->p.snd[1].v[SP_SUSTAIN] > 0.99f && x->p.snd[1].v[SP_VCA_ATK] > 0.2f, "GRAIN's home is held (SUSTAIN), with an attack");
        CHECK(fabsf(x->p.snd[1].v[SP_LEVEL] - 0.66f) < 1e-5f && fabsf(x->p.snd[1].v[SP_PAN] - 0.3f) < 1e-5f, "the level and pan stay the track's");
        setf(in, "v1", 0.1f);
        set(in, "voice", "2");
        CHECK(fabsf(x->p.snd[1].v[SP_V1] - 0.9f) < 1e-5f, "back on FM6: its V1 as left (%f)", x->p.snd[1].v[SP_V1]);
        CHECK(fabsf(x->p.snd[1].v[SP_CUTOFF] - 0.21f) < 1e-5f && fabsf(x->p.snd[1].v[SP_VCA_DEC] - 0.7f) < 1e-5f,
              "back on FM6: its chain as left (cutoff %f)", x->p.snd[1].v[SP_CUTOFF]);
        CHECK(fabsf(x->p.snd[1].full[ENG_GRAIN][SP_V1] - 0.1f) < 1e-5f, "GRAIN's sound remembered (%f)", x->p.snd[1].full[ENG_GRAIN][SP_V1]);
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "VOICE") && strstr(b, "FM6"), "the readout names the voice ('%s')", b);
        setf(in, "v3", 0.5f);
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "FM6 MOD INDEX") != NULL, "the readout names the voice's slider ('%s')", b);
        set(in, "v_reset", "1");
        sound_home(ENG_FM6, home);
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_FM6 && fabsf(x->p.snd[1].v[SP_CUTOFF] - home[SP_CUTOFF]) < 1e-5f &&
              fabsf(x->p.snd[1].v[SP_V1] - ENG_DEFS[ENG_FM6].def[0]) < 1e-5f, "VOICE RESET: FM6's home sound");
        CHECK(fabsf(x->p.snd[1].v[SP_LEVEL] - 0.66f) < 1e-5f, "VOICE RESET keeps the level");
        CHECK(fabsf(x->p.snd[1].full[ENG_GRAIN][SP_V1] - 0.1f) < 1e-5f, "VOICE RESET leaves the other voices' sounds");
        set(in, "v_next", "1");
        CHECK((int)x->p.snd[1].v[SP_VOICE] == ENG_FM4, "VOICE > (%g)", x->p.snd[1].v[SP_VOICE]);
        set(in, "v_prev", "1"); set(in, "v_prev", "1"); set(in, "v_prev", "1"); set(in, "v_prev", "1");
        CHECK((int)x->p.snd[1].v[SP_VOICE] == NUM_ENGINES - 1, "VOICE < round to the last voice (%g)", x->p.snd[1].v[SP_VOICE]);
        CHECK((int)getf(in, "voice") == NUM_ENGINES - 1, "MPC reads the voice (%g)", getf(in, "voice"));
        set(in, "voice", "15");
        CHECK(x->p.snd[1].v[SP_VCA_DEC] > 0.3f && x->p.snd[1].v[SP_RESO] > 0.4f, "BAZZ's home: BAZZLINE's bass");
        set(in, "trk1", "1");
    }
    /* ---- TIMBRE and MORPH: the voice's two sliders that change it most */
    {
        char b[128] = "";
        set(in, "trk3", "1");
        set(in, "voice", "2");   /* FM6: MOD INDEX, FEEDBACK */
        setf(in, "timbre", 0.77f);
        setf(in, "morph", 0.33f);
        CHECK(fabsf(x->p.snd[2].v[SP_V3] - 0.77f) < 1e-5f && fabsf(x->p.snd[2].v[SP_V4] - 0.33f) < 1e-5f, "FM6: TIMBRE = MOD INDEX, MORPH = FEEDBACK");
        CHECK(fabsf(getf(in, "timbre") - 0.77f) < 1e-4f, "MPC reads TIMBRE (%f)", getf(in, "timbre"));
        E->get_param(in, "readout", b, sizeof b);
        CHECK(strstr(b, "MORPH") && strstr(b, "FEEDBACK"), "the readout names MORPH's slider ('%s')", b);
        set(in, "voice", "11");  /* GRAIN: DENSITY, POSITION */
        CHECK(fabsf(getf(in, "timbre") - x->p.snd[2].v[SP_V2]) < 1e-4f, "TIMBRE follows the voice");
        set(in, "trk1", "1");
    }
    /* ---- a 0.1.0 project (state version 1) loads: its sounds, the voices' sliders, the patterns */
    {
        typedef struct { float v[SP_COUNT_V1]; float mem[NUM_ENGINES_V1][ENG_SLIDERS_V1]; } SoundV1;
        typedef struct {
            SoundV1 snd[NUM_TRACKS]; Pattern pats[NUM_PATTERNS]; float g[G_COUNT_V1]; float bpm;
            uint8_t links, mutes, gdOn, dlySync; float dlyTap; uint8_t pad[4]; float send[NUM_TRACKS]; float master;
            uint8_t solos; uint8_t pad2[3];
        } ProjectV1;
        static ProjectV1 o;
        static char text[2 * sizeof(ProjectV1) + 128];
        static const char hex[] = "0123456789abcdef";
        void *in3 = E->create(NULL);
        Bl *z = (Bl *)in3;
        int t, e, k, h;
        size_t i;
        memset(&o, 0, sizeof o);
        for (t = 0; t < NUM_TRACKS; t++) {
            for (k = 0; k < SP_COUNT_V1; k++) o.snd[t].v[k] = SOUND_DEFS[k].def;
            o.snd[t].v[SP_VCA_DEC] = t & 1 ? 0.5f : 0.f;
            o.snd[t].v[SP_VOICE] = (float)(t * 2);
            o.snd[t].v[SP_CUTOFF] = 0.1f * t;
            for (e = 0; e < NUM_ENGINES_V1; e++) for (k = 0; k < ENG_SLIDERS_V1; k++) o.snd[t].mem[e][k] = 0.01f * (e + k);
        }
        memcpy(o.pats, x->p.pats, sizeof o.pats);
        o.bpm = 133; o.master = 0.7f; o.mutes = 5;
        for (k = 0; k < G_COUNT_V1; k++) o.g[k] = GLOBAL_DEFS[k].def;
        for (t = 0; t < NUM_TRACKS; t++) o.send[t] = 1.f;
        h = snprintf(text, 128, "LXRXXL-ZYNTH 1 %u 3 2\n", (unsigned)sizeof o);
        for (i = 0; i < sizeof o; i++) { text[h + 2 * i] = hex[((uint8_t *)&o)[i] >> 4]; text[h + 2 * i + 1] = hex[((uint8_t *)&o)[i] & 15]; }
        text[h + 2 * sizeof o] = 0;
        set(in3, "state", text);
        CHECK(z->p.bpm == 133 && z->p.master == 0.7f && z->p.mutes == 5 && z->patSlot == 3 && z->ui.track == 2, "0.1.0: the project's settings");
        CHECK(!memcmp(z->p.pats, x->p.pats, sizeof z->p.pats), "0.1.0: the patterns");
        for (t = 0; t < NUM_TRACKS; t++) {
            CHECK((int)z->p.snd[t].v[SP_VOICE] == t * 2 && fabsf(z->p.snd[t].v[SP_CUTOFF] - 0.1f * t) < 1e-6f, "0.1.0: T%d's sound as it was", t + 1);
            CHECK(fabsf(z->p.snd[t].full[5][SP_V2] - 0.06f) < 1e-6f, "0.1.0: T%d's voices' sliders remembered", t + 1);
            CHECK(z->p.rset[t].lenOpt == 3, "0.1.0: STEP RANDOM's settings at their defaults");
            CHECK(z->p.snd[t].v[SP_SUSTAIN] == (t & 1 ? 0.f : 1.f), "0.1.0: VCA ENV down = SUSTAIN 100 %%, else 0 (T%d)", t + 1);
        }
        E->destroy(in3);
    }
    /* ---- every voice's home sound: held voices keep sounding while the key is, struck ones ring out */
    {
        int ty;
        for (ty = 0; ty < NUM_ENGINES; ty++) {
            float h[SP_COUNT];
            sound_home(ty, h);
            CHECK(h[SP_DRIVE] <= 0.2f && h[SP_PITCH_AMT] == 0.f && h[SP_NOISE] == 0.f && h[SP_FM] == 0.f,
                  "%s's home: a clean chain", ENG_DEFS[ty].name);
            CHECK(ty == ENG_BAZZ || h[SP_CUTOFF] >= 0.7f, "%s's home: the filter open (%f)", ENG_DEFS[ty].name, h[SP_CUTOFF]);
        }
    }
    /* ---- the randomizers */
    {
        Project before = x->p;
        int changed = 0, t, v;
        set(in, "rnd_amt", "10");
        set(in, "shift", "1");
        set(in, "rnd", "1");   /* SHIFT: every track */
        for (t = 0; t < NUM_TRACKS; t++) {
            CHECK(x->p.snd[t].v[SP_VOICE] == before.snd[t].v[SP_VOICE], "SOUND RANDOM keeps the voice (T%d)", t + 1);
            CHECK(x->p.snd[t].v[SP_LEVEL] == before.snd[t].v[SP_LEVEL], "SOUND RANDOM keeps the level (T%d)", t + 1);
            CHECK(x->p.snd[t].v[SP_CUTOFF] >= 0.23f - 1e-6f, "the cutoff stays open enough (T%d %f)", t + 1, x->p.snd[t].v[SP_CUTOFF]);
            for (v = 0; v < SP_COUNT; v++) changed += x->p.snd[t].v[v] != before.snd[t].v[v];
            for (v = 0; v < SP_COUNT; v++)
                CHECK(x->p.snd[t].v[v] == x->p.snd[t].full[(int)x->p.snd[t].v[SP_VOICE]][v], "the sound and the memory agree");
        }
        CHECK(changed > 100, "SOUND RANDOM changes the sounds (%d)", changed);
        before = x->p;
        set(in, "rnd", "1");   /* a tap: the selected track only */
        changed = 0;
        for (t = 1; t < NUM_TRACKS; t++) for (v = 0; v < SP_COUNT; v++) changed += x->p.snd[t].v[v] != before.snd[t].v[v];
        CHECK(changed == 0, "SOUND RANDOM: the selected track only (%d)", changed);
        changed = 0;
        for (v = 0; v < SP_COUNT; v++) changed += x->p.snd[0].v[v] != before.snd[0].v[v];
        CHECK(changed > 5, "SOUND RANDOM: the selected track changed (%d)", changed);
        {   /* C: at a low amount, the harsh controls stay low */
            int k, bad = 0;
            set(in, "rnd_amt", "2");
            for (k = 0; k < 200; k++) {
                set(in, "rnd", "1");
                bad += x->p.snd[0].v[SP_DRIVE] > 0.25f + 0.55f * 0.04f + 1e-4f || x->p.snd[0].v[SP_RESO] > 0.45f + 0.45f * 0.04f + 1e-4f ||
                       x->p.snd[0].v[SP_PITCH_AMT] > 0.04f + 0.4f * 0.04f + 1e-4f;
            }
            CHECK(bad == 0, "SOUND RANDOM at 20 %%: drive, resonance and the pitch envelope held low (%d over)", bad);
        }
        /* STEP RANDOM: a tap does the selected track; the others stay */
        before = x->p;
        set(in, "srnd_amt", "5");
        set(in, "srnd", "1");
        CHECK(!memcmp(&before.pats[x->patSlot].t[1], &bl_pat(x)->t[1], sizeof(TrackPat)), "STEP RANDOM: the other tracks stay");
        {
            int gates = 0;
            for (v = 0; v < 16; v++) gates += (bl_pat(x)->t[0].s[v].f & ST_GATE) != 0;
            CHECK(gates >= 3, "STEP RANDOM: a line on the selected track (%d steps)", gates);
        }
        /* setup: SHIFT + STEP RANDOM; 32 steps, SWELL, ARP; a tap in setup does every track */
        set(in, "shift", "1");
        set(in, "srnd", "1");
        CHECK(x->ui.mode == MD_RNDSET, "SHIFT + STEP RANDOM: the setup");
        set(in, "s6", "1");    /* 32 steps */
        set(in, "s12", "1");   /* SWELL */
        set(in, "s13", "1");   /* CHORD */
        set(in, "s13", "1");   /* ARP */
        CHECK(x->p.rset[0].lenOpt == 5 && x->p.rset[0].vel == RV_SWELL && x->p.rset[0].style == RS_ARP, "the setup's keys (%d %d %d)",
              x->p.rset[0].lenOpt, x->p.rset[0].vel, x->p.rset[0].style);
        set(in, "srnd", "1");
        CHECK(x->ui.mode == MD_NORMAL, "STEP RANDOM in the setup: done, back to the steps");
        CHECK(bl_pat(x)->t[0].len == 32, "STEP RANDOM writes the setup's length (%d)", bl_pat(x)->t[0].len);
        {
            const Harmony *h = &x->p.harm[x->patSlot];
            const uint16_t mask = seq_scaleMask((uint8_t[]){1, 2, 3, 4, 5, 6, 8}[h->mode]);
            int out = 0, gates = 0, risen = 0, prevV = -1;
            CHECK(h->valid, "the pattern has a harmony");
            for (t = 0; t < NUM_TRACKS; t++) {
                const TrackPat *tp = &bl_pat(x)->t[t];
                for (v = 0; v < tp->len; v++) if (tp->s[v].f & ST_GATE) {
                    gates++;
                    if (!tp->scale && !(mask & (1u << ((tp->s[v].note - h->root + 120) % 12)))) out++;
                    CHECK(x->p.vel[x->patSlot][t][v] > 0, "a generated step has its velocity");
                }
            }
            CHECK(gates > 8 * 4, "STEP RANDOM in the setup: every track (%d steps)", gates);
            CHECK(out == 0, "every generated note in the key (%d out)", out);
            for (v = 0; v < 8; v++) {   /* SWELL: rising through the first chord */
                const uint8_t vv = x->p.vel[x->patSlot][0][v];
                if (vv && prevV >= 0 && vv > prevV) risen++;
                if (vv) prevV = vv;
            }
            CHECK(risen > 0, "SWELL: the velocities rise");
        }
        {   /* hundreds of lines: 50 presses, 50 different lines */
            uint32_t seen[50];
            int k, j, dup = 0;
            for (k = 0; k < 50; k++) {
                const TrackPat *tp = &bl_pat(x)->t[0];
                uint32_t hsh = 2166136261u;
                set(in, "srnd", "1");
                for (j = 0; j < tp->len; j++) hsh = (hsh ^ (tp->s[j].note * 4u + (tp->s[j].f & 3))) * 16777619u;
                for (j = 0; j < k; j++) dup += seen[j] == hsh;
                seen[k] = hsh;
            }
            CHECK(dup == 0, "50 presses, 50 different lines (%d repeats)", dup);
        }
        peak = play(in, &ppq, 120, 4.0, 1, &rms, NULL);
        CHECK(peak == peak && peak < 1.f && rms > 0.005, "the random project plays (peak %f rms %f)", peak, rms);
        play(in, &ppq, 120, 0.5, 0, &rms, NULL);
        set(in, "shift", "1"); set(in, "mute", "1");   /* the factory project again */
    }
    /* ---- the worst case's time: eight tracks of the heaviest voices, unison, FM, the swarm */
    {
        static const int heavy[8] = {ENG_MACRO, ENG_PHYS, ENG_FM6, ENG_GRAIN, ENG_DRONE, ENG_WAVE, ENG_TEXTURE, ENG_TRIO};
        int t;
        struct timespec t0, t1;
        double us;
        for (t = 0; t < NUM_TRACKS; t++) {
            Sound *s = &x->p.snd[t];
            sound_setVoice(s, heavy[t]);
            s->v[SP_SW_MODE] = 1; s->v[SP_SPREAD] = 0.6f; s->v[SP_SW_MIX] = 0.8f;
            s->v[SP_FM] = 0.4f; s->v[SP_SUB] = 0.3f; s->v[SP_NOISE] = 0.1f; s->v[SP_VCA_DEC] = 0.f;
            s->v[SP_V2] = 1.f;   /* GRAIN: dense; DRONE: seven oscillators */
            { int k2; for (k2 = 6; k2 < 12; k2++) s->v[SP_V1 + k2] = 0.7f; }   /* the deeper controls on */
            voice_noteOn(&x->v[t], s, 48.f + t * 3, 0.8f, 0, 0, 0, 1);
        }
        setf(in, "dly_wet", 0.5f);
        setf(in, "shift_mix", 0.5f); setf(in, "chaos_depth", 0.5f);   /* SHIFT and CHAOS on too */
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
        setf(in, "shift_mix", 0.f); setf(in, "chaos_depth", 0.f);
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
