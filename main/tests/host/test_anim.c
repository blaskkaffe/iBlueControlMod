/* Host test of the boot animation maths: see run.sh */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../../system/iblue_anim_curve.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t d(uint32_t ms, uint32_t total, int led) { return iblue_anim_duty(ms, total, led); }

int main(void) {
    uint32_t totals[] = {3000, 6000, 9000, 15000};

    for (unsigned n = 0; n < sizeof(totals) / sizeof(totals[0]); n++) {
        uint32_t total = totals[n], peak[4] = {0}, prev[4] = {0}, maxstep = 0;
        int lit_at_end = 0;

        for (int led = 0; led < 4; led++) {
            CHECK(d(total, total, led) == 0 && d(total + 500, total, led) == 0 && d(0, total, led) == 0 && d(0, 0, led) == 0);
        }
        CHECK(d(100, total, 4) == 0 && d(100, total, -1) == 0);
        for (uint32_t ms = 0; ms < total; ms += 10) {
            for (int led = 0; led < 4; led++) {
                uint32_t v = d(ms, total, led);
                uint32_t step = v > prev[led] ? v - prev[led] : prev[led] - v;
                CHECK(v <= IBLUE_ANIM_MAX);
                if (ms > 0 && step > maxstep) maxstep = step;
                if (v > peak[led]) peak[led] = v;
                prev[led] = v;
                if (ms > total - 100 && v > 150) lit_at_end++;
            }
        }
        /* every LED gets a clear crest, nothing jumps by more than a third of the range in 10 ms, and it
         * has eased out before the end */
        for (int led = 0; led < 4; led++) CHECK(peak[led] > 600);
        CHECK(maxstep < IBLUE_ANIM_MAX / 3);
        CHECK(lit_at_end == 0);
    }
    {
        /* The first drop lands between B and C, so they are equal, and the ripple reaches A and D later. */
        const uint32_t total = 6000;
        uint32_t tb = 0, ta = 0, bb = 0, ba = 0;

        for (uint32_t ms = 150; ms < 3000; ms += 10) {
            CHECK(abs((int)d(ms, total, 1) - (int)d(ms, total, 2)) <= 2 || ms > 1900); /* mirrored until the 2nd drop */
            if (ms < 1900) CHECK(abs((int)d(ms, total, 0) - (int)d(ms, total, 3)) <= 2);
        }
        for (uint32_t ms = 150; ms < 1500; ms += 5) {
            uint32_t vb = d(ms, total, 1), va = d(ms, total, 0);
            if (vb > bb) { bb = vb; tb = ms; }
            if (ms > 300 && va > ba) { ba = va; ta = ms; }
        }
        CHECK(bb > 500 && ba > 500 && ta > tb + 300);
        /* calm water glows a little, it is not dark: A at 0.6 s is between the crests */
        CHECK(d(5000, total, 1) > 0);
    }

    {
        /* The water level over the animation: it comes in at the start level, is raised after about a third,
         * then fades out slowly: every step lower, nothing at the end */
        float prev = 0.0f, peak = 0.0f;
        int fall_started = 0, rises_after_peak = 0;

        CHECK(iblue_anim_calm(0.0f) == 0.0f && iblue_anim_calm(1.0f) == 0.0f);
        CHECK(iblue_anim_calm(0.25f) < 0.21f && iblue_anim_calm(0.25f) > 0.15f);   /* still the start level */
        CHECK(iblue_anim_calm(0.50f) > 0.40f && iblue_anim_calm(0.50f) < 0.43f);   /* raised */
        CHECK(iblue_anim_calm(0.40f) > iblue_anim_calm(0.25f) + 0.08f);            /* the rise is under way after a third */
        for (int i = 0; i <= 1000; i++) {
            float c = iblue_anim_calm((float)i / 1000.0f);
            if (c > peak) peak = c;
            if (fall_started && c > prev + 1e-6f) rises_after_peak++;
            if (c < peak - 0.001f) fall_started = 1;
            prev = c;
        }
        CHECK(rises_after_peak == 0);
        CHECK(iblue_anim_calm(0.75f) < 0.6f * peak && iblue_anim_calm(0.9f) < 0.15f * peak);
    }
    {
        /* gentle start: no LED steps by more than 45/1023 in 10 ms during the first second */
        const uint32_t total = 6000;
        uint32_t maxstep_start = 0, prev0[4] = {0};

        for (uint32_t ms = 0; ms < 1000; ms += 10) {
            for (int led = 0; led < 4; led++) {
                uint32_t v = d(ms, total, led), st = v > prev0[led] ? v - prev0[led] : prev0[led] - v;
                if (ms > 0 && st > maxstep_start) maxstep_start = st;
                prev0[led] = v;
            }
        }
        CHECK(maxstep_start < 45);
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
