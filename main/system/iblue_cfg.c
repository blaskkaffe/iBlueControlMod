/*
 * iBlueControlMod settings channel (board side). See iblue_cfg.h.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "bluetooth/host.h"
#include "system/manager.h"
#include "iblue_cfg.h"

#ifdef CONFIG_BLUERETRO_IBLUE_CONFIG_CHANNEL

#define POLL_MS 20
#define LIVE_STALE_US 500000

_Static_assert(sizeof(struct iblue_map) == sizeof(struct map_cfg), "iblue_map must match map_cfg");
_Static_assert(IBLUE_MAP_MAX == ADAPTER_MAPPING_MAX, "table size");
_Static_assert(IBLUE_PORTS <= WIRED_MAX_DEV, "ports");

static DRAM_ATTR uint8_t window[IBLUE_WINDOW_SIZE];
static DRAM_ATTR volatile uint32_t run_pending;
static DRAM_ATTR volatile uint32_t live[IBLUE_PORTS][4];
static DRAM_ATTR volatile int64_t live_us[IBLUE_PORTS];

static void op_get_port(void *ctx, uint8_t port, struct iblue_port *out) {
    struct bt_dev *dev = NULL;
    int32_t idx = bt_host_get_active_dev_from_out_idx(port, &dev);

    (void)ctx;
    out->dev_mode = config.out_cfg[port].dev_mode;
    out->acc_mode = config.out_cfg[port].acc_mode;
    out->map_size = config.in_cfg[port].map_size;
    out->connected = idx >= 0;
    out->plugged = sys_mgr_port_plugged(port);
    if (idx >= 0 && dev) {
        out->vid = bt_adapter.data[dev->ids.id].base.vid;
        out->pid = bt_adapter.data[dev->ids.id].base.pid;
        if (dev->name) {
            strncpy(out->name, dev->name->name, IBLUE_NAME_LEN - 1);
        }
    }
}

static void op_get_map(void *ctx, uint8_t port, uint16_t first, uint16_t count, struct iblue_map *out) {
    (void)ctx;
    memcpy(out, &config.in_cfg[port].map_cfg[first], count * sizeof(*out));
}

static void op_set_map(void *ctx, uint8_t port, uint16_t first, uint16_t count, const struct iblue_map *in,
                       uint16_t new_size) {
    (void)ctx;
    memcpy(&config.in_cfg[port].map_cfg[first], in, count * sizeof(*in));
    if (new_size != 0xFFFF) {
        config.in_cfg[port].map_size = new_size;
    }
}

static void op_set_type(void *ctx, uint8_t port, uint8_t dev_mode, uint8_t acc_mode) {
    (void)ctx;
    config.out_cfg[port].dev_mode = dev_mode;
    config.out_cfg[port].acc_mode = acc_mode;
    adapter_init_buffer(port);
}

static void op_reset_map(void *ctx, uint8_t port) {
    (void)ctx;
    config_reset_map(port);
}

static int op_commit(void *ctx) {
    (void)ctx;
    config_update(config_get_src());
    return 0;
}

static void op_revert(void *ctx) {
    (void)ctx;
    config_init(config_get_src());
}

static int op_live(void *ctx, uint8_t port, uint32_t out[4]) {
    struct bt_dev *dev = NULL;
    int connected = bt_host_get_active_dev_from_out_idx(port, &dev) >= 0;

    (void)ctx;
    if (connected && esp_timer_get_time() - live_us[port] < LIVE_STALE_US) {
        for (int i = 0; i < 4; i++) {
            out[i] = live[port][i];
        }
    }
    return connected;
}

static void op_get_global(void *ctx, struct iblue_global *out) {
    (void)ctx;
    out->inquiry_mode = config.global_cfg.inquiry_mode;
}

static void op_set_global(void *ctx, const struct iblue_global *in) {
    (void)ctx;
    config.global_cfg.inquiry_mode = in->inquiry_mode;
}

static const struct iblue_cfg_ops ops = {
    .get_port = op_get_port,
    .get_map = op_get_map,
    .set_map = op_set_map,
    .set_type = op_set_type,
    .reset_map = op_reset_map,
    .commit = op_commit,
    .revert = op_revert,
    .live = op_live,
    .get_global = op_get_global,
    .set_global = op_set_global,
};

static void cfg_task(void *arg) {
    struct iblue_cfg cfg = {.ops = &ops};

    (void)arg;
    while (1) {
        vTaskDelay(POLL_MS / portTICK_PERIOD_MS);
        if (run_pending) {
            iblue_cfg_run(&cfg, window);
            run_pending = 0;
        }
    }
}

void iblue_cfg_init(void) {
    iblue_cfg_window_init(window);
    xTaskCreatePinnedToCore(cfg_task, "iblue_cfg", 8192, NULL, 5, NULL, 1);
}

int IRAM_ATTR iblue_cfg_owns(uint32_t addr) {
    return addr >= IBLUE_BLOCK_FIRST * IBLUE_BLOCK_SIZE &&
        addr < (IBLUE_BLOCK_FIRST + IBLUE_BLOCK_COUNT) * IBLUE_BLOCK_SIZE;
}

void IRAM_ATTR iblue_cfg_read(uint32_t addr, uint8_t *data, uint32_t size) {
    memcpy(data, &window[addr - IBLUE_BLOCK_FIRST * IBLUE_BLOCK_SIZE], size);
}

void IRAM_ATTR iblue_cfg_write(uint32_t addr, const uint8_t *data, uint32_t size) {
    uint32_t off = addr - IBLUE_BLOCK_FIRST * IBLUE_BLOCK_SIZE;

    /* The console must not rewrite a request the task is running */
    if (!run_pending && off + size <= IBLUE_WINDOW_SIZE) {
        memcpy(&window[off], data, size);
    }
}

void IRAM_ATTR iblue_cfg_write_done(uint32_t block, uint32_t phase) {
    if (block == IBLUE_BLOCK_FIRST && phase == 3) {
        run_pending = 1;
    }
}

void iblue_cfg_note_input(uint8_t out_idx, const struct wireless_ctrl *ctrl) {
    uint32_t w[4];

    if (out_idx >= IBLUE_PORTS || ctrl == NULL) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        w[i] = ctrl->btns[i].value;
    }
    for (uint32_t a = 0; a < ADAPTER_MAX_AXES; a++) {
        const struct ctrl_meta *meta = ctrl->axes[a].meta;
        uint32_t neg = axis_to_btn_id(a);
        int32_t v = ctrl->axes[a].value;

        if (meta == NULL || meta->abs_max == 0) {
            continue;
        }
        if (a >= TRIG_L && a <= TRIG_R) {
            if (v > meta->abs_max / 2) {
                w[neg >> 5] |= BIT(neg & 0x1F);
            }
        }
        else if (v > meta->abs_max / 2 || v < -(meta->abs_max / 2)) {
            uint32_t pos = neg + 1;
            uint32_t id = ((v > 0) == (btn_sign(meta->polarity, pos) > 0)) ? pos : neg;

            w[id >> 5] |= BIT(id & 0x1F);
        }
    }
    for (int i = 0; i < 4; i++) {
        live[out_idx][i] = w[i];
    }
    live_us[out_idx] = esp_timer_get_time();
}

#endif /* CONFIG_BLUERETRO_IBLUE_CONFIG_CHANNEL */
