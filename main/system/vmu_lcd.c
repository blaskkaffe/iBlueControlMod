/*
 * Capture of the VMU LCD frame the Dreamcast writes to the emulated VMU
 * SPDX-License-Identifier: Apache-2.0
 *
 * The Maple ISR (wired core, bare metal) only copies the frame; whatever
 * displays or forwards it runs on the main core via vmu_lcd_get(). The frame
 * is kept as received, no word swap is applied here.
 */

#include <string.h>
#include <esp_attr.h>
#include "vmu_lcd.h"

/* Plain DRAM: written from the Maple ISR. Seqlock: seq is odd while the
 * writer is mid-copy. */
static DRAM_ATTR uint8_t frame[VMU_LCD_SIZE];
static DRAM_ATTR volatile uint32_t seq;

void IRAM_ATTR vmu_lcd_push(const void *src) {
    const uint32_t *s = (const uint32_t *)src;
    uint32_t *d = (uint32_t *)frame;

    ++seq;
    for (uint32_t i = 0; i < VMU_LCD_SIZE / 4; i++) {
        d[i] = s[i];
    }
    ++seq;
}

uint32_t vmu_lcd_get(void *dst) {
    uint32_t s1, s2;

    do {
        s1 = seq;
        if (s1 == 0) {
            return 0;
        }
        memcpy(dst, frame, VMU_LCD_SIZE);
        s2 = seq;
    } while (s1 != s2 || (s1 & 1));

    return s1 >> 1;
}
