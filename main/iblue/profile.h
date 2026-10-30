/*
 * iBlueControlMod: controller profiles and per-game configurations (flash files).
 *  - /fs/prof_<slot>: the type and button mapping of one port. Home + face button loads one.
 *  - /fs/DC_<game id>: the whole configuration of a game, loaded by the existing game id code.
 * Both survive a factory reset (fs_reset keeps these names). All of it runs in brctl_exec().
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_PROFILE_H_
#define _IBLUE_PROFILE_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "iblue/brctl_proto.h"

#ifdef CONFIG_BLUERETRO_IBLUE_PROFILES

/* Return BRCTL_RES_* */
uint32_t iblue_profile_save(uint32_t slot, uint32_t port);
uint32_t iblue_profile_load(uint32_t slot, uint32_t port, bool persist);
uint32_t iblue_profile_delete(uint32_t slot);
void iblue_profile_list(struct brctl_profiles *out);

/* id: BRCTL_GAME_ID_MAX characters of letters, digits, - and _ ("" = no game: the normal config) */
uint32_t iblue_game_set(const char *id);
uint32_t iblue_game_save(void);
uint32_t iblue_game_delete(void);
void iblue_game_info(struct brctl_game *out);

#endif

#endif /* _IBLUE_PROFILE_H_ */
