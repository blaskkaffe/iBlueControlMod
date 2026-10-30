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
#define REST_LOW 0.20f      /* brightness of calm water at the start (a trough is dark) */
#define REST_HIGH 0.42f     /* ... after the water has risen */
#define RISE_AT 0.30f       /* the water starts to rise here, fraction of the animation ... */
#define RISE_LEN 0.20f      /* ... and is up this much later */
#define FADE_AT 0.45f       /* from here the water goes down and the light with it, to nothing at the end */
#define SETTLE 0.16f        /* the light comes in over this part of the animation */
#define GAIN 1.7f           /* the ripples are drawn a bit larger than life */
#define SPLASH_S 0.25f      /* time for the splash to build up */
#define TWO_PI 6.2831853f
#define IMAGES 2            /* mirror images of a drop on each side: the walls */

struct drop {
    float at;   /* time of the impact, seconds */
    float x;    /* where, 0..WIDTH */
    float amp;
    float soft; /* 1 = as the others; more = a broader, slower swell: longer wavelength, wider front */
};

#define FIRST_DROP 0.04f    /* the first drop falls at this fraction of the animation */
#define DROP_GAP_S 1.74f    /* seconds between drops */
#define LAST_DROP 0.75f     /* no new drop after this fraction of the animation */
/* Where the drops fall: the first in the middle, then to either side */
static const float drop_x[] = {2.00f, 0.85f, 3.20f, 1.30f, 2.85f, 0.55f, 3.45f};

/* Drop k of an animation of total_s seconds, 0 if there is no such drop. The first one is the big, slow
 * swell; the others follow at a steady pace. A longer animation just gets more of them. */
static int drop_k(int k, float total_s, struct drop *d) {
    d->at = FIRST_DROP * total_s + (float)k * DROP_GAP_S;
    if (d->at > LAST_DROP * total_s) {
        return 0;
    }
    d->x = drop_x[k % (int)(sizeof(drop_x) / sizeof(drop_x[0]))];
    d->amp = k == 0 ? 0.70f : (k % 2 ? 0.90f : 0.95f);
    d->soft = k == 0 ? 1.5f : 1.0f;
    return 1;
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth(float x) { x = clamp01(x); return x * x * (3.0f - 2.0f * x); }
/* gentler at both ends than smooth() */
static float smoother(float x) { x = clamp01(x); return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }

/* Distance to the impact, rounded off so the water has no sharp point right where the drop hits */
static float dist(float d) { return sqrtf(d * d + 0.04f); }

/* Height of one ring ripple at distance r, `age` seconds after the impact. s is how far the point
 * is behind the front (negative: the front has not arrived yet). */
static float ripple(float r, float age, float soft) {
    float s = r - SPEED * age;
    float front = FRONT * soft;
    float trail = TRAIL * soft;
    float env = s > 0.0f ? expf(-(s / front) * (s / front)) : expf(-(s / trail) * (s / trail));
    float spread = 1.0f / sqrtf(1.0f + r / NEAR);
    /* the splash builds up over a moment, so the water does not jump at the impact */
    return cosf(TWO_PI * s / (WAVELENGTH * soft)) * env * spread * expf(-age / DAMPING_S) * smooth(age / (SPLASH_S * soft));
}

/* Height of the water at x, t seconds into the animation */
static float height(float x, float t, float total_s) {
    float h = 0.0f;
    struct drop dr;

    for (int i = 0; drop_k(i, total_s, &dr); i++) {
        float age = t - dr.at;

        if (age < 0.0f) {
            continue;
        }
        /* the drop and its mirror images in the two walls */
        for (int k = -IMAGES; k <= IMAGES; k++) {
            float a = 2.0f * WIDTH * (float)k + dr.x;
            float b = 2.0f * WIDTH * (float)k - dr.x;

            h += dr.amp * ripple(dist(x - a), age, dr.soft);
            h += dr.amp * ripple(dist(x - b), age, dr.soft);
        }
    }
    return h;
}

float iblue_anim_height(float x, uint32_t elapsed_ms, uint32_t total_ms) {
    return height(x, (float)elapsed_ms / 1000.0f, (float)total_ms / 1000.0f);
}

/* Level of the calm water: REST_LOW at the start, up to REST_HIGH around a third of the way */
static float rest_level(float u) {
    return REST_LOW + (REST_HIGH - REST_LOW) * smoother((u - RISE_AT) / RISE_LEN);
}

/* ... and the whole thing eased in at the start and out at the end */
static float master(float u) {
    return smoother(u / SETTLE) * (1.0f - smoother((u - FADE_AT) / (1.0f - FADE_AT)));
}

float iblue_anim_calm(float u) {
    return rest_level(u) * master(u);
}

uint32_t iblue_anim_duty(uint32_t elapsed_ms, uint32_t total_ms, int led) {
    float u, t, rest, v;

    if (total_ms == 0 || elapsed_ms >= total_ms || led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    u = (float)elapsed_ms / (float)total_ms; /* 0..1 over the animation */
    t = (float)elapsed_ms / 1000.0f;
    rest = rest_level(u);
    /* height of the water is the brightness: calm water glows, a crest is bright, a trough dark */
    v = clamp01(rest + (1.0f - rest) * GAIN * height((float)led + 0.5f, t, (float)total_ms / 1000.0f));
    v *= master(u);
    return (uint32_t)(powf(v, GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}

uint32_t iblue_anim_intro_duty(uint32_t ms, int led) {
    uint32_t fade_at;
    float level;

    if (led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    if (ms < IBLUE_ANIM_INTRO_A_MS) {
        return led == 0 ? IBLUE_ANIM_MAX : 0;
    }
    fade_at = IBLUE_ANIM_INTRO_FADE_AT_MS + (uint32_t)led * IBLUE_ANIM_INTRO_STAGGER_MS;
    if (ms < fade_at) {
        return IBLUE_ANIM_MAX;
    }
    if (ms >= fade_at + IBLUE_ANIM_INTRO_FADE_MS) {
        return 0;
    }
    level = 1.0f - (float)(ms - fade_at) / (float)IBLUE_ANIM_INTRO_FADE_MS; /* what the eye sees: full, then down */
    return (uint32_t)(powf(level, GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}
