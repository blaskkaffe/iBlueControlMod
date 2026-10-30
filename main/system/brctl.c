/*
 * BlueRetro control channel over the Dreamcast Maple bus
 * SPDX-License-Identifier: Apache-2.0
 *
 * The Maple ISR (wired core, bare metal) only copies memory and queues
 * operations; everything that touches flash, Bluetooth or FreeRTOS runs in
 * brctl_exec() on the main core (bt_fb_task).
 */

#include <stdio.h>
#include <string.h>
#include <esp_attr.h>
#include <esp_app_desc.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/gameid.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "system/manager.h"
#include "system/iblue_led.h"
#include "brctl.h"

#ifdef CONFIG_BLUERETRO_BRCTL

/* Plain DRAM (not const/flash): read from the Maple ISR */
static DRAM_ATTR struct brctl_info info;
static DRAM_ATTR struct brctl_status status;

void brctl_init(void) {
    const esp_app_desc_t *app_desc = esp_app_get_description();

    memset(&info, 0, sizeof(info));
    info.magic = BRCTL_INFO_MAGIC;
    info.proto_ver = BRCTL_PROTO_VER;
    info.config_size = sizeof(config);
    info.config_magic = CONFIG_MAGIC;
    info.config_version = CONFIG_VERSION;
    info.wired_max_dev = WIRED_MAX_DEV;
    info.mapping_max = ADAPTER_MAPPING_MAX;
    info.bt_max_dev = BT_MAX_DEV;
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    info.features |= BRCTL_FEAT_RGB_LED;
#endif
#ifdef IBLUE_PAIR_WINDOW
    info.features |= BRCTL_FEAT_PAIR_WINDOW;
#endif
    strncpy(info.fw_version, app_desc->version, sizeof(info.fw_version) - 1);
    strncpy(info.fw_name, app_desc->project_name, sizeof(info.fw_name) - 1);

    memset(&status, 0, sizeof(status));
    status.magic = BRCTL_STATUS_MAGIC;
    brctl_status_update();
    printf("# %s: Maple control channel ready (proto %d, config %d bytes)\n",
        __FUNCTION__, BRCTL_PROTO_VER, info.config_size);
}

void brctl_status_update(void) {
    struct brctl_status st = {0};

    st.magic = BRCTL_STATUS_MAGIC;
    st.seq = status.seq + 1;
    if (bt_hci_get_inquiry()) {
        st.flags |= BRCTL_ST_PAIRING;
    }
#ifdef IBLUE_PAIR_WINDOW
    if (sys_mgr_pairing_allowed()) {
        st.flags |= BRCTL_ST_PAIR_ALLOWED;
    }
    st.pair_window_s = sys_mgr_pair_window_left_s();
#endif
    if (sys_mgr_console_on()) {
        st.flags |= BRCTL_ST_CONSOLE_ON;
    }
#ifdef IBLUE_BTN_MAP
    sys_mgr_btn_get(st.btn_map, &st.btn_seq, &st.btn_last, &st.btn_stage);
#else
    st.btn_last = 0xFF;
#endif
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_led_get_rgb(st.led_rgb);
    st.led_preset = iblue_led_get_preset();
#endif
    for (uint32_t i = 0; i < BRCTL_PORTS; i++) {
        struct bt_dev *device = NULL;

        st.port_bt_id[i] = 0xFF;
        st.port_bt_type[i] = 0xFF;
        st.port_bt_subtype[i] = 0xFF;
        bt_host_get_active_dev_from_out_idx(i, &device);
        if (device) {
            st.port_bt_id[i] = device->ids.id;
            st.port_bt_type[i] = device->ids.type;
            st.port_bt_subtype[i] = device->ids.subtype;
        }
    }
    /* Single copy, ISR may observe a torn struct at worst for one poll */
    memcpy(&status, &st, sizeof(status));
}

uint32_t IRAM_ATTR brctl_read(uint32_t param, uint32_t len, void *dst) {
    uint32_t item = BRCTL_PARAM_ITEM(param);
    uint32_t off = BRCTL_PARAM_OFF(param);
    uint8_t *base;
    uint32_t size;

    switch (item) {
        case BRCTL_ITEM_INFO:
            base = (uint8_t *)&info;
            size = sizeof(info);
            break;
        case BRCTL_ITEM_STATUS:
            base = (uint8_t *)&status;
            size = sizeof(status);
            break;
        case BRCTL_ITEM_CONFIG:
            base = (uint8_t *)&config;
            size = sizeof(config);
            break;
        default:
            return 0;
    }
    if (off & 3 || off >= size) {
        return 0;
    }
    if (len > BRCTL_MAX_XFER) {
        len = BRCTL_MAX_XFER;
    }
    if (len > size - off) {
        len = size - off;
    }
    memcpy(dst, base + off, len);
    return len;
}

bool IRAM_ATTR brctl_write(uint32_t param, const void *src, uint32_t len) {
    uint32_t item = BRCTL_PARAM_ITEM(param);
    uint32_t off = BRCTL_PARAM_OFF(param);

    if (item != BRCTL_ITEM_CONFIG || off & 3 || len > BRCTL_MAX_XFER
            || off > sizeof(config) || len > sizeof(config) - off) {
        return false;
    }
    memcpy((uint8_t *)&config + off, src, len);
    return true;
}

void IRAM_ATTR brctl_queue_op(const uint32_t *op_and_args, uint32_t words) {
    struct raw_fb fb_data = {0};
    uint32_t args[3] = {0};

    for (uint32_t i = 0; i < 3 && i + 1 < words; i++) {
        args[i] = op_and_args[i + 1];
    }
    fb_data.header.wired_id = 0;
    fb_data.header.type = FB_TYPE_BRCTL;
    fb_data.header.data_len = 13;
    fb_data.data[0] = (uint8_t)op_and_args[0];
    memcpy(&fb_data.data[1], args, sizeof(args));
    adapter_q_fb(&fb_data);
}

void brctl_exec(struct raw_fb *fb_data) {
    uint32_t op = fb_data->data[0];
    uint32_t args[3];

    memcpy(args, &fb_data->data[1], sizeof(args));
    printf("# %s: op %ld args %08lX %08lX %08lX\n", __FUNCTION__, op, args[0], args[1], args[2]);

    switch (op) {
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
        case BRCTL_OP_LED_RGB:
            iblue_led_set_rgb((args[0] >> 16) & 0xFF, (args[0] >> 8) & 0xFF, args[0] & 0xFF);
            if (args[1] & 1) {
                iblue_led_save_custom();
            }
            break;
        case BRCTL_OP_LED_PRESET:
            iblue_led_set_color(args[0]);
            break;
#endif
        case BRCTL_OP_PAIR_START:
#ifdef IBLUE_PAIR_WINDOW
            sys_mgr_pair_window_open();
#endif
            if (!bt_hci_get_inquiry()) {
                bt_hci_start_inquiry();
            }
            break;
        case BRCTL_OP_PAIR_STOP:
            if (bt_hci_get_inquiry()) {
                bt_hci_stop_inquiry();
            }
            break;
        case BRCTL_OP_DISCONNECT_ALL:
            bt_host_disconnect_all();
            break;
        case BRCTL_OP_CONFIG_SAVE:
            config_update(config_get_src());
            break;
        case BRCTL_OP_CONFIG_DEFAULT:
        {
            char tmp_str[32] = "/fs/";
            config_init(DEFAULT_CFG);
            strncat(tmp_str, gid_get(), sizeof(tmp_str) - strlen(tmp_str) - 1);
            remove(tmp_str);
            break;
        }
#ifdef IBLUE_BTN_MAP
        case BRCTL_OP_BTN_SET:
            sys_mgr_btn_set(args[0], args[1], args[2] & 1);
            break;
        case BRCTL_OP_BTN_DEFAULTS:
            sys_mgr_btn_defaults(args[0] & 1);
            break;
#endif
        case BRCTL_OP_ADAPTER_RESET:
            sys_mgr_cmd(SYS_MGR_CMD_ADAPTER_RST);
            break;
        default:
            break;
    }
    brctl_status_update();
}

#endif /* CONFIG_BLUERETRO_BRCTL */
