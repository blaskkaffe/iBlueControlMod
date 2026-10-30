/*
 * iBlueControlMod: Home + face button picks a profile slot for the controller it is pressed on.
 * Portable C (no ESP-IDF), tested on a PC: sh tests/host/run.sh
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_HOTKEY_H_
#define _IBLUE_HOTKEY_H_

#include <stdint.h>

/* BlueRetro generic button ids (adapter.h): Home is PAD_MT, the face buttons PAD_RB_* (16..19) */
#define IBLUE_HOTKEY_HOME  22
#define IBLUE_HOTKEY_FACES 0x000F0000u

struct iblue_hotkey {
    uint32_t prev_faces;
};

/* Feed it the held generic buttons (word 0) of every report of one controller. While Home is held
 * the face buttons are taken out of *btns (the game does not see them) and a newly pressed face
 * returns its slot: face down 0, right 1, left 2, up 3 (A, B, X, Y). Otherwise -1. */
int iblue_hotkey_filter(struct iblue_hotkey *st, uint32_t *btns);

#endif /* _IBLUE_HOTKEY_H_ */
