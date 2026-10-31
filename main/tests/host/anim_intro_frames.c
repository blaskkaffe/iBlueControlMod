/* Prints what the port LEDs do from the moment the LED code starts: the intro and then the first seconds of the
 * drops, the way system/iblue_port_led.c plays them (the LED task works in 20 ms frames, the drops come in
 * after the intro with a 150 ms fade-in, brightness capped at the LED_MAX_PCT).
 * One line per `step_ms`: "<ms> d0 d1 d2 d3" (PWM duty 0..1023).
 * cc -o anim_intro_frames anim_intro_frames.c ../../system/iblue_anim_curve.c -lm
 * ./anim_intro_frames <total_ms> <start_ms> <show_ms> <step_ms> */
#include <stdio.h>
#include <stdlib.h>
#include "../../system/iblue_anim_curve.h"

#define FRAME_MS 20
#define FADE_IN_MS 150
#define MAX_PCT 80

int main(int argc, char **argv) {
    uint32_t total = argc > 1 ? (uint32_t)atoi(argv[1]) : 9000;
    uint32_t start = argc > 2 ? (uint32_t)atoi(argv[2]) : 1000;  /* how far into the BIOS animation the LED code starts */
    uint32_t show = argc > 3 ? (uint32_t)atoi(argv[3]) : 5000;
    uint32_t step = argc > 4 ? (uint32_t)atoi(argv[4]) : 10;

    for (uint32_t ms = 0; ms < show; ms += step) {
        uint32_t run = ms / FRAME_MS * FRAME_MS; /* the LED task updates every 20 ms */

        printf("%u", ms);
        for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
            uint32_t duty;

            if (run < IBLUE_ANIM_INTRO_MS) {
                duty = iblue_anim_intro_duty(run, i);
            }
            else {
                uint32_t since = run - IBLUE_ANIM_INTRO_MS;

                duty = iblue_anim_duty(start + run, total, i);
                if (since < FADE_IN_MS) duty = duty * since / FADE_IN_MS;
            }
            printf(" %u", duty * MAX_PCT / 100);
        }
        printf("\n");
    }
    return 0;
}
