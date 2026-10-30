/* Prints the boot animation as frames: one line per frame, "<ms> d0 d1 d2 d3" (PWM duty 0..1023).
 * cc -o anim_frames anim_frames.c ../../system/iblue_anim_curve.c -lm && ./anim_frames <style> <total_ms> <fps> */
#include <stdio.h>
#include <stdlib.h>
#include "../../system/iblue_anim_curve.h"

int main(int argc, char **argv) {
    int style = argc > 1 ? atoi(argv[1]) : 0;
    uint32_t total = argc > 2 ? (uint32_t)atoi(argv[2]) : 6000;
    int fps = argc > 3 ? atoi(argv[3]) : 30;

    for (uint32_t ms = 0; ms < total + 400; ms += 1000 / fps) {
        printf("%u", ms);
        for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
            printf(" %u", iblue_anim_duty(style, ms, total, i));
        }
        printf("\n");
    }
    return 0;
}
