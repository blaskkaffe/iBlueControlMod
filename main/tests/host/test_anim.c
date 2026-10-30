/* Host test of the boot animation maths: see run.sh */
#include <stdio.h>
#include <stdlib.h>
#include "../../system/iblue_anim_curve.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t d(uint32_t ms, uint32_t total, int led) { return iblue_anim_duty(ms, total, led); }

int main(void) {
    uint32_t totals[] = {3000, 6000, 9000};

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
        /* every LED gets a bright crest, nothing jumps by more than a third of the range in 10 ms, and it
         * has eased out before the end */
        for (int led = 0; led < 4; led++) CHECK(peak[led] > 900);
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
        CHECK(bb > 900 && ba > 900 && ta > tb + 300);
        /* calm water glows a little, it is not dark: A at 0.6 s is between the crests */
        CHECK(d(5000, total, 1) > 0);
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
