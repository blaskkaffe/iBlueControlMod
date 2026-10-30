/*
 * iBlueControlMod settings channel (board side): a window of the emulated VMU
 * that openMenu reads and writes to edit the controller mapping.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Spec: docs/openmenu-config-channel.md. The command handler itself is the
 * portable iblue_cfg_proto.c.
 */

#ifndef _IBLUE_CFG_H_
#define _IBLUE_CFG_H_

#include <stdint.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_CONFIG_CHANNEL

#include "adapter/adapter.h"
#include "iblue_cfg_proto.h"

/* Start the task that runs the commands. After config_init. */
void iblue_cfg_init(void);

/* VMU byte addresses (block * 512 + offset) of the window. The Maple code asks first. */
int iblue_cfg_owns(uint32_t addr);
void iblue_cfg_read(uint32_t addr, uint8_t *data, uint32_t size);
void iblue_cfg_write(uint32_t addr, const uint8_t *data, uint32_t size);
/* After each 128 byte phase of a block write: phase 3 of the first block starts the command */
void iblue_cfg_write_done(uint32_t block, uint32_t phase);

/* Called with every converted input report of a Bluetooth controller (for press-to-learn) */
void iblue_cfg_note_input(uint8_t out_idx, const struct wireless_ctrl *ctrl);

#else
#define iblue_cfg_init()
#define iblue_cfg_owns(addr) 0
#define iblue_cfg_read(addr, data, size)
#define iblue_cfg_write(addr, data, size)
#define iblue_cfg_write_done(block, phase)
#define iblue_cfg_note_input(out_idx, ctrl)
#endif

#endif /* _IBLUE_CFG_H_ */
