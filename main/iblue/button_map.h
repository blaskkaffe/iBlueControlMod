/*
 * iBlueControlMod: what each button / touch sensor gesture does (remappable through brctl).
 * The gesture detection itself stays in system/manager.c (boot_btn_hdl); this holds the map.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_BUTTON_MAP_H_
#define _IBLUE_BUTTON_MAP_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
#define IBLUE_BTN_MAP 1

void iblue_btn_load(void);
/* Action (BRCTL_ACT_*) of a gesture (BRCTL_GEST_*): the map, or the built-in default */
uint32_t iblue_btn_action(uint32_t gesture);
void iblue_btn_report(uint32_t gesture);
void iblue_btn_stage(uint8_t stage);
void iblue_btn_set(uint32_t gesture, uint32_t action, bool persist);
void iblue_btn_defaults(bool persist);
void iblue_btn_get(uint8_t *map8, uint16_t *seq, uint8_t *last, uint8_t *stage);
#endif

#endif /* _IBLUE_BUTTON_MAP_H_ */
