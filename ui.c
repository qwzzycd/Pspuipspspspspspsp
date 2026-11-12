#include "ui.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>

#define MAX_GAMES 512
#define ROWS      6
#define LIST_Y    50
#define ROW_H     30
#define ROW_X     12
#define ROW_W     456
#define SYS_ROWS  6

/* palette */
#define C_BG      RGB(11, 13, 18)
#define C_BAR     RGB(16, 19, 27)
#define C_LINE    RGB(27, 32, 45)
#define C_SEL     RGB(29, 38, 60)
#define C_PANEL   RGB(24, 29, 43)
#define C_ACCENT  RGB(124, 156, 255)
#define C_TEXT    RGB(232, 236, 244)
#define C_SUB     RGB(142, 151, 172)
#define C_DIM     RGB(84, 93, 114)
#define C_GOOD    RGB(91, 227, 160)
#define C_WARN    RGB(255, 184, 107)
#define C_BAD     RGB(255, 107, 122)
#define C_DARK    RGB(11, 13, 18)

static Game games[MAX_GAMES];
static int ngames, sel, top;
static Config cfg;
static int tab;              /* 0 = Games, 1 = System */
static int ssel;             /* selected row on System tab */
static char toast[80];
static int toast_t;
static int launching;        /* 1 = requested, 2 = launch on next frame */
static int clean_stage;      /* 0 idle, 1 scanned, 2 cleaned */
static unsigned clean_files;
static uint64_t clean_bytes;
static int bat, chg, hh, mm;
static unsigned ram_kb;
static uint64_t ms_free, ms_total;
static int ms_ok;
static int tick_n;

static const int CLOCKS[3] = { 133, 222, 333 };

/* ------------------------------------------------------------------ helpers */

static void set_toast(const char *s)
{
    snprintf(toast, sizeof toast, "%s", s);
    toast_t = 150;
}

static void fmt_size(char *o, size_t n, uint64_t b)
{
    if (b >= (1ULL << 30)) {
        unsigned t = (unsigned)((b * 10) >> 30);
        snprintf(o, n, "%u.%u GB", t / 10, t % 10);
    } else if (b >= (1ULL << 20)) {
        snprintf(o, n, "%u MB", (unsigned)(b >> 20));
    } else {
        snprintf(o, n, "%u KB", (unsigned)(b >> 10));
    }
}

static void fix_scroll(void)
{
    if (sel < top) top = sel;
    if (sel >= top + ROWS) top = sel - ROWS + 1;
    if (top < 0) top = 0;
    if (ngames > ROWS && top > ngames - ROWS) top = ngames - ROWS;
    if (ngames <= ROWS) top = 0;
}

static void refresh_ms(void)
{
    ms_ok = (plat_ms_info(&ms_free, &ms_total) == 0 && ms_total > 0);
}

static int refresh_status(void)
{
    int p, c, h, m;
    unsigned r = plat_ram_free_kb();
    plat_status(&p, &c, &h, &m);
    int changed = (p != bat || c != chg || h != hh || m != mm || (r / 100) != (ram_kb / 100));
    bat = p; chg = c; hh = h; mm = m; ram_kb = r;
    return changed;
}

static void rescan(int restore)
{
    ngames = plat_scan(games, MAX_GAMES);
    if (ngames < 0) ngames = 0;
    if (restore && cfg.last >= 0 && cfg.last < ngames) sel = cfg.last;
    if (sel >= ngames) sel = ngames ? ngames - 1 : 0;
    fix_scroll();
}

/* ------------------------------------------------------------------ init */

void ui_init(void)
{
    cfg.clock = 1;
    cfg.last = 0;
    plat_cfg_load(&cfg);
    if (cfg.clock < 0 || cfg.clock > 2) cfg.clock = 1;
    plat_cpu_set(cfg.clock);
    refresh_status();
    refresh_ms();
    rescan(1);
}

/* ------------------------------------------------------------------ actions */

static void change_clock(int d)
{
    int c = cfg.clock + d;
    if (c < 0 || c > 2) return;
    cfg.clock = c;
    plat_cpu_set(c);
    plat_cfg_save(&cfg);
}

static void do_clean(void)
{
    unsigned f = 0;
    uint64_t b = 0;
    if (clean_stage == 1 && clean_files > 0) {
        plat_clean(1, &f, &b);
        clean_files = f;
        clean_bytes = b;
        clean_stage = 2;
        refresh_ms();
    } else {
        plat_clean(0, &f, &b);
        clean_files = f;
        clean_bytes = b;
        clean_stage = 1;
    }
}

static void do_launch_now(void)
{
    cfg.last = sel;
    plat_cfg_save(&cfg);
    int r = plat_launch(&games[sel]);
    char b[48];
    snprintf(b, sizeof b, "Launch failed (0x%08X)", (unsigned)r);
    set_toast(b);
}

/* ------------------------------------------------------------------ input */

int ui_input(unsigned p)
{
    int d = 0;
    if (launching) return 0;

    if (p & (BTN_L | BTN_R)) {
        tab = (p & BTN_R) ? 1 : 0;
        if (tab == 1) { refresh_ms(); refresh_status(); }
        return 1;
    }

    if (tab == 0) {
        if (p & BTN_UP)   { if (ngames) sel = (sel + ngames - 1) % ngames; d = 1; }
        if (p & BTN_DOWN) { if (ngames) sel = (sel + 1) % ngames; d = 1; }
        if (p & BTN_LEFT)  { sel -= ROWS; if (sel < 0) sel = 0; d = 1; }
        if (p & BTN_RIGHT) { sel += ROWS; if (sel >= ngames) sel = ngames ? ngames - 1 : 0; d = 1; }
        if (p & BTN_SELECT) { rescan(0); set_toast("Library updated"); d = 1; }
        if ((p & BTN_CROSS) && ngames) { launching = 1; set_toast("Launching..."); d = 1; }
        fix_scroll();
    } else {
        if (p & BTN_UP)   { ssel = (ssel + SYS_ROWS - 1) % SYS_ROWS; d = 1; }
        if (p & BTN_DOWN) { ssel = (ssel + 1) % SYS_ROWS; d = 1; }
        if (ssel == 0) {
            if (p & BTN_LEFT)  { change_clock(-1); d = 1; }
            if (p & BTN_RIGHT) { change_clock(+1); d = 1; }
            if (p & BTN_CROSS) { cfg.clock = (cfg.clock + 1) % 3; plat_cpu_set(cfg.clock); plat_cfg_save(&cfg); d = 1; }
        }
        if (p & BTN_CROSS) {
            if (ssel == 3) { do_clean(); d = 1; }
            if (ssel == 4) { rescan(0); set_toast("Library updated"); d = 1; }
            if (ssel == 5) { plat_quit(); }
        }
        if (p & (BTN_UP | BTN_DOWN)) { if (ssel != 3) clean_stage = 0; }
    }
    return d;
}

int ui_tick(void)
{
    int d = 0;
    if (launching == 2) { launching = 0; do_launch_now(); d = 1; }
    else if (launching == 1) { launching = 2; }

    if (toast_t > 0 && --toast_t == 0) d = 1;

    if (++tick_n >= 60) {
        tick_n = 0;
        if (refresh_status()) d = 1;
    }
    return d;
}

/* ------------------------------------------------------------------ drawing */

static void draw_top(void)
{
    gfx_rect(0, 0, SCR_W, 40, C_BAR);
    gfx_rect(0, 40, SCR_W, 1, C_LINE);
    gfx_text(16, 14, "NOVA", C_ACCENT, 2);

    static const char *names[2] = { "Games", "System" };
    int x = 104;
    for (int t = 0; t < 2; t++) {
        int w = gfx_text_w(names[t], 2) + 24;
        if (t == tab) gfx_rrect(x, 7, w, 26, 13, C_SEL);
        gfx_text(x + 12, 14, names[t], t == tab ? C_TEXT : C_DIM, 2);
        x += w + 6;
    }

    char b[16];
    snprintf(b, sizeof b, "%02d:%02d", hh, mm);
    int cw = gfx_text_w(b, 2);
    gfx_text(SCR_W - 16 - cw, 14, b, C_TEXT, 2);

    snprintf(b, sizeof b, "%d%%", bat);
    int bw = gfx_text_w(b, 2);
    int bx = SCR_W - 16 - cw - 18 - bw;
    uint32_t bc = chg ? C_GOOD : (bat <= 15 ? C_BAD : (bat <= 30 ? C_WARN : C_SUB));
    gfx_text(bx, 14, b, bc, 2);

    int ix = bx - 32;
    gfx_rrect(ix, 15, 22, 12, 3, C_DIM);
    gfx_rrect(ix + 2, 17, 18, 8, 2, C_BAR);
    int fw = 18 * bat / 100;
    if (fw < 2 && bat > 0) fw = 2;
    if (fw > 0) gfx_rrect(ix + 2, 17, fw, 8, 2, bc == C_SUB ? C_TEXT : bc);
    gfx_rect(ix + 22, 19, 2, 4, C_DIM);
}

static void hint(int *x, const char *key, const char *label)
{
    *x += gfx_text(*x, 258, key, C_TEXT, 1);
    *x += 4;
    *x += gfx_text(*x, 258, label, C_DIM, 1);
    *x += 14;
}

static void draw_footer(const char *line1, int mode)
{
    char b[96];
    gfx_rect(0, 238, SCR_W, 34, C_BAR);
    gfx_rect(0, 238, SCR_W, 1, C_LINE);
    gfx_fit(b, sizeof b, line1, 456, 1);
    gfx_text(12, 245, b, C_SUB, 1);
    int x = 12;
    if (mode == 0) {
        hint(&x, "X", "Launch");
        hint(&x, "SELECT", "Rescan");
        hint(&x, "L/R", "Tabs");
        hint(&x, "LEFT/RIGHT", "Page");
    } else {
        hint(&x, "X", "Select");
        hint(&x, "UP/DOWN", "Move");
        hint(&x, "LEFT/RIGHT", "Change");
        hint(&x, "L/R", "Tabs");
    }
}

static void row_bg(int i, int selected)
{
    int y = LIST_Y + i * ROW_H;
    if (!selected) return;
    gfx_rrect(ROW_X, y, ROW_W, 26, 8, C_SEL);
    gfx_rrect(ROW_X + 6, y + 7, 3, 12, 1, C_ACCENT);
}

static void draw_games(void)
{
    if (!ngames) {
        const char *a = "No games found";
        const char *b = "Put ISO / CSO / ZSO into ms0:/ISO";
        gfx_text((SCR_W - gfx_text_w(a, 2)) / 2, 108, a, C_TEXT, 2);
        gfx_text((SCR_W - gfx_text_w(b, 1)) / 2, 134, b, C_DIM, 1);
        draw_footer("", 0);
        return;
    }

    static const char *kt[4] = { "ISO", "CSO", "ZSO", "APP" };
    const uint32_t kc[4] = { C_ACCENT, C_SUB, C_GOOD, C_WARN };

    for (int i = 0; i < ROWS; i++) {
        int idx = top + i;
        if (idx >= ngames) break;
        const Game *g = &games[idx];
        int y = LIST_Y + i * ROW_H;
        int s = (idx == sel);
        row_bg(i, s);

        gfx_rrect(24, y + 6, 34, 14, 4, s ? C_BG : C_PANEL);
        gfx_text(24 + (34 - gfx_text_w(kt[g->kind], 1)) / 2, y + 10, kt[g->kind], kc[g->kind], 1);

        char sz[16];
        fmt_size(sz, sizeof sz, g->size);
        int szw = gfx_text_w(sz, 1);
        gfx_text(ROW_X + ROW_W - 12 - szw, y + 10, sz, C_DIM, 1);

        char t[80];
        gfx_fit(t, sizeof t, g->name, ROW_X + ROW_W - 12 - szw - 12 - 70, 2);
        gfx_text(70, y + 6, t, s ? C_TEXT : C_SUB, 2);
    }

    if (ngames > ROWS) {
        int th = ROWS * ROW_H - 4;
        int kh = th * ROWS / ngames;
        if (kh < 14) kh = 14;
        int ky = LIST_Y + (th - kh) * top / (ngames - ROWS);
        gfx_rrect(473, LIST_Y, 3, th, 1, C_PANEL);
        gfx_rrect(473, ky, 3, kh, 1, C_DIM);
    }

    draw_footer(games[sel].name, 0);
}

static void sys_label(int i, const char *label)
{
    int y = LIST_Y + i * ROW_H;
    gfx_text(28, y + 6, label, i == ssel ? C_TEXT : C_SUB, 2);
}

static void draw_system(void)
{
    char b[64], c[32];
    for (int i = 0; i < SYS_ROWS; i++) row_bg(i, i == ssel);

    /* 0: CPU clock */
    sys_label(0, "CPU clock");
    {
        int y = LIST_Y;
        int x0 = ROW_X + ROW_W - 12 - (3 * 52 + 2 * 4);
        for (int p = 0; p < 3; p++) {
            int x = x0 + p * 56;
            int on = (p == cfg.clock);
            gfx_rrect(x, y + 3, 52, 20, 7, on ? C_ACCENT : (ssel == 0 ? C_BG : C_PANEL));
            snprintf(b, sizeof b, "%d", CLOCKS[p]);
            gfx_text(x + (52 - gfx_text_w(b, 2)) / 2, y + 6, b, on ? C_DARK : C_SUB, 2);
        }
    }

    /* 1: RAM */
    sys_label(1, "Free RAM");
    gfx_text(150, LIST_Y + ROW_H + 10, "freed on every launch", C_DIM, 1);
    snprintf(b, sizeof b, "%u.%u MB", ram_kb / 1024, (ram_kb % 1024) * 10 / 1024);
    gfx_text(ROW_X + ROW_W - 12 - gfx_text_w(b, 2), LIST_Y + ROW_H + 6, b, C_TEXT, 2);

    /* 2: Memory Stick */
    sys_label(2, "Memory Stick");
    {
        int y = LIST_Y + 2 * ROW_H;
        if (ms_ok) {
            fmt_size(c, sizeof c, ms_free);
            gfx_text(ROW_X + ROW_W - 12 - gfx_text_w(c, 2), y + 6, c, C_TEXT, 2);
            int used = (int)(150 - (ms_free * 150 / ms_total));
            if (used < 0) used = 0;
            if (used > 150) used = 150;
            uint32_t uc = (used > 135) ? C_WARN : C_ACCENT;
            gfx_rrect(196, y + 10, 150, 6, 3, ssel == 2 ? C_BG : C_PANEL);
            if (used > 0) gfx_rrect(196, y + 10, used < 6 ? 6 : used, 6, 3, uc);
        } else {
            gfx_text(ROW_X + ROW_W - 12 - gfx_text_w("n/a", 2), y + 6, "n/a", C_DIM, 2);
        }
    }

    /* 3: Clean junk */
    sys_label(3, "Clean junk");
    {
        int y = LIST_Y + 3 * ROW_H;
        if (clean_stage == 0) {
            gfx_text(ROW_X + ROW_W - 12 - gfx_text_w("Scan", 2), y + 6, "Scan", C_ACCENT, 2);
        } else if (clean_stage == 1) {
            if (clean_files == 0) {
                gfx_text(ROW_X + ROW_W - 12 - gfx_text_w("Nothing to clean", 2), y + 6, "Nothing to clean", C_GOOD, 2);
            } else {
                char sz[16];
                fmt_size(sz, sizeof sz, clean_bytes);
                snprintf(b, sizeof b, "%u files, %s - press X", clean_files, sz);
                gfx_text(ROW_X + ROW_W - 12 - gfx_text_w(b, 1), y + 10, b, C_WARN, 1);
            }
        } else {
            snprintf(b, sizeof b, "Removed %u files", clean_files);
            gfx_text(ROW_X + ROW_W - 12 - gfx_text_w(b, 2), y + 6, b, C_GOOD, 2);
        }
    }

    /* 4: Rescan */
    sys_label(4, "Rescan games");
    snprintf(b, sizeof b, "%d found", ngames);
    gfx_text(ROW_X + ROW_W - 12 - gfx_text_w(b, 2), LIST_Y + 4 * ROW_H + 6, b, C_SUB, 2);

    /* 5: Exit */
    sys_label(5, "Exit to XMB");

    static const char *info[SYS_ROWS] = {
        "LEFT/RIGHT: launcher clock (133 / 222 / 333 MHz)",
        "Launching a game restarts memory - nothing to clean by hand",
        "Free space on ms0:",
        "Removes Thumbs.db, .DS_Store, ._* and .tmp files from /ISO and /PSP/GAME",
        "Re-read /ISO and /PSP/GAME",
        "Close NOVA and go back to the XMB"
    };
    draw_footer(info[ssel], 1);
}

static void draw_toast(void)
{
    if (toast_t <= 0) return;
    int w = gfx_text_w(toast, 2) + 32;
    int x = (SCR_W - w) / 2;
    gfx_rrect(x, 200, w, 28, 14, C_ACCENT);
    gfx_text(x + 16, 207, toast, C_DARK, 2);
}

void ui_draw(uint32_t *fb, int stride)
{
    gfx_target(fb, stride);
    gfx_fill(C_BG);
    if (tab == 0) draw_games(); else draw_system();
    draw_top();
    draw_toast();
}
