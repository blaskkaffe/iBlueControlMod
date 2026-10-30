/*
 * iBlueControlMod: what each button / touch sensor gesture does. See button_map.h.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <nvs.h>
#include "iblue/brctl_proto.h"
#include "iblue/button_map.h"

#ifdef IBLUE_BTN_MAP

static const uint8_t btn_default_act[BRCTL_GEST_MAX] = {
    [BRCTL_GEST_TAP] = BRCTL_ACT_PAIR,
    [BRCTL_GEST_DOUBLE_TAP] = BRCTL_ACT_LED_NEXT,
    [BRCTL_GEST_TAP_HOLD] = BRCTL_ACT_LED_ADJUST,
    [BRCTL_GEST_HOLD_3S] = BRCTL_ACT_PAIR_STOP_OR_DISCONNECT,
    [BRCTL_GEST_HOLD_6S] = BRCTL_ACT_PAIR,
    [BRCTL_GEST_HOLD_10S] = BRCTL_ACT_FACTORY_RESET,
};
static uint8_t btn_map[8]; /* BRCTL_ACT_DEFAULT (0) = built-in action */
static volatile uint16_t btn_seq;
static volatile uint8_t btn_last = 0xFF;
static volatile uint8_t btn_stage;

uint32_t iblue_btn_action(uint32_t gesture) {
    uint32_t act = btn_map[gesture];

    if (act == BRCTL_ACT_DEFAULT || act >= BRCTL_ACT_MAX) {
        act = btn_default_act[gesture];
    }
    return act;
}

void iblue_btn_load(void) {
    nvs_handle_t nvs;
    size_t len = sizeof(btn_map);

    if (nvs_open("iblue", NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_blob(nvs, "btn_map", btn_map, &len) != ESP_OK) {
            memset(btn_map, 0, sizeof(btn_map));
        }
        nvs_close(nvs);
    }
}

static void btn_map_save(void) {
    nvs_handle_t nvs;

    if (nvs_open("iblue", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, "btn_map", btn_map, sizeof(btn_map));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_btn_set(uint32_t gesture, uint32_t action, bool persist) {
    if (gesture < BRCTL_GEST_MAX && action < BRCTL_ACT_MAX) {
        btn_map[gesture] = action;
        if (persist) {
            btn_map_save();
        }
    }
}

void iblue_btn_defaults(bool persist) {
    memset(btn_map, 0, sizeof(btn_map));
    if (persist) {
        btn_map_save();
    }
}

void iblue_btn_get(uint8_t *map8, uint16_t *seq, uint8_t *last, uint8_t *stage) {
    for (uint32_t i = 0; i < 8; i++) {
        map8[i] = (i < BRCTL_GEST_MAX) ? iblue_btn_action(i) : 0;
    }
    *seq = btn_seq;
    *last = btn_last;
    *stage = btn_stage;
}

void iblue_btn_report(uint32_t gesture) {
    btn_last = gesture;
    btn_seq++;
    printf("# %s: gesture %ld -> action %ld\n", __FUNCTION__, gesture, iblue_btn_action(gesture));
}

void iblue_btn_stage(uint8_t stage) {
    btn_stage = stage;
}

#endif /* IBLUE_BTN_MAP */
