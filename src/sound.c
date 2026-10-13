/* LXRXXL ZYNTH -- a track's sound: the voice it plays and the six sliders of that voice.
 *
 * Each track remembers every voice's six sliders: switching FM6 -> GRAIN -> FM6 finds the FM6 sound as it was left.
 * v[SP_V1..SP_V6] always hold the sliders of the voice in use (what the MODULES page, MPC and the presets see);
 * mem[voice] keeps them for when that voice is picked again. */
#include <string.h>
#include "bl.h"
#include "param_map.h"

static int cur(const Sound *s)
{
    int t = (int)(s->v[SP_VOICE] + 0.5f);
    return t < 0 ? 0 : t >= NUM_ENGINES ? NUM_ENGINES - 1 : t;
}

void sound_setVoice(Sound *s, int type)
{
    int k;
    const int now = cur(s);
    if (type < 0) type = 0;
    if (type >= NUM_ENGINES) type = NUM_ENGINES - 1;
    for (k = 0; k < ENG_SLIDERS; k++) s->mem[now][k] = s->v[SP_V1 + k];
    s->v[SP_VOICE] = (float)type;
    for (k = 0; k < ENG_SLIDERS; k++) s->v[SP_V1 + k] = s->mem[type][k];
}

void sound_setSlider(Sound *s, int k, float v)
{
    if (k < 0 || k >= ENG_SLIDERS) return;
    v = clampf(v, 0, 1);
    s->v[SP_V1 + k] = v;
    s->mem[cur(s)][k] = v;
}

void sound_resetVoice(Sound *s, int type)
{
    int k;
    for (k = 0; k < ENG_SLIDERS; k++) s->mem[type][k] = ENG_DEFS[type].def[k];
    if (cur(s) == type)
        for (k = 0; k < ENG_SLIDERS; k++) s->v[SP_V1 + k] = s->mem[type][k];
}

/* the sliders in v[] written back into the voice's memory (after presets, the randomizer, the factory set them) */
void sound_sync(Sound *s)
{
    int k;
    for (k = 0; k < ENG_SLIDERS; k++) s->mem[cur(s)][k] = s->v[SP_V1 + k];
}

void init_sound(Sound *s)
{
    int i, e;
    for (i = 0; i < SP_COUNT; i++) s->v[i] = SOUND_DEFS[i].def;
    for (e = 0; e < NUM_ENGINES; e++)
        for (i = 0; i < ENG_SLIDERS; i++) s->mem[e][i] = ENG_DEFS[e].def[i];
    for (i = 0; i < ENG_SLIDERS; i++) s->v[SP_V1 + i] = s->mem[cur(s)][i];
}
