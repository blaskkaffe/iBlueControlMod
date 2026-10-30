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

/*
 * Drops falling on water. Take a line through the water: the four LEDs sit on it, A to D. A drop
 * lands at a point of the line and a ring ripple spreads from it (seen in the cut, a wave packet
 * with a sharp leading crest and a few weaker ones behind, getting weaker with distance and time).
 * The ends of the line are tank walls, so the ripples come back and cross each other. The height
 * of the water at each LED is its brightness; the water below its resting level is dark.
 */

/* PWM duty 0..IBLUE_ANIM_MAX of one LED (0 = A .. 3 = D) `elapsed_ms` into an animation of
 * `total_ms`. It fades in and out at the ends and is dark from total_ms on. */
uint32_t iblue_anim_duty(uint32_t elapsed_ms, uint32_t total_ms, int led);

/* How bright calm water is at u (0 .. 1 over the animation): it comes in, is raised after about a third and
 * is eased out to nothing at the end. For the preview drawings and the tests. */
float iblue_anim_calm(float u);

/* The water height itself at x (0 .. 4, LED i at i + 0.5), for the preview drawings and the tests. */
float iblue_anim_height(float x, uint32_t elapsed_ms, uint32_t total_ms);

#endif /* _IBLUE_ANIM_CURVE_H_ */
