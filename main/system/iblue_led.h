/*
 * iBlueControlMod RGB status LED driver
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_LED_H_
#define _IBLUE_LED_H_

#include <stdint.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

enum {
    IBLUE_LED_ORANGE = 0,
    IBLUE_LED_BLUE,
    IBLUE_LED_COLOR_MAX,
};

/* Take over the 3 colour pins (after sys_mgr pin init) and start the pattern task */
void iblue_led_init(void);
/* Button feedback: active=1 while the button is held, stage 0..3 = hold stage */
void iblue_led_btn(uint32_t active, uint32_t stage);
/* Preset colours, persisted in NVS */
void iblue_led_set_color(uint32_t idx);
void iblue_led_toggle_color(void);
/* Arbitrary colour (not persisted), for future Maple-bus / BLE commands */
void iblue_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);
/* Turn the LED off and latch the pins off for deep sleep */
void iblue_led_sleep(void);

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
#endif /* _IBLUE_LED_H_ */
