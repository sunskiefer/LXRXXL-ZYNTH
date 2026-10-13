/* LXRXXL ZYNTH -- the state chunk MPC's project keeps: the whole project (sounds, 64 patterns, delay, gate/duck,
 * tempo) as text: a header line, then the project's bytes in hex. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bl.h"
#include "param_map.h"

#define MAGIC "LXRXXL-ZYNTH"

/* 0.1.0's layout (version 1): each voice's six sliders only, no STEP RANDOM settings, no velocities */
typedef struct {
    float v[SP_COUNT_V1];
    float mem[NUM_ENGINES][ENG_SLIDERS];
} SoundV1;
typedef struct {
    SoundV1 snd[NUM_TRACKS];
    Pattern pats[NUM_PATTERNS];
    float g[G_COUNT];
    float bpm;
    uint8_t links, mutes, gdOn, dlySync;
    float dlyTap;
    uint8_t pad[4];
    float send[NUM_TRACKS];
    float master;
    uint8_t solos;
    uint8_t pad2[3];
} ProjectV1;

static void from_v1(Project *np, const ProjectV1 *o)
{
    int t, e, k;
    bl_initProject(np);
    for (t = 0; t < NUM_TRACKS; t++) {
        Sound *s = &np->snd[t];
        for (e = 0; e < NUM_ENGINES; e++)   /* each voice: its home sound with the sliders the track had */
            for (k = 0; k < ENG_SLIDERS; k++) s->full[e][SP_V1 + k] = o->snd[t].mem[e][k];
        memcpy(s->v, o->snd[t].v, sizeof o->snd[t].v);   /* the voice in use: exactly as it was */
        /* 0.1.0's VCA ENV fully down held the note: that is SUSTAIN 100 %; any other decayed it away: SUSTAIN 0 */
        s->v[SP_SUSTAIN] = s->v[SP_VCA_DEC] <= 0.02f ? 1.f : 0.f;
    }
    memcpy(np->pats, o->pats, sizeof np->pats);
    memcpy(np->g, o->g, sizeof np->g);
    np->bpm = o->bpm;
    np->links = o->links; np->mutes = o->mutes; np->gdOn = o->gdOn; np->dlySync = o->dlySync;
    np->dlyTap = o->dlyTap;
    memcpy(np->send, o->send, sizeof np->send);
    np->master = o->master;
    np->solos = o->solos;
}

char *store_serialize(Bl *x, int *len)
{
    const size_t n = sizeof(Project);
    const uint8_t *b = (const uint8_t *)&x->p;
    static const char hex[] = "0123456789abcdef";
    char *s = malloc(n * 2 + 128);
    size_t i;
    int h, t;
    if (!s) return NULL;
    for (t = 0; t < NUM_TRACKS; t++) sound_sync(&x->p.snd[t]);   /* each voice's memory holds the sound in use */
    h = snprintf(s, 128, "%s 2 %u %u %u\n", MAGIC, (unsigned)n, (unsigned)x->patSlot, (unsigned)x->ui.track);
    for (i = 0; i < n; i++) {
        s[h + 2 * i] = hex[b[i] >> 4];
        s[h + 2 * i + 1] = hex[b[i] & 15];
    }
    s[h + 2 * n] = 0;
    *len = (int)(h + 2 * n);
    return s;
}

static int nib(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int store_deserialize(Bl *x, const char *text)
{
    unsigned ver = 0, size = 0, pat = 0, track = 0;
    const char *p;
    Project *np;
    uint8_t *raw;
    size_t i;
    int t;
    if (!text || strncmp(text, MAGIC " ", strlen(MAGIC) + 1)) return 0;
    if (sscanf(text + strlen(MAGIC) + 1, "%u %u %u %u", &ver, &size, &pat, &track) != 4) return 0;
    /* 2: this layout; 1: 0.1.0's, converted */
    if (!((ver == 2 && size == sizeof(Project)) || (ver == 1 && size == sizeof(ProjectV1)))) return 0;
    p = strchr(text, '\n');
    if (!p) return 0;
    p++;
    if (strlen(p) < 2 * (size_t)size) return 0;
    raw = malloc(size);
    np = malloc(sizeof *np);
    if (!np || !raw) { free(np); free(raw); return 0; }
    for (i = 0; i < size; i++) {
        const int a = nib(p[2 * i]), b = nib(p[2 * i + 1]);
        if (a < 0 || b < 0) { free(np); free(raw); return 0; }
        raw[i] = (uint8_t)(a << 4 | b);
    }
    if (ver == 1) from_v1(np, (const ProjectV1 *)raw);
    else memcpy(np, raw, sizeof *np);
    free(raw);
    /* sanity: values in range */
    for (t = 0; t < NUM_TRACKS; t++) {
        int k;
        for (k = 0; k < SP_COUNT; k++) {
            const float mx = SOUND_DEFS[k].nopts ? (float)(SOUND_DEFS[k].nopts - 1) : 1.f;
            if (!(np->snd[t].v[k] >= 0.f && np->snd[t].v[k] <= mx)) np->snd[t].v[k] = SOUND_DEFS[k].def;
        }
        for (k = 0; k < NUM_ENGINES; k++) {
            float home[SP_COUNT];
            int j;
            sound_home(k, home);
            for (j = 0; j < SP_COUNT; j++) {
                const float mx = SOUND_DEFS[j].nopts ? (float)(SOUND_DEFS[j].nopts - 1) : 1.f;
                if (!(np->snd[t].full[k][j] >= 0.f && np->snd[t].full[k][j] <= mx)) np->snd[t].full[k][j] = home[j];
            }
            np->snd[t].full[k][SP_VOICE] = (float)k;
        }
        sound_sync(&np->snd[t]);
        {
            RndSet *r = &np->rset[t];
            if (r->lenOpt > 7) r->lenOpt = 3;
            if (r->vel >= RV_COUNT) r->vel = RV_GROOVE;
            if (r->style >= RS_COUNT) r->style = RS_AUTO;
        }
    }
    for (i = 0; i < NUM_PATTERNS; i++)
        for (t = 0; t < NUM_TRACKS; t++) {
            TrackPat *tp = &np->pats[i].t[t];
            if (tp->len < 1 || tp->len > NUM_STEPS) tp->len = 16;
            if (tp->playMode >= PM_COUNT) tp->playMode = 0;
            if (tp->scale >= NUM_SCALES) tp->scale = 0;
            if (tp->arp >= ARP_COUNT) tp->arp = 0;
            if (tp->transp < -24 || tp->transp > 24) tp->transp = 0;
        }
    for (i = 0; i < NUM_PATTERNS; i++) {
        Harmony *h = &np->harm[i];
        if (h->valid > 1 || h->root > 11 || h->mode >= NUM_HARM_MODES || h->prog >= 32 || h->color > 4) memset(h, 0, sizeof *h);
        {
            uint8_t *vv = &np->vel[i][0][0];
            int k;
            for (k = 0; k < NUM_TRACKS * NUM_STEPS; k++) if (vv[k] > 127) vv[k] = 127;
        }
    }
    for (t = 0; t < G_COUNT; t++) {
        const float mx = GLOBAL_DEFS[t].nopts ? (float)(GLOBAL_DEFS[t].nopts - 1) : 1.f;
        if (!(np->g[t] >= 0.f && np->g[t] <= mx)) np->g[t] = GLOBAL_DEFS[t].def;
    }
    if (np->dlySync > 2) np->dlySync = 0;
    if (!(np->dlyTap >= 0.f && np->dlyTap <= 4.f)) np->dlyTap = 0;
    if (!(np->bpm >= 0.f && np->bpm <= 300.f)) np->bpm = 0;
    for (t = 0; t < NUM_TRACKS; t++) if (!(np->send[t] >= 0.f && np->send[t] <= 1.f)) np->send[t] = 1.f;
    if (!(np->master >= 0.f && np->master <= 1.f)) np->master = 0.8f;
    seq_stop(x);
    x->p = *np;
    free(np);
    x->patSlot = (uint8_t)(pat < NUM_PATTERNS ? pat : 0);
    x->ui.track = (uint8_t)(track < NUM_TRACKS ? track : 0);
    x->ui.mode = MD_NORMAL;
    x->ui.sel = 0;
    x->ui.bar = 0;
    x->nextPat = 0xff;
    for (t = 0; t < NUM_TRACKS; t++) voice_kill(&x->v[t]);
    x->displayRev++;
    return 1;
}
