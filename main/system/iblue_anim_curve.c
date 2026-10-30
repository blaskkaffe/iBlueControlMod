/* iBlueControlMod: boot animation maths. See iblue_anim_curve.h. */

#include <math.h>
#include "system/iblue_anim_curve.h"

#define SWEEP_MS 1500.0f /* wanted time of one A to D to A sweep */
#define GAMMA 2.2f       /* LED brightness is not linear in duty */

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth(float x) { x = clamp01(x); return x * x * (3.0f - 2.0f * x); }

/* 0 -> 1 -> 0 over x in [0, 1] */
static float tri(float x) {
    x -= floorf(x);
    return x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x;
}

int iblue_anim_cycles(uint32_t total_ms) {
    int n = (int)((float)total_ms / SWEEP_MS + 0.5f);
    return n < 1 ? 1 : n;
}

/* brightness 0..1 of an LED, before gamma and the fade at the ends */
static float level(int style, float t, int cycles, int led) {
    float c = (float)cycles * t; /* sweeps done so far */
    float x = tri(c);            /* 0 at A, 1 at D */
    float rising = (c - floorf(c)) < 0.5f;

    switch (style) {
        case IBLUE_ANIM_COMET: {
            /* head position 0..3; the tail trails behind it, nothing is lit ahead */
            float p = 3.0f * x;
            float behind = rising ? (p - (float)led) : ((float)led - p);
            if (behind < -0.35f) return 0.0f;
            if (behind < 0.0f) return smooth(1.0f + behind / 0.35f);
            return expf(-behind / 0.9f);
        }
        case IBLUE_ANIM_FILL:
            /* each LED goes on while the front passes it and off again on the way back */
            return smooth(3.0f * 1.35f * x - (float)led * 1.0f + 0.15f);
        case IBLUE_ANIM_WAVE:
            return 0.5f - 0.5f * cosf(6.2831853f * (c * 2.0f - (float)led * 0.25f));
        case IBLUE_ANIM_CENTER: {
            float r = fabsf((float)led - 1.5f);       /* 0.5 for B and C, 1.5 for A and D */
            float ring = 2.0f * x;                    /* the ring grows out, then comes back */
            return smooth(1.0f - fabsf(ring - r) / 0.95f);
        }
        case IBLUE_ANIM_SCAN:
        default: {
            float d = fabsf(3.0f * x - (float)led);
            return smooth(1.0f - d / 1.2f);
        }
    }
}

uint32_t iblue_anim_duty(int style, uint32_t elapsed_ms, uint32_t total_ms, int led) {
    float t, env, v;
    int cycles;

    if (total_ms == 0 || elapsed_ms >= total_ms || led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    t = (float)elapsed_ms / (float)total_ms;
    cycles = iblue_anim_cycles(total_ms);
    /* fade in over the first 5 % and out over the last 8 % so it neither pops on nor off */
    env = smooth(t / 0.05f) * smooth((1.0f - t) / 0.08f);
    v = level(style, t, cycles, led) * env;
    return (uint32_t)(powf(clamp01(v), GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}
