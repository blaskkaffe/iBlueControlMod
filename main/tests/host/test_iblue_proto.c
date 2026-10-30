/* Host test of the iBlue settings channel handler: cc -o t test_iblue_proto.c ../../system/iblue_cfg_proto.c && ./t */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../system/iblue_cfg_proto.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

struct fake {
    struct iblue_port port[IBLUE_PORTS];
    struct iblue_map map[IBLUE_PORTS][IBLUE_MAP_MAX];
    struct iblue_port saved_port[IBLUE_PORTS];
    struct iblue_map saved_map[IBLUE_PORTS][IBLUE_MAP_MAX];
    int commits, fail_commit;
    uint32_t held[4];
    struct iblue_global global;
    uint8_t plugged[IBLUE_PORTS];
};

static void defaults(struct fake *f) {
    memset(f, 0, sizeof(*f));
    for (int p = 0; p < IBLUE_PORTS; p++) {
        f->port[p].map_size = 118;
        f->port[p].acc_mode = 1;
        for (int i = 0; i < 118; i++) {
            f->map[p][i] = (struct iblue_map){i, i, p, 100, 50, 135, 0, 0};
        }
    }
    memcpy(f->saved_port, f->port, sizeof(f->port));
    memcpy(f->saved_map, f->map, sizeof(f->map));
}
static void o_get_port(void *c, uint8_t p, struct iblue_port *o) { *o = ((struct fake *)c)->port[p]; strcpy(o->name, "Pad"); o->connected = p == 1; o->plugged = ((struct fake *)c)->plugged[p]; }
static void o_get_map(void *c, uint8_t p, uint16_t f0, uint16_t n, struct iblue_map *o) { memcpy(o, &((struct fake *)c)->map[p][f0], n * 8); }
static void o_set_map(void *c, uint8_t p, uint16_t f0, uint16_t n, const struct iblue_map *i, uint16_t ns) {
    struct fake *f = c; memcpy(&f->map[p][f0], i, n * 8); if (ns != 0xFFFF) f->port[p].map_size = ns;
}
static void o_set_type(void *c, uint8_t p, uint8_t d, uint8_t a) { struct fake *f = c; f->port[p].dev_mode = d; f->port[p].acc_mode = a; }
static void o_reset(void *c, uint8_t p) { struct fake *f = c; for (int i = 0; i < 118; i++) f->map[p][i] = (struct iblue_map){i, i, p, 100, 50, 135, 0, 0}; f->port[p].map_size = 118; }
static int o_commit(void *c) { struct fake *f = c; if (f->fail_commit) return -1; f->commits++; memcpy(f->saved_port, f->port, sizeof(f->port)); memcpy(f->saved_map, f->map, sizeof(f->map)); return 0; }
static void o_revert(void *c) { struct fake *f = c; memcpy(f->port, f->saved_port, sizeof(f->port)); memcpy(f->map, f->saved_map, sizeof(f->map)); }
static int o_live(void *c, uint8_t p, uint32_t o[4]) { struct fake *f = c; memcpy(o, f->held, 16); return p == 1; }
static void o_get_global(void *c, struct iblue_global *g) { *g = ((struct fake *)c)->global; }
static void o_set_global(void *c, const struct iblue_global *g) { ((struct fake *)c)->global = *g; }
static const struct iblue_cfg_ops ops = {o_get_port, o_get_map, o_set_map, o_set_type, o_reset, o_commit, o_revert, o_live, o_get_global, o_set_global};

static uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static void wr16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

static uint32_t seq;
/* Client side as openMenu does it: fill the window, run, read the answer. Returns result code */
static uint16_t call(struct iblue_cfg *c, uint8_t *w, uint16_t cmd, const uint8_t *arg, uint16_t len, const uint8_t **out, uint16_t *olen) {
    memset(w + IBLUE_HDR_SIZE, 0, IBLUE_DATA_MAX);
    memcpy(w + IBLUE_HDR_SIZE, arg, len);
    wr16(w + IBLUE_H_STATUS, IBLUE_ST_PENDING);
    wr16(w + IBLUE_H_CMD, cmd);
    wr16(w + IBLUE_H_LEN, len);
    wr32(w + IBLUE_H_SEQ, ++seq);
    wr32(w + IBLUE_H_CRC, iblue_crc32(w + IBLUE_HDR_SIZE, len));
    iblue_cfg_run(c, w);
    CHECK(!(rd16(w + IBLUE_H_STATUS) & IBLUE_ST_PENDING));
    CHECK(rd32(w + IBLUE_H_SEQ) == seq);
    *olen = rd16(w + IBLUE_H_LEN);
    *out = w + IBLUE_HDR_SIZE;
    CHECK(iblue_crc32(*out, *olen) == rd32(w + IBLUE_H_CRC));
    return rd16(w + IBLUE_H_RESULT);
}

int main(void) {
    static struct fake f;
    static uint8_t w[IBLUE_WINDOW_SIZE];
    struct iblue_cfg c = {&ops, &f, 0};
    const uint8_t *o; uint16_t ol;
    uint8_t a[2000];

    CHECK(iblue_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926);
    defaults(&f);
    iblue_cfg_window_init(w);
    iblue_cfg_run(&c, w); /* idle window: nothing happens */
    CHECK(memcmp(w, "iBLU", 4) == 0 && rd16(w + IBLUE_H_STATUS) == 0);

    CHECK(call(&c, w, IBLUE_CMD_HELLO, a, 0, &o, &ol) == IBLUE_RES_OK && ol == 8);
    CHECK(rd16(o) == IBLUE_VERSION && o[2] == 4 && rd16(o + 4) == 128);

    a[0] = 1;
    CHECK(call(&c, w, IBLUE_CMD_PORT_INFO, a, 1, &o, &ol) == IBLUE_RES_OK && ol == 34);
    CHECK(o[2] == 1 && rd16(o + 4) == 118 && strcmp((const char *)o + 10, "Pad") == 0);
    a[0] = 4;
    CHECK(call(&c, w, IBLUE_CMD_PORT_INFO, a, 1, &o, &ol) == IBLUE_RES_BAD_ARG && ol == 0);

    /* read the whole table in two parts */
    a[0] = 0; wr16(a + 1, 0); wr16(a + 3, 118);
    CHECK(call(&c, w, IBLUE_CMD_GET_MAP, a, 5, &o, &ol) == IBLUE_RES_OK && ol == 118 * 8);
    CHECK(o[18 * 8] == 18 && o[18 * 8 + 1] == 18);
    wr16(a + 1, 100); wr16(a + 3, 29);
    CHECK(call(&c, w, IBLUE_CMD_GET_MAP, a, 5, &o, &ol) == IBLUE_RES_BAD_ARG);

    /* remap: source 16 (RB_LEFT) now acts as destination 17 */
    a[0] = 0; wr16(a + 1, 16); wr16(a + 3, 1); wr16(a + 5, 0xFFFF);
    struct iblue_map m = {16, 17, 0, 100, 50, 135, 0, 0};
    memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_OK);
    CHECK(f.map[0][16].dst_btn == 17 && (c.status & IBLUE_ST_UNSAVED));
    CHECK(rd16(w + IBLUE_H_STATUS) & IBLUE_ST_UNSAVED);

    /* invalid entries are refused and change nothing */
    m.dst_id = 9; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_BAD_ARG);
    m.dst_id = 0; m.perc_max = 101; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_BAD_ARG);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 14, &o, &ol) == IBLUE_RES_BAD_ARG); /* length mismatch */
    CHECK(f.map[0][16].dst_btn == 17 && f.map[0][16].perc_max == 100);

    /* a corrupted request is refused */
    m.perc_max = 100; memcpy(a + 7, &m, 8);
    memset(w + IBLUE_HDR_SIZE, 0, IBLUE_DATA_MAX); memcpy(w + IBLUE_HDR_SIZE, a, 15);
    wr16(w + IBLUE_H_STATUS, IBLUE_ST_PENDING); wr16(w + IBLUE_H_CMD, IBLUE_CMD_SET_MAP); wr16(w + IBLUE_H_LEN, 15);
    wr32(w + IBLUE_H_CRC, iblue_crc32(w + IBLUE_HDR_SIZE, 15) ^ 1);
    iblue_cfg_run(&c, w);
    CHECK(rd16(w + IBLUE_H_RESULT) == IBLUE_RES_BAD_CRC);

    /* grow the table (a new binding) */
    wr16(a + 1, 118); wr16(a + 3, 1); wr16(a + 5, 119);
    m = (struct iblue_map){5, 18, 0, 100, 50, 135, 0, 0}; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_OK && f.port[0].map_size == 119);
    wr16(a + 5, 128 + 1);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_BAD_ARG);

    /* type: port A keeps its VMU */
    a[0] = 0; a[1] = 1; a[2] = 2;
    CHECK(call(&c, w, IBLUE_CMD_SET_TYPE, a, 3, &o, &ol) == IBLUE_RES_LOCKOUT && f.port[0].acc_mode == 1);
    a[0] = 2; a[2] = 2;
    CHECK(call(&c, w, IBLUE_CMD_SET_TYPE, a, 3, &o, &ol) == IBLUE_RES_OK && f.port[2].dev_mode == 1 && f.port[2].acc_mode == 2);
    a[1] = 4;
    CHECK(call(&c, w, IBLUE_CMD_SET_TYPE, a, 3, &o, &ol) == IBLUE_RES_BAD_ARG);

    /* revert drops everything, commit keeps it */
    CHECK(call(&c, w, IBLUE_CMD_REVERT, a, 0, &o, &ol) == IBLUE_RES_OK);
    CHECK(f.map[0][16].dst_btn == 16 && f.port[2].dev_mode == 0 && !(c.status & IBLUE_ST_UNSAVED));
    a[0] = 0; wr16(a + 1, 16); wr16(a + 3, 1); wr16(a + 5, 0xFFFF);
    m = (struct iblue_map){16, IBLUE_DST_NONE, 0, 100, 50, 135, 0, 0}; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_OK);
    f.fail_commit = 1;
    CHECK(call(&c, w, IBLUE_CMD_COMMIT, a, 0, &o, &ol) == IBLUE_RES_FAILED && (c.status & IBLUE_ST_UNSAVED));
    f.fail_commit = 0;
    CHECK(call(&c, w, IBLUE_CMD_COMMIT, a, 0, &o, &ol) == IBLUE_RES_OK && f.commits == 1 && !(c.status & IBLUE_ST_UNSAVED));
    CHECK(call(&c, w, IBLUE_CMD_REVERT, a, 0, &o, &ol) == IBLUE_RES_OK && f.map[0][16].dst_btn == IBLUE_DST_NONE);

    /* reset one port */
    a[0] = 0;
    CHECK(call(&c, w, IBLUE_CMD_RESET_MAP, a, 1, &o, &ol) == IBLUE_RES_OK && f.map[0][16].dst_btn == 16);

    /* live input */
    f.held[0] = 1u << 18; a[0] = 1;
    CHECK(call(&c, w, IBLUE_CMD_LIVE, a, 1, &o, &ol) == IBLUE_RES_OK && ol == 20 && o[0] == 1 && rd32(o + 4) == (1u << 18));
    a[0] = 0;
    CHECK(call(&c, w, IBLUE_CMD_LIVE, a, 1, &o, &ol) == IBLUE_RES_OK && o[0] == 0);

    /* physical controller sense is reported per port */
    f.plugged[2] = 1; a[0] = 2;
    CHECK(call(&c, w, IBLUE_CMD_PORT_INFO, a, 1, &o, &ol) == IBLUE_RES_OK && o[3] == 1);
    a[0] = 3;
    CHECK(call(&c, w, IBLUE_CMD_PORT_INFO, a, 1, &o, &ol) == IBLUE_RES_OK && o[3] == 0);

    /* global: pairing mode */
    CHECK(call(&c, w, IBLUE_CMD_GET_GLOBAL, a, 0, &o, &ol) == IBLUE_RES_OK && ol == 8 && o[0] == 0);
    a[0] = 1;
    CHECK(call(&c, w, IBLUE_CMD_SET_GLOBAL, a, 1, &o, &ol) == IBLUE_RES_OK && f.global.inquiry_mode == 1 && (c.status & IBLUE_ST_UNSAVED));
    a[0] = 2;
    CHECK(call(&c, w, IBLUE_CMD_SET_GLOBAL, a, 1, &o, &ol) == IBLUE_RES_BAD_ARG && f.global.inquiry_mode == 1);

    /* per button options: scaling mode is 0..5 */
    a[0] = 0; wr16(a + 1, 16); wr16(a + 3, 1); wr16(a + 5, 0xFFFF);
    m = (struct iblue_map){16, 16, 0, 80, 40, 10, 6, 5}; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_OK && f.map[0][16].turbo == 6 && f.map[0][16].algo == 5);
    m.algo = 6; memcpy(a + 7, &m, 8);
    CHECK(call(&c, w, IBLUE_CMD_SET_MAP, a, 15, &o, &ol) == IBLUE_RES_BAD_ARG);

    CHECK(call(&c, w, 0x7F, a, 0, &o, &ol) == IBLUE_RES_BAD_CMD);

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
