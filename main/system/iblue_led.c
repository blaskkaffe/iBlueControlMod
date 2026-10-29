/*
 * iBlueControlMod RGB status LED driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * The board has a 4-pin common-anode RGB LED: each colour pin lights when
 * driven LOW. Each colour gets its own LEDC low-speed channel with the output
 * inverted, so duty 0 = off and full duty = fully on.
 *
 * Pattern priority: button held > pairing (BT inquiry) > solid colour.
 */

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <nvs.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/gpio_hal.h"
#include "soc/io_mux_reg.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "iblue_led.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define IBLUE_LED_TIMER     LEDC_TIMER_2
#define IBLUE_LED_MODE      LEDC_LOW_SPEED_MODE
#define IBLUE_LED_RES       LEDC_TIMER_10_BIT
#define IBLUE_LED_MAX_DUTY  1023
#define IBLUE_LED_FREQ_HZ   5000
#define IBLUE_LED_TICK_MS   20
#define IBLUE_LED_BREATH_MS 1500

#define NVS_NS  "iblue"
#define NVS_KEY "led_col"

static const uint8_t pins[3] = {
    CONFIG_BLUERETRO_IBLUE_LED_R_GPIO,
    CONFIG_BLUERETRO_IBLUE_LED_G_GPIO,
    CONFIG_BLUERETRO_IBLUE_LED_B_GPIO,
};
static const ledc_channel_t chans[3] = {LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5};

/* r, g, b. Green is scaled down for orange as LED green is much brighter. */
static const uint8_t presets[IBLUE_LED_COLOR_MAX][3] = {
    [IBLUE_LED_ORANGE] = {255, 26, 0}, /* deep amber like the original DC LED */
    [IBLUE_LED_BLUE]   = {0, 0, 255},
};
/* Blink frequency per button hold stage (stage 0 = solid) */
static const uint32_t btn_blink_hz[4] = {0, 2, 4, 8};

static volatile uint8_t rgb[3];
static volatile uint32_t color_idx;
static volatile uint32_t btn_active;
static volatile uint32_t btn_stage;
static volatile uint32_t sleeping;
static uint32_t init_done;

static void set_level(uint32_t level_1023) {
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t duty = (rgb[i] * level_1023) / 255;
        ledc_set_duty(IBLUE_LED_MODE, chans[i], duty);
        ledc_update_duty(IBLUE_LED_MODE, chans[i]);
    }
}

static void iblue_led_task(void *arg) {
    uint32_t t = 0;

    while (1) {
        uint32_t level = IBLUE_LED_MAX_DUTY;

        if (sleeping) {
            level = 0;
        }
        else if (btn_active) {
            uint32_t hz = btn_blink_hz[btn_stage & 3];
            if (hz) {
                uint32_t period = 1000 / hz;
                level = ((t % period) < (period / 2)) ? IBLUE_LED_MAX_DUTY : 0;
            }
        }
        else if (bt_hci_get_inquiry()) {
            /* Triangle "breathing" between 5% and 100% */
            uint32_t ph = t % IBLUE_LED_BREATH_MS;
            uint32_t half = IBLUE_LED_BREATH_MS / 2;
            uint32_t tri = (ph < half) ? ph : (IBLUE_LED_BREATH_MS - ph);
            uint32_t lin = (tri * 1000) / half; /* 0..1000 */
            level = 50 + ((lin * lin / 1000) * (IBLUE_LED_MAX_DUTY - 50)) / 1000;
        }

        set_level(level);
        vTaskDelay(IBLUE_LED_TICK_MS / portTICK_PERIOD_MS);
        t += IBLUE_LED_TICK_MS;
    }
}

static void apply_preset(uint32_t idx) {
    if (idx >= IBLUE_LED_COLOR_MAX) {
        idx = 0;
    }
    color_idx = idx;
    rgb[0] = presets[idx][0];
    rgb[1] = presets[idx][1];
    rgb[2] = presets[idx][2];
}

static uint32_t load_color(void) {
    uint32_t idx =
#ifdef CONFIG_BLUERETRO_IBLUE_LED_BLUE
        IBLUE_LED_BLUE;
#else
        IBLUE_LED_ORANGE;
#endif
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t val;
        if (nvs_get_u8(nvs, NVS_KEY, &val) == ESP_OK && val < IBLUE_LED_COLOR_MAX) {
            idx = val;
        }
        nvs_close(nvs);
    }
    return idx;
}

static void save_color(uint32_t idx) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, NVS_KEY, (uint8_t)idx);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_led_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = IBLUE_LED_MODE,
        .duty_resolution = IBLUE_LED_RES,
        .timer_num = IBLUE_LED_TIMER,
        .freq_hz = IBLUE_LED_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    apply_preset(load_color());

    for (uint32_t i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = pins[i],
            .speed_mode = IBLUE_LED_MODE,
            .channel = chans[i],
            .timer_sel = IBLUE_LED_TIMER,
            .duty = 0,
            .hpoint = 0,
            .flags.output_invert = 1, /* common anode: low = on */
        };
        ledc_channel_config(&ch);
    }
    init_done = 1;
    printf("# %s: RGB LED on R:%d G:%d B:%d colour %ld\n", __FUNCTION__,
        pins[0], pins[1], pins[2], color_idx);

    xTaskCreatePinnedToCore(iblue_led_task, "iblue_led", 2048, NULL, 4, NULL, 0);
}

void iblue_led_btn(uint32_t active, uint32_t stage) {
    btn_stage = stage;
    btn_active = active;
}

void iblue_led_set_color(uint32_t idx) {
    apply_preset(idx);
    save_color(color_idx);
}

void iblue_led_toggle_color(void) {
    iblue_led_set_color((color_idx + 1) % IBLUE_LED_COLOR_MAX);
    printf("# %s: colour %ld\n", __FUNCTION__, color_idx);
}

void iblue_led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

void iblue_led_sleep(void) {
    sleeping = 1;
    if (init_done) {
        for (uint32_t i = 0; i < 3; i++) {
            ledc_set_duty(IBLUE_LED_MODE, chans[i], 0);
            ledc_update_duty(IBLUE_LED_MODE, chans[i]);
        }
    }
    /* LEDC stops in deep sleep: latch the pins high (= LED off) */
    for (uint32_t i = 0; i < 3; i++) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pins[i]], PIN_FUNC_GPIO);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 1);
        gpio_hold_en(pins[i]);
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
