/* iBlueControlMod: profiles and per-game configurations. See profile.h. */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/gameid.h"
#include "iblue/profile.h"

#ifdef CONFIG_BLUERETRO_IBLUE_PROFILES

#define PROFILE_MAGIC 0x31464F50 /* "POF1" */

struct profile_file {
    uint32_t magic;
    uint8_t dev_mode;
    uint8_t acc_mode;
    uint16_t map_size;
    struct map_cfg map[ADAPTER_MAPPING_MAX];
} __packed;

/* 1 KB: static, only brctl_exec() comes here */
static struct profile_file prof;

static void profile_path(uint32_t slot, char *path, size_t len) {
    snprintf(path, len, "/fs/prof_%u", (unsigned)slot);
}

static bool profile_read(uint32_t slot) {
    char path[24];
    FILE *f;
    size_t n;

    profile_path(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    n = fread(&prof, sizeof(prof), 1, f);
    fclose(f);
    return n == 1 && prof.magic == PROFILE_MAGIC && prof.map_size <= ADAPTER_MAPPING_MAX;
}

void iblue_profile_list(struct brctl_profiles *out) {
    memset(out, 0, sizeof(*out));
    out->magic = BRCTL_PROFILES_MAGIC;
    for (uint32_t i = 0; i < BRCTL_PROFILES; i++) {
        if (profile_read(i)) {
            out->used[i] = 1;
            out->dev_mode[i] = prof.dev_mode;
            out->acc_mode[i] = prof.acc_mode;
        }
    }
}

uint32_t iblue_profile_save(uint32_t slot, uint32_t port) {
    char path[24];
    FILE *f;
    size_t n;

    if (slot >= BRCTL_PROFILES || port >= BRCTL_PORTS) {
        return BRCTL_RES_BAD;
    }
    prof.magic = PROFILE_MAGIC;
    prof.dev_mode = config.out_cfg[port].dev_mode;
    prof.acc_mode = config.out_cfg[port].acc_mode;
    prof.map_size = config.in_cfg[port].map_size;
    memcpy(prof.map, config.in_cfg[port].map_cfg, sizeof(prof.map));
    profile_path(slot, path, sizeof(path));
    f = fopen(path, "wb");
    if (f == NULL) {
        return BRCTL_RES_FAILED;
    }
    n = fwrite(&prof, sizeof(prof), 1, f);
    fclose(f);
    return n == 1 ? BRCTL_RES_OK : BRCTL_RES_FAILED;
}

uint32_t iblue_profile_load(uint32_t slot, uint32_t port, bool persist) {
    if (slot >= BRCTL_PROFILES || port >= BRCTL_PORTS) {
        return BRCTL_RES_BAD;
    }
    if (!profile_read(slot)) {
        return BRCTL_RES_EMPTY;
    }
    config.in_cfg[port].map_size = prof.map_size;
    for (uint32_t i = 0; i < prof.map_size; i++) {
        config.in_cfg[port].map_cfg[i] = prof.map[i];
        config.in_cfg[port].map_cfg[i].dst_id = port; /* the profile is for whichever port it is loaded on */
    }
    config.out_cfg[port].dev_mode = prof.dev_mode & 3;
    config.out_cfg[port].acc_mode = prof.acc_mode & 3;
    adapter_init_buffer(port);
    if (persist) {
        config_update(config_get_src()); /* the game's file if a game's configuration is in use */
        adapter_toggle_fb(port, 150000, 0xFF, 0xFF); /* one short buzz: switched */
    }
    return BRCTL_RES_OK;
}

uint32_t iblue_profile_delete(uint32_t slot) {
    char path[24];

    if (slot >= BRCTL_PROFILES) {
        return BRCTL_RES_BAD;
    }
    profile_path(slot, path, sizeof(path));
    remove(path);
    return BRCTL_RES_OK;
}

/* ---- Games: /fs/DC_<id> is the game's whole configuration ---------------------------------- */

static bool game_file_exists(const char *full) {
    struct stat st;
    char path[32];

    snprintf(path, sizeof(path), "/fs/%s", full);
    return full[0] && stat(path, &st) == 0;
}

uint32_t iblue_game_set(const char *id) {
    char full[24] = "";
    size_t n = strnlen(id, BRCTL_GAME_ID_MAX + 1);

    if (n > BRCTL_GAME_ID_MAX) {
        return BRCTL_RES_BAD;
    }
    for (size_t i = 0; i < n; i++) {
        char c = id[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) {
            return BRCTL_RES_BAD; /* the id becomes part of a file name */
        }
    }
    if (n) {
        snprintf(full, sizeof(full), "DC_%s", id);
    }
    gid_set(full);
    config_init(GAMEID_CFG); /* the game's file if it has one, else the normal configuration */
    return BRCTL_RES_OK;
}

uint32_t iblue_game_save(void) {
    if (!gid_get()[0]) {
        return BRCTL_RES_NO_GAME;
    }
    config_update(GAMEID_CFG);
    return game_file_exists(gid_get()) ? BRCTL_RES_OK : BRCTL_RES_FAILED;
}

uint32_t iblue_game_delete(void) {
    char path[32];

    if (!gid_get()[0]) {
        return BRCTL_RES_NO_GAME;
    }
    snprintf(path, sizeof(path), "/fs/%s", gid_get());
    remove(path);
    config_init(DEFAULT_CFG);
    return BRCTL_RES_OK;
}

void iblue_game_info(struct brctl_game *out) {
    const char *full = gid_get();

    memset(out, 0, sizeof(*out));
    if (full[0]) {
        out->flags = BRCTL_GAME_SET | (game_file_exists(full) ? BRCTL_GAME_HAS_OWN : 0) |
            (config_get_src() == GAMEID_CFG ? BRCTL_GAME_IN_USE : 0);
        strncpy(out->id, strncmp(full, "DC_", 3) == 0 ? full + 3 : full, sizeof(out->id) - 1);
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_PROFILES */
