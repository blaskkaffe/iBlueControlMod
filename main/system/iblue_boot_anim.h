/*
 * iBlueControlMod: the four controller port LEDs run an animation while the Dreamcast BIOS boot
 * animation plays. The shape is in iblue_anim_curve.c (portable, tested on a PC); this drives the
 * LEDs with LEDC high speed channels 4 to 7 (timer 3), which nothing else uses.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_BOOT_ANIM_H_
#define _IBLUE_BOOT_ANIM_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_ANIM
/* Start it: the console powered on `elapsed_ms` ago (the adapter was still booting), so only the
 * rest of the BIOS animation is left. `pins` are the four port LED GPIOs, A to D. `done` is called
 * from the animation task when it has finished and let go of the pins. */
void iblue_boot_anim_start(const uint8_t pins[4], uint32_t elapsed_ms, void (*done)(void));
/* True from the start until the pins are let go: the port LEDs belong to the animation */
bool iblue_boot_anim_active(void);
#else
#define iblue_boot_anim_start(pins, elapsed_ms, done)
#define iblue_boot_anim_active() false
#endif

#endif /* _IBLUE_BOOT_ANIM_H_ */
