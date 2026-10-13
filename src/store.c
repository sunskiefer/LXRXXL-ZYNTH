/* LXRXXL ZYNTH -- the state chunk MPC's project keeps: the whole project (sounds, 64 patterns, delay, gate/duck,
 * tempo) as text: a header line, then the project's bytes in hex. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bl.h"
#include "param_map.h"

#define MAGIC "LXRXXL-ZYNTH"

char *store_serialize(Bl *x, int *len)
{
    const size_t n = sizeof(Project);
    const uint8_t *b = (const uint8_t *)&x->p;
    static const char hex[] = "0123456789abcdef";
    char *s = malloc(n * 2 + 128);
    size_t i;
    int h;
    if (!s) return NULL;
    h = snprintf(s, 128, "%s 1 %u %u %u\n", MAGIC, (unsigned)n, (unsigned)x->patSlot, (unsigned)x->ui.track);
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
    size_t i;
    int t;
    if (!text || strncmp(text, MAGIC " ", strlen(MAGIC) + 1)) return 0;
    if (sscanf(text + strlen(MAGIC) + 1, "%u %u %u %u", &ver, &size, &pat, &track) != 4) return 0;
    /* 1: this layout */
    if (!(ver == 1 && size == sizeof(Project))) return 0;
    p = strchr(text, '\n');
    if (!p) return 0;
    p++;
    if (strlen(p) < 2 * (size_t)size) return 0;
    np = malloc(sizeof *np);
    if (!np) return 0;
    for (i = 0; i < size; i++) {
        const int a = nib(p[2 * i]), b = nib(p[2 * i + 1]);
        if (a < 0 || b < 0) { free(np); return 0; }
        ((uint8_t *)np)[i] = (uint8_t)(a << 4 | b);
    }
    /* sanity: values in range */
    for (t = 0; t < NUM_TRACKS; t++) {
        int k;
        for (k = 0; k < SP_COUNT; k++) {
            const float mx = SOUND_DEFS[k].nopts ? (float)(SOUND_DEFS[k].nopts - 1) : 1.f;
            if (!(np->snd[t].v[k] >= 0.f && np->snd[t].v[k] <= mx)) np->snd[t].v[k] = SOUND_DEFS[k].def;
        }
        for (k = 0; k < NUM_ENGINES; k++) {
            int j;
            for (j = 0; j < ENG_SLIDERS; j++)
                if (!(np->snd[t].mem[k][j] >= 0.f && np->snd[t].mem[k][j] <= 1.f)) np->snd[t].mem[k][j] = ENG_DEFS[k].def[j];
        }
        sound_sync(&np->snd[t]);
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
