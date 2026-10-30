/* iBlueControlMod: Home + face button filter. See hotkey.h. */

#include "iblue/hotkey.h"

int iblue_hotkey_filter(struct iblue_hotkey *st, uint32_t *btns) {
    uint32_t faces = *btns & IBLUE_HOTKEY_FACES;
    uint32_t fresh = faces & ~st->prev_faces;
    int slot = -1;

    st->prev_faces = faces;
    if (!(*btns & (1u << IBLUE_HOTKEY_HOME))) {
        return -1;
    }
    *btns &= ~IBLUE_HOTKEY_FACES;
    if (fresh & (1u << 18)) slot = 0;
    else if (fresh & (1u << 17)) slot = 1;
    else if (fresh & (1u << 16)) slot = 2;
    else if (fresh & (1u << 19)) slot = 3;
    return slot;
}
