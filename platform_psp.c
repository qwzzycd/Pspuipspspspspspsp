/*
 * NOVA - minimal launcher for PSP (ARK-4 / ARK-5 CFW)
 * Platform layer: everything that touches the PSP.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psprtc.h>
#include <pspiofilemgr.h>
#include <psploadexec.h>
#include <psploadexec_kernel.h>

#ifndef NO_CFW
#include <systemctrl.h>   /* from psp-cfw-sdk: sctrl* functions */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "gfx.h"

PSP_MODULE_INFO("NOVA", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define BUF_W     512
#define FB_BYTES  (BUF_W * SCR_H * 4)
#define VRAM_BASE 0x04000000u

static char self_dir[128]  = "ms0:/PSP/GAME/NOVA";
static char self_name[64]  = "NOVA";

/* ------------------------------------------------------------ exit callback */

static int exit_cb(int a, int b, void *c)
{
    (void)a; (void)b; (void)c;
    sceKernelExitGame();
    return 0;
}

static int cb_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int id = sceKernelCreateCallback("nova_exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int t = sceKernelCreateThread("nova_cb", cb_thread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ------------------------------------------------------------ string helpers */

static int ci_eq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

static int ci_cmp(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return (unsigned char)x - (unsigned char)y;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* keep printable ASCII only; one '?' per UTF-8 character (the built-in font is ASCII) */
static void sanitize(char *dst, size_t n, const char *src)
{
    size_t o = 0;
    for (; *src && o + 1 < n; src++) {
        unsigned char c = (unsigned char)*src;
        if (c >= 32 && c < 127) dst[o++] = (char)c;
        else if (c >= 0xC0) dst[o++] = '?';
    }
    dst[o] = 0;
}

static int ext_kind(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return -1;
    if (ci_eq(dot, ".iso")) return K_ISO;
    if (ci_eq(dot, ".cso")) return K_CSO;
    if (ci_eq(dot, ".zso")) return K_ZSO;
    return -1;
}

/* ------------------------------------------------------------ library scan */

static Game *g_out;
static int g_n, g_max;

static void add_game(const char *name, const char *path, int kind, uint64_t size, int strip_ext)
{
    if (g_n >= g_max) return;
    Game *g = &g_out[g_n++];
    sanitize(g->name, sizeof g->name, name);
    if (strip_ext) {
        char *dot = strrchr(g->name, '.');
        if (dot) *dot = 0;
    }
    snprintf(g->path, sizeof g->path, "%s", path);
    g->kind = kind;
    g->size = size;
}

static void scan_images(const char *dir, int depth)
{
    SceUID d = sceIoDopen(dir);
    if (d < 0) return;
    SceIoDirent e;
    memset(&e, 0, sizeof e);
    while (sceIoDread(d, &e) > 0) {
        if (e.d_name[0] != '.') {
            char path[160];
            snprintf(path, sizeof path, "%s/%s", dir, e.d_name);
            if (FIO_S_ISDIR(e.d_stat.st_mode)) {
                if (depth < 2) scan_images(path, depth + 1);
            } else {
                int k = ext_kind(e.d_name);
                if (k >= 0) add_game(e.d_name, path, k, (uint64_t)e.d_stat.st_size, 1);
            }
        }
        memset(&e, 0, sizeof e);
    }
    sceIoDclose(d);
}

static void scan_apps(const char *dir)
{
    SceUID d = sceIoDopen(dir);
    if (d < 0) return;
    SceIoDirent e;
    memset(&e, 0, sizeof e);
    while (sceIoDread(d, &e) > 0) {
        if (FIO_S_ISDIR(e.d_stat.st_mode) &&
            e.d_name[0] != '.' && e.d_name[0] != '%' &&
            !strstr(e.d_name, "__SCE__") && !ci_eq(e.d_name, self_name)) {
            char path[160], eboot[180];
            SceIoStat st;
            snprintf(path, sizeof path, "%s/%s", dir, e.d_name);
            snprintf(eboot, sizeof eboot, "%s/EBOOT.PBP", path);
            if (sceIoGetstat(eboot, &st) >= 0)
                add_game(e.d_name, path, K_APP, (uint64_t)st.st_size, 0);
        }
        memset(&e, 0, sizeof e);
    }
    sceIoDclose(d);
}

static int game_cmp(const void *pa, const void *pb)
{
    const Game *a = (const Game *)pa, *b = (const Game *)pb;
    int aa = (a->kind == K_APP), bb = (b->kind == K_APP);
    if (aa != bb) return aa - bb;               /* games first, apps last */
    return ci_cmp(a->name, b->name);
}

int plat_scan(Game *out, int max)
{
    g_out = out; g_n = 0; g_max = max;
    scan_images("ms0:/ISO", 0);
    scan_apps("ms0:/PSP/GAME");
    qsort(out, (size_t)g_n, sizeof(Game), game_cmp);
    return g_n;
}

/* ------------------------------------------------------------ status */

void plat_status(int *pct, int *charging, int *hour, int *min)
{
    int p = scePowerGetBatteryLifePercent();
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    *pct = p;
    *charging = scePowerIsBatteryCharging() > 0;
    pspTime t;
    sceRtcGetCurrentClockLocalTime(&t);
    *hour = t.hour;
    *min = t.minutes;
}

unsigned plat_ram_free_kb(void)  { return (unsigned)sceKernelTotalFreeMemSize() / 1024; }
unsigned plat_ram_block_kb(void) { return (unsigned)sceKernelMaxFreeMemSize() / 1024; }

int plat_ms_info(uint64_t *free_b, uint64_t *total_b)
{
    /* devctl 0x02425818 fills {maxClusters, freeClusters, ?, sectorSize, sectorsPerCluster...}.
       The buffer is oversized on purpose: some firmwares write a 64-bit last field. */
    unsigned int info[8];
    unsigned int *pinfo = info;
    memset(info, 0, sizeof info);
    int r = sceIoDevctl("ms0:", 0x02425818, &pinfo, sizeof pinfo, NULL, 0);
    if (r < 0) return -1;
    uint64_t cluster = (uint64_t)info[3] * (uint64_t)info[4];
    if (!cluster) return -1;
    *total_b = (uint64_t)info[0] * cluster;
    *free_b  = (uint64_t)info[1] * cluster;
    return 0;
}

/* ------------------------------------------------------------ CPU */

void plat_cpu_set(int p)
{
    static const int mhz[3] = { 133, 222, 333 };
    static const int bus[3] = { 66, 111, 166 };
    if (p < 0 || p > 2) p = 1;
    scePowerSetClockFrequency(mhz[p], mhz[p], bus[p]);
#if defined(USE_HEN_SET_SPEED) && !defined(NO_CFW)
    sctrlHENSetSpeed(mhz[p], bus[p]);   /* clock for the next launched game (if your ARK exports it) */
#endif
}

/* ------------------------------------------------------------ launching */

int plat_launch(const Game *g)
{
    char eboot[200];
#ifndef NO_CFW
    struct SceKernelLoadExecVSHParam p;
    static const char boot[] = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";

    memset(&p, 0, sizeof p);
    p.size = sizeof p;
    p.key = "game";

    if (g->kind == K_APP) {
        snprintf(eboot, sizeof eboot, "%s/EBOOT.PBP", g->path);
        p.args = (SceSize)strlen(eboot) + 1;
        p.argp = eboot;
        return sctrlKernelLoadExecVSHWithApitype(0x141, eboot, &p);
    }

    p.args = sizeof boot;
    p.argp = (void *)boot;
    sctrlSESetBootConfFileIndex(MODE_INFERNO);
    sctrlSESetUmdFile((char *)g->path);
    sctrlSESetDiscType(0x10);                       /* PSP_UMD_TYPE_GAME */
    return sctrlKernelLoadExecVSHWithApitype(0x120, boot, &p);
#else
    /* no CFW: only plain homebrew can be started (PPSSPP / testing) */
    if (g->kind != K_APP) return -1;
    struct SceKernelLoadExecParam q;
    snprintf(eboot, sizeof eboot, "%s/EBOOT.PBP", g->path);
    memset(&q, 0, sizeof q);
    q.size = sizeof q;
    q.args = (SceSize)strlen(eboot) + 1;
    q.argp = eboot;
    q.key = NULL;
    return sceKernelLoadExec(eboot, &q);
#endif
}

void plat_quit(void) { sceKernelExitGame(); }

/* ------------------------------------------------------------ junk cleaner */

static int is_junk(const char *n)
{
    if (ci_eq(n, "Thumbs.db") || ci_eq(n, "desktop.ini") || ci_eq(n, ".DS_Store")) return 1;
    if (n[0] == '.' && n[1] == '_') return 1;
    size_t l = strlen(n);
    return l > 4 && ci_eq(n + l - 4, ".tmp");
}

static void clean_walk(const char *dir, int depth, int del, unsigned *files, uint64_t *bytes)
{
    SceUID d = sceIoDopen(dir);
    if (d < 0) return;
    SceIoDirent e;
    memset(&e, 0, sizeof e);
    while (sceIoDread(d, &e) > 0) {
        char path[200];
        snprintf(path, sizeof path, "%s/%s", dir, e.d_name);
        if (FIO_S_ISDIR(e.d_stat.st_mode)) {
            if (e.d_name[0] != '.' && depth < 4) clean_walk(path, depth + 1, del, files, bytes);
        } else if (is_junk(e.d_name)) {
            (*files)++;
            *bytes += (uint64_t)e.d_stat.st_size;
            if (del) sceIoRemove(path);
        }
        memset(&e, 0, sizeof e);
    }
    sceIoDclose(d);
}

int plat_clean(int do_delete, unsigned *files, uint64_t *bytes)
{
    *files = 0;
    *bytes = 0;
    clean_walk("ms0:/ISO", 0, do_delete, files, bytes);
    clean_walk("ms0:/PSP/GAME", 0, do_delete, files, bytes);
    return 0;
}

/* ------------------------------------------------------------ config */

static void cfg_path(char *o, size_t n) { snprintf(o, n, "%s/nova.cfg", self_dir); }

void plat_cfg_load(Config *c)
{
    char path[160], line[64];
    cfg_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        int v;
        if (sscanf(line, "clock=%d", &v) == 1) c->clock = v;
        else if (sscanf(line, "last=%d", &v) == 1) c->last = v;
    }
    fclose(f);
}

void plat_cfg_save(const Config *c)
{
    char path[160];
    cfg_path(path, sizeof path);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "clock=%d\nlast=%d\n", c->clock, c->last);
    fclose(f);
}

/* ------------------------------------------------------------ main loop */

static void init_self(int argc, char **argv)
{
    if (argc > 0 && argv && argv[0]) {
        const char *slash = strrchr(argv[0], '/');
        if (slash && slash > argv[0]) {
            size_t n = (size_t)(slash - argv[0]);
            if (n < sizeof self_dir) {
                memcpy(self_dir, argv[0], n);
                self_dir[n] = 0;
                const char *q = strrchr(self_dir, '/');
                snprintf(self_name, sizeof self_name, "%s", q ? q + 1 : self_dir);
            }
        }
    }
}

static unsigned map_buttons(const SceCtrlData *p)
{
    unsigned b = 0, s = p->Buttons;
    if (s & PSP_CTRL_UP)       b |= BTN_UP;
    if (s & PSP_CTRL_DOWN)     b |= BTN_DOWN;
    if (s & PSP_CTRL_LEFT)     b |= BTN_LEFT;
    if (s & PSP_CTRL_RIGHT)    b |= BTN_RIGHT;
    if (s & PSP_CTRL_CROSS)    b |= BTN_CROSS;
    if (s & PSP_CTRL_CIRCLE)   b |= BTN_CIRCLE;
    if (s & PSP_CTRL_TRIANGLE) b |= BTN_TRIANGLE;
    if (s & PSP_CTRL_SQUARE)   b |= BTN_SQUARE;
    if (s & PSP_CTRL_LTRIGGER) b |= BTN_L;
    if (s & PSP_CTRL_RTRIGGER) b |= BTN_R;
    if (s & PSP_CTRL_START)    b |= BTN_START;
    if (s & PSP_CTRL_SELECT)   b |= BTN_SELECT;
    if (p->Ly < 40)  b |= BTN_UP;
    if (p->Ly > 215) b |= BTN_DOWN;
    if (p->Lx < 40)  b |= BTN_LEFT;
    if (p->Lx > 215) b |= BTN_RIGHT;
    return b;
}

int main(int argc, char **argv)
{
    SceCtrlData pad;
    unsigned prev, last_dirs = 0;
    int rep = 0, back = 1, dirty = 1;

    setup_callbacks();
    init_self(argc, argv);

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sceDisplaySetMode(0, SCR_W, SCR_H);

    /* clear both buffers so the first flip is clean */
    gfx_target((uint32_t *)(0x40000000u | VRAM_BASE), BUF_W);
    gfx_fill(RGB(11, 13, 18));
    gfx_target((uint32_t *)(0x40000000u | (VRAM_BASE + FB_BYTES)), BUF_W);
    gfx_fill(RGB(11, 13, 18));

    ui_init();

    /* ignore buttons that were already held when the app started (e.g. the X that launched us) */
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = map_buttons(&pad);

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned b = map_buttons(&pad);
        unsigned pressed = b & ~prev;
        prev = b;

        unsigned dirs = b & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
        if (dirs && dirs == last_dirs) {
            rep++;
            if (rep > 20 && ((rep - 20) % 4) == 0) pressed |= dirs;   /* key repeat */
        } else {
            rep = 0;
        }
        last_dirs = dirs;

        if (pressed) dirty |= ui_input(pressed);
        dirty |= ui_tick();

        if (dirty) {                                  /* redraw only when something changed */
            ui_draw((uint32_t *)(0x40000000u | (VRAM_BASE + (unsigned)back * FB_BYTES)), BUF_W);
            sceDisplaySetFrameBuf((void *)(VRAM_BASE + (unsigned)back * FB_BYTES), BUF_W,
                                  PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
            back ^= 1;
            dirty = 0;
        }
        sceDisplayWaitVblankStart();
    }
    return 0;
}
