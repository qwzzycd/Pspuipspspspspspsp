/* Generates assets/ICON0.PNG source (PPM) using the launcher's own renderer. */
#include "gfx.h"
#include <stdio.h>
static uint32_t fb[SCR_W * SCR_H];
int main(void)
{
    gfx_target(fb, SCR_W);
    gfx_fill(RGB(11, 13, 18));
    gfx_rrect(4, 4, 136, 72, 12, RGB(24, 29, 43));
    gfx_text(24, 20, "NOVA", RGB(124, 156, 255), 4);
    gfx_rrect(24, 56, 96, 3, 1, RGB(124, 156, 255));
    gfx_text(24, 64, "ARK-5 LAUNCHER", RGB(142, 151, 172), 1);
    FILE *f = fopen("out/icon.ppm", "wb");
    fprintf(f, "P6\n%d %d\n255\n", SCR_W, SCR_H);
    for (int i = 0; i < SCR_W * SCR_H; i++) { uint32_t p = fb[i]; fputc(p & 255, f); fputc((p >> 8) & 255, f); fputc((p >> 16) & 255, f); }
    fclose(f);
    return 0;
}
