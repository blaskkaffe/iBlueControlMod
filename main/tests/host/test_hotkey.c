/* Host test of the Home + face button filter: see run.sh */
#include <stdio.h>
#include "../../iblue/hotkey.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void) {
    struct iblue_hotkey hk = {0};
    uint32_t b = 1u << 18;

    CHECK(iblue_hotkey_filter(&hk, &b) == -1 && b == (1u << 18));                       /* a face alone is a normal press */
    b = (1u << 22) | (1u << 18);
    CHECK(iblue_hotkey_filter(&hk, &b) == -1 && !(b & 0xF0000) && (b & (1u << 22)));    /* held before Home: no slot, hidden */
    b = 1u << 22;
    CHECK(iblue_hotkey_filter(&hk, &b) == -1);
    b = (1u << 22) | (1u << 18);
    CHECK(iblue_hotkey_filter(&hk, &b) == 0 && !(b & 0xF0000));
    CHECK(iblue_hotkey_filter(&hk, &b) == -1);                                          /* held: only the edge counts */
    b = (1u << 22) | (1u << 17); CHECK(iblue_hotkey_filter(&hk, &b) == 1);
    b = (1u << 22) | (1u << 16); CHECK(iblue_hotkey_filter(&hk, &b) == 2);
    b = (1u << 22) | (1u << 19); CHECK(iblue_hotkey_filter(&hk, &b) == 3);
    b = 1u << 19; CHECK(iblue_hotkey_filter(&hk, &b) == -1 && b == (1u << 19));
    b = (1u << 22) | (1u << 3) | (1u << 25);                                            /* other buttons are left alone */
    CHECK(iblue_hotkey_filter(&hk, &b) == -1 && b == ((1u << 22) | (1u << 3) | (1u << 25)));

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
