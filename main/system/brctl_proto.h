/*
 * BlueRetro control channel over the Dreamcast Maple bus ("brctl")
 * Shared between the adapter firmware and the Dreamcast client library.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Transport
 * ---------
 * Requests are ordinary Maple frames sent to the *main unit* (controller)
 * of a port where BlueRetro presents a device. The first payload word is a
 * private function code that no game uses, so normal traffic is unaffected.
 *
 *   BLOCK_READ  (cmd 11)  [func, param, nbytes]        -> DATA_TX [func, param, data...]
 *   BLOCK_WRITE (cmd 12)  [func, param, data...]       -> ACK (7) or FILE_ERROR (0xFB)
 *   SET_COND    (cmd 14)  [func, op, arg0, arg1, arg2] -> ACK (7)
 *
 *   param = (item << 24) | byte_offset   (offset multiple of 4)
 *
 * Byte order: the Maple bus transmits every 32-bit word most significant
 * byte first, so the Dreamcast sees each word byte-reversed compared to the
 * adapter. The client library swaps every payload word in both directions;
 * after that both sides see identical byte streams and little-endian
 * integers, and the structs below can be used as-is on both sides.
 */

#ifndef _BRCTL_PROTO_H_
#define _BRCTL_PROTO_H_

#include <stdint.h>

#define BRCTL_PROTO_VER 1

/* Function code in adapter (logical) byte order: bytes 'B' 'R' 'T' 'L' */
#define BRCTL_FUNC 0x4C545242u

#define BRCTL_MAX_XFER 512   /* bytes per read/write request */

#define BRCTL_PARAM(item, off) ((((uint32_t)(item)) << 24) | (((uint32_t)(off)) & 0x00FFFFFFu))
#define BRCTL_PARAM_ITEM(p) ((p) >> 24)
#define BRCTL_PARAM_OFF(p) ((p) & 0x00FFFFFFu)

/* Items */
enum {
    BRCTL_ITEM_INFO = 0x01,     /* RO  struct brctl_info */
    BRCTL_ITEM_STATUS = 0x02,   /* RO  struct brctl_status */
    BRCTL_ITEM_CONFIG = 0x03,   /* RW  raw BlueRetro `struct config` (same as blueretro.io) */
};

/* SET_COND operations (args are little-endian u32) */
enum {
    BRCTL_OP_NOP = 0x00,
    BRCTL_OP_LED_RGB = 0x01,        /* arg0 = 0x00RRGGBB, arg1 bit0 = save as custom colour */
    BRCTL_OP_LED_PRESET = 0x02,     /* arg0 = preset (0 orange, 1 blue, 2 custom) */
    BRCTL_OP_PAIR_START = 0x03,     /* open pairing window and start pairing */
    BRCTL_OP_PAIR_STOP = 0x04,
    BRCTL_OP_DISCONNECT_ALL = 0x05,
    BRCTL_OP_CONFIG_SAVE = 0x06,    /* apply + persist config written via ITEM_CONFIG */
    BRCTL_OP_CONFIG_DEFAULT = 0x07, /* reset config to defaults */
    BRCTL_OP_ADAPTER_RESET = 0x08,  /* restart the adapter */
};

/* Response codes (Maple) */
#define BRCTL_RSP_ACK 0x07
#define BRCTL_RSP_DATA 0x08
#define BRCTL_RSP_FILE_ERROR 0xFB

#define BRCTL_INFO_MAGIC 0x46495242u    /* 'BRIF' */
#define BRCTL_STATUS_MAGIC 0x54535242u  /* 'BRST' */

struct brctl_info {
    uint32_t magic;
    uint16_t proto_ver;
    uint16_t config_size;       /* sizeof(struct config) on the adapter */
    uint32_t config_magic;
    uint8_t config_version;
    uint8_t wired_max_dev;
    uint8_t mapping_max;
    uint8_t bt_max_dev;
    uint8_t features;           /* bit0 RGB LED, bit1 pairing window */
    uint8_t reserved[3];
    char fw_version[32];
    char fw_name[32];
} __attribute__((packed));

#define BRCTL_FEAT_RGB_LED 0x01
#define BRCTL_FEAT_PAIR_WINDOW 0x02

#define BRCTL_PORTS 4

struct brctl_status {
    uint32_t magic;
    uint32_t seq;               /* increments on every adapter-side update */
    uint16_t flags;
    uint16_t pair_window_s;     /* seconds left in the pairing window */
    uint8_t led_rgb[3];
    uint8_t led_preset;
    uint8_t port_bt_id[BRCTL_PORTS];      /* 0xFF = no BT controller on this port */
    uint8_t port_bt_type[BRCTL_PORTS];    /* BlueRetro BT_* type */
    uint8_t port_bt_subtype[BRCTL_PORTS];
} __attribute__((packed));

#define BRCTL_ST_PAIRING 0x0001         /* inquiry (pairing) running */
#define BRCTL_ST_PAIR_ALLOWED 0x0002    /* inside the pairing window */
#define BRCTL_ST_CONSOLE_ON 0x0004

#endif /* _BRCTL_PROTO_H_ */
