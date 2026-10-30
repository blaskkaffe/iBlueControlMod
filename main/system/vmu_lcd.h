/*
 * Capture of the VMU LCD frame the Dreamcast writes to the emulated VMU
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VMU_LCD_H_
#define _VMU_LCD_H_

#include <stdint.h>
#include <stdbool.h>

/* 48x32 pixels, 1 bpp: 32 rows of 6 bytes */
#define VMU_LCD_SIZE 192

/* Wired core (Maple ISR): IRAM, no flash access */
void vmu_lcd_push(const void *src);

/* Main core: copy out the latest frame, in the byte order it had on the
 * Maple bus. Returns the frame counter (starts at 1, wraps),
 * or 0 if no frame was written yet. A consumer that only wants new
 * frames compares the returned value with the one from its previous call. */
uint32_t vmu_lcd_get(void *dst);

#endif /* _VMU_LCD_H_ */
