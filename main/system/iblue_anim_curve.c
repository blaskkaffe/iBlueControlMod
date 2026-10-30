/* iBlueControlMod: boot animation maths (water ripples). See iblue_anim_curve.h. */

#include <math.h>
#include "system/iblue_anim_curve.h"

#define GAMMA 2.2f          /* LED brightness is not linear in duty */
#define WIDTH 4.0f          /* the tank: LED i sits at i + 0.5, the walls at 0 and 4 */
#define SPEED 1.6f          /* ripple speed, LED spacings per second */
#define WAVELENGTH 2.4f     /* crest to crest */
#define FRONT 0.45f         /* how sharp the leading edge is */
#define TRAIL 1.7f         /* how long the trailing ripples last behind the front */
#define NEAR 3.0f          /* distance from which the amplitude falls with the square root of it */
#define DAMPING_S 4.5f      /* the ripples die out with this time constant */
#define REST 0.20f          /* brightness of calm water (the LEDs glow a little, a trough is dark) */
#define GAIN 1.7f          /* the ripples are drawn a bit larger than life */
#define SPLASH_S 0.18f      /* time for the splash to build up */
#define TWO_PI 6.2831853f
#define IMAGES 2            /* mirror images of a drop on each side: the walls */

struct drop {
    float at;   /* time of the impact, fraction of the animation */
    float x;    /* where, 0..WIDTH */
    float amp;
};

/* The first one lands in the middle and is the big one; two smaller ones follow */
static const struct drop drops[] = {
    {0.02f, 2.00f, 1.00f},
    {0.33f, 0.85f, 0.90f},
    {0.62f, 3.20f, 0.95f},
};

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth(float x) { x = clamp01(x); return x * x * (3.0f - 2.0f * x); }

/* Distance to the impact, rounded off so the water has no sharp point right where the drop hits */
static float dist(float d) { return sqrtf(d * d + 0.04f); }

/* Height of one ring ripple at distance r, `age` seconds after the impact. s is how far the point
 * is behind the front (negative: the front has not arrived yet). */
static float ripple(float r, float age) {
    float s = r - SPEED * age;
    float env = s > 0.0f ? expf(-(s / FRONT) * (s / FRONT)) : expf(-(s / TRAIL) * (s / TRAIL));
    float spread = 1.0f / sqrtf(1.0f + r / NEAR);
    /* the splash builds up over a moment, so the water does not jump at the impact */
    return cosf(TWO_PI * s / WAVELENGTH) * env * spread * expf(-age / DAMPING_S) * smooth(age / SPLASH_S);
}

/* Height of the water at x, t seconds into the animation */
static float height(float x, float t, float total_s) {
    float h = 0.0f;

    for (unsigned i = 0; i < sizeof(drops) / sizeof(drops[0]); i++) {
        float age = t - drops[i].at * total_s;

        if (age < 0.0f) {
            continue;
        }
        /* the drop and its mirror images in the two walls */
        for (int k = -IMAGES; k <= IMAGES; k++) {
            float a = 2.0f * WIDTH * (float)k + drops[i].x;
            float b = 2.0f * WIDTH * (float)k - drops[i].x;

            h += drops[i].amp * ripple(dist(x - a), age);
            h += drops[i].amp * ripple(dist(x - b), age);
        }
    }
    return h;
}

float iblue_anim_height(float x, uint32_t elapsed_ms, uint32_t total_ms) {
    return height(x, (float)elapsed_ms / 1000.0f, (float)total_ms / 1000.0f);
}

uint32_t iblue_anim_duty(uint32_t elapsed_ms, uint32_t total_ms, int led) {
    float t, v, env;

    if (total_ms == 0 || elapsed_ms >= total_ms || led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    t = (float)elapsed_ms / 1000.0f;
    /* height of the water is the brightness: calm water glows a little, a crest is bright, a trough dark */
    v = clamp01(REST + (1.0f - REST) * GAIN * height((float)led + 0.5f, t, (float)total_ms / 1000.0f));
    /* the water settles in over the first 0.35 s and is eased out over the last 8 % */
    env = smooth(t / 0.35f) * smooth(((float)total_ms - (float)elapsed_ms) / (0.08f * (float)total_ms));
    return (uint32_t)(powf(v * env, GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}
