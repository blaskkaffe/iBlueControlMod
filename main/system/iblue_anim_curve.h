/*
 * iBlueControlMod: boot animation of the four controller port LEDs, as pure maths.
 * Portable C (no ESP-IDF): the firmware calls it every frame, tests/host/anim_frames.c prints the
 * same frames so the previews are what the LEDs do. Spec in docs/led-behaviour.md.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_ANIM_CURVE_H_
#define _IBLUE_ANIM_CURVE_H_

#include <stdint.h>

#define IBLUE_ANIM_LEDS 4
#define IBLUE_ANIM_MAX 1023 /* duty of a 10 bit PWM */

enum {
    IBLUE_ANIM_SCAN = 0, /* a soft light runs A to D and back (Knight Rider) */
    IBLUE_ANIM_COMET,    /* the same, with a long tail behind a bright head */
    IBLUE_ANIM_FILL,     /* A to D light up one after the other, then go out again from D to A */
    IBLUE_ANIM_WAVE,     /* every LED breathes, a quarter of a breath later than its neighbour */
    IBLUE_ANIM_CENTER,   /* B and C, then A and D, then back in */
    IBLUE_ANIM_STYLES,
};

/* Number of A to D to A sweeps that fit the animation time (at least one). */
int iblue_anim_cycles(uint32_t total_ms);

/* PWM duty 0..IBLUE_ANIM_MAX of one LED (0 = A .. 3 = D) `elapsed_ms` into an animation of
 * `total_ms`. It fades in and out at the ends and is dark from total_ms on. */
uint32_t iblue_anim_duty(int style, uint32_t elapsed_ms, uint32_t total_ms, int led);

#endif /* _IBLUE_ANIM_CURVE_H_ */
