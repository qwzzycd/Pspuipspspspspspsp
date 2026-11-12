#include <stdlib.h>
/* Renders the launcher UI on a PC into PPM files (no PSP needed).
 * Build: gcc -O1 -Wall -I src tools/preview.c src/ui.c src/gfx.c -lm -o preview
 */
#include "ui.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>

static uint32_t fb[SCR_W * SCR_H];

static const struct { const char *n; int k; uint64_t s; } FAKE[] = {
    { "Crisis Core - Final Fantasy VII", K_ZSO, 612ULL << 20 },
    { "God of War - Chains of Olympus", K_ZSO, 540ULL << 20 },
    { "Grand Theft Auto - Liberty City Stories", K_CSO, 790ULL << 20 },
    { "Monster Hunter Freedom Unite", K_ZSO, 905ULL << 20 },
    { "Persona 3 Portable", K_ISO, 1650ULL << 20 },
    { "Tekken 6", K_ZSO, 780ULL << 20 },
    { "Need for Speed - Most Wanted 5-1-0", K_CSO, 411ULL << 20 },
    { "Patapon 2", K_ZSO, 196ULL << 20 },
    { "PPSSPP Gold", K_APP, 14ULL << 20 },
    { "ARK_Loader", K_APP, 2ULL << 20 },
    { "Daxter", K_ZSO, 330ULL << 20 },
    { "Ys vs Sora no Kiseki", K_ISO, 1100ULL << 20 },
};

int plat_scan(Game *o, int max)
{
    int n = (int)(sizeof FAKE / sizeof FAKE[0]);
    if (getenv("EMPTY")) return 0;
    for (int i = 0; i < n && i < max; i++) {
        snprintf(o[i].name, sizeof o[i].name, "%s", FAKE[i].n);
        snprintf(o[i].path, sizeof o[i].path, "ms0:/ISO/%s.iso", FAKE[i].n);
        o[i].kind = FAKE[i].k;
        o[i].size = FAKE[i].s;
    }
    return n;
}
void plat_status(int *p, int *c, int *h, int *m) { *p = 78; *c = 0; *h = 21; *m = 45; }
unsigned plat_ram_free_kb(void) { return 21850; }
unsigned plat_ram_block_kb(void) { return 21700; }
int plat_ms_info(uint64_t *f, uint64_t *t) { *t = 31ULL << 30; *f = 12ULL << 30; return 0; }
void plat_cpu_set(int p) { (void)p; }
int plat_launch(const Game *g) { (void)g; return -1; }
int plat_clean(int d, unsigned *f, uint64_t *b) { (void)d; *f = 14; *b = 380ULL << 10; return 0; }
void plat_cfg_load(Config *c) { c->clock = 1; c->last = 0; }
void plat_cfg_save(const Config *c) { (void)c; }
void plat_quit(void) {}

static void shot(const char *name)
{
    ui_draw(fb, SCR_W);
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        uint32_t p = fb[i];
        fputc(p & 0xFF, f); fputc((p >> 8) & 0xFF, f); fputc((p >> 16) & 0xFF, f);
    }
    fclose(f);
}

int main(void)
{
    ui_init();
    if (getenv("EMPTY")) { shot("out/empty.ppm"); return 0; }
    shot("out/games_0.ppm");
    for (int i = 0; i < 8; i++) ui_input(BTN_DOWN);
    shot("out/games_8.ppm");
    ui_input(BTN_R);
    shot("out/system_0.ppm");
    ui_input(BTN_RIGHT);
    ui_input(BTN_DOWN); ui_input(BTN_DOWN); ui_input(BTN_DOWN);
    ui_input(BTN_CROSS);
    shot("out/system_clean.ppm");
    ui_input(BTN_L);
    ui_input(BTN_CROSS);
    shot("out/launching.ppm");
    return 0;
}
