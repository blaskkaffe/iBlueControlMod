/* iBlueControlMod: boot animation of the port LEDs. See iblue_boot_anim.h. */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include "driver/ledc.h"
#include "system/iblue_anim_curve.h"
#include "system/iblue_boot_anim.h"

#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_ANIM

#define ANIM_MODE LEDC_HIGH_SPEED_MODE
#define ANIM_TIMER LEDC_TIMER_3
#define ANIM_FIRST_CHANNEL LEDC_CHANNEL_4
#define ANIM_FREQ_HZ 5000
#define ANIM_FRAME_MS 20
#define ANIM_MIN_LEFT_MS 400 /* not worth starting for less */
#define ANIM_FADE_IN_MS 150  /* when starting in the middle of it, do not pop on */

#if defined(CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_COMET)
#define ANIM_STYLE IBLUE_ANIM_COMET
#elif defined(CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_FILL)
#define ANIM_STYLE IBLUE_ANIM_FILL
#elif defined(CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_WAVE)
#define ANIM_STYLE IBLUE_ANIM_WAVE
#elif defined(CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_CENTER)
#define ANIM_STYLE IBLUE_ANIM_CENTER
#else
#define ANIM_STYLE IBLUE_ANIM_SCAN
#endif

static volatile bool active;
static uint8_t led_pin[IBLUE_ANIM_LEDS];
static uint32_t start_ms;
static void (*done_cb)(void);

static void anim_task(void *arg) {
    const uint32_t total = CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_MS;
    int64_t t0 = esp_timer_get_time();

    (void)arg;
    while (1) {
        uint32_t run = (uint32_t)((esp_timer_get_time() - t0) / 1000);
        uint32_t elapsed = start_ms + run;

        if (elapsed >= total) {
            break;
        }
        for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
            uint32_t duty = iblue_anim_duty(ANIM_STYLE, elapsed, total, i);

            if (run < ANIM_FADE_IN_MS) {
                duty = duty * run / ANIM_FADE_IN_MS;
            }
            ledc_set_duty(ANIM_MODE, ANIM_FIRST_CHANNEL + i, duty);
            ledc_update_duty(ANIM_MODE, ANIM_FIRST_CHANNEL + i);
        }
        vTaskDelay(ANIM_FRAME_MS / portTICK_PERIOD_MS);
    }
    for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
        ledc_stop(ANIM_MODE, ANIM_FIRST_CHANNEL + i, 0);
    }
    active = false;
    if (done_cb) {
        done_cb();
    }
    vTaskDelete(NULL);
}

bool iblue_boot_anim_active(void) {
    return active;
}

void iblue_boot_anim_start(const uint8_t pins[4], uint32_t elapsed_ms, void (*done)(void)) {
    ledc_timer_config_t timer = {
        .speed_mode = ANIM_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = ANIM_TIMER,
        .freq_hz = ANIM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    if (active || elapsed_ms + ANIM_MIN_LEFT_MS >= CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_MS) {
        return;
    }
    start_ms = elapsed_ms;
    done_cb = done;
    ledc_timer_config(&timer);
    for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = pins[i],
            .speed_mode = ANIM_MODE,
            .channel = ANIM_FIRST_CHANNEL + i,
            .timer_sel = ANIM_TIMER,
            .duty = 0,
            .hpoint = 0,
        };

        led_pin[i] = pins[i];
        ledc_channel_config(&ch);
    }
    active = true;
    xTaskCreatePinnedToCore(anim_task, "iblue_anim", 2048, NULL, 4, NULL, 0);
}

#endif /* CONFIG_BLUERETRO_IBLUE_BOOT_ANIM */
