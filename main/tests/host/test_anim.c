/* Host test of the boot animation maths: see run.sh */
#include <stdio.h>
#include "../../system/iblue_anim_curve.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void) {
    const uint32_t total = 6000;

    CHECK(iblue_anim_cycles(6000) == 4 && iblue_anim_cycles(100) == 1 && iblue_anim_cycles(0) == 1);
    for (int style = 0; style < IBLUE_ANIM_STYLES; style++) {
        uint32_t peak[4] = {0}, prev[4] = {0}, maxstep = 0;
        int lit_at_end = 0;

        for (int led = 0; led < 4; led++) {
            CHECK(iblue_anim_duty(style, total, total, led) == 0);
            CHECK(iblue_anim_duty(style, total + 500, total, led) == 0);
            CHECK(iblue_anim_duty(style, 0, total, led) == 0);
            CHECK(iblue_anim_duty(style, 0, 0, led) == 0);
        }
        CHECK(iblue_anim_duty(style, 100, total, 4) == 0 && iblue_anim_duty(style, 100, total, -1) == 0);
        for (uint32_t ms = 0; ms < total; ms += 10) {
            for (int led = 0; led < 4; led++) {
                uint32_t d = iblue_anim_duty(style, ms, total, led);
                uint32_t step = d > prev[led] ? d - prev[led] : prev[led] - d;
                CHECK(d <= IBLUE_ANIM_MAX);
                if (ms > 0 && step > maxstep) maxstep = step;
                if (d > peak[led]) peak[led] = d;
                prev[led] = d;
                if (ms > total - 100 && d > 150) lit_at_end++;
            }
        }
        /* every LED gets bright at some point, nothing jumps by more than a third of the range in 10 ms,
         * and it has faded out before the end */
        for (int led = 0; led < 4; led++) CHECK(peak[led] > 900);
        CHECK(maxstep < IBLUE_ANIM_MAX / 3);
        CHECK(lit_at_end == 0);
    }
    /* the scan runs A to D and back: D is the light at a half sweep (750 ms), A again after a whole one (1500 ms) */
    CHECK(iblue_anim_duty(IBLUE_ANIM_SCAN, 750, total, 3) > 900 && iblue_anim_duty(IBLUE_ANIM_SCAN, 750, total, 0) < 100);
    CHECK(iblue_anim_duty(IBLUE_ANIM_SCAN, 1500, total, 0) > 900 && iblue_anim_duty(IBLUE_ANIM_SCAN, 1500, total, 3) < 100);
    CHECK(iblue_anim_duty(IBLUE_ANIM_SCAN, 375, total, 1) > iblue_anim_duty(IBLUE_ANIM_SCAN, 375, total, 3));

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
