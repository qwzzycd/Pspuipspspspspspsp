#ifndef UI_H
#define UI_H

#include <stdint.h>

enum { K_ISO, K_CSO, K_ZSO, K_APP };

typedef struct {
    char name[72];
    char path[160];
    int kind;
    uint64_t size;
} Game;

typedef struct {
    int clock;   /* 0 = 133, 1 = 222, 2 = 333 MHz */
    int last;    /* last selected game index */
} Config;

enum {
    BTN_UP = 1 << 0, BTN_DOWN = 1 << 1, BTN_LEFT = 1 << 2, BTN_RIGHT = 1 << 3,
    BTN_CROSS = 1 << 4, BTN_CIRCLE = 1 << 5, BTN_TRIANGLE = 1 << 6, BTN_SQUARE = 1 << 7,
    BTN_L = 1 << 8, BTN_R = 1 << 9, BTN_START = 1 << 10, BTN_SELECT = 1 << 11
};

/* ---- implemented by the platform layer (platform_psp.c / host preview) ---- */
int      plat_scan(Game *out, int max);                       /* returns number of games */
void     plat_status(int *pct, int *charging, int *hour, int *min);
unsigned plat_ram_free_kb(void);
unsigned plat_ram_block_kb(void);
int      plat_ms_info(uint64_t *free_b, uint64_t *total_b);   /* 0 = ok */
void     plat_cpu_set(int profile);
int      plat_launch(const Game *g);                          /* returns only on failure */
int      plat_clean(int do_delete, unsigned *files, uint64_t *bytes);
void     plat_cfg_load(Config *c);
void     plat_cfg_save(const Config *c);
void     plat_quit(void);

/* ---- UI ---- */
void ui_init(void);
int  ui_input(unsigned pressed);          /* returns 1 if a redraw is needed */
int  ui_tick(void);                       /* call once per frame; returns 1 if a redraw is needed */
void ui_draw(uint32_t *fb, int stride);

#endif
