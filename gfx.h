#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stddef.h>

#define SCR_W 480
#define SCR_H 272

/* PSP 8888 format: memory order R,G,B,A -> little-endian u32 = A<<24 | B<<16 | G<<8 | R */
#define RGB(r, g, b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

void gfx_target(uint32_t *px, int stride);               /* where to draw, stride in pixels */
void gfx_fill(uint32_t c);
void gfx_rect(int x, int y, int w, int h, uint32_t c);
void gfx_rrect(int x, int y, int w, int h, int r, uint32_t c);   /* anti-aliased corners */
int  gfx_text(int x, int y, const char *s, uint32_t c, int scale); /* returns width */
int  gfx_text_w(const char *s, int scale);
void gfx_fit(char *out, size_t n, const char *s, int maxw, int scale); /* truncate with ".." */

#endif
