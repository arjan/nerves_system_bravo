/* Draw the kernel log and /diag.log on the framebuffer. The stock HTC kernel
 * has no fbcon, so this is the only way to see anything on the screen.
 * Static, ~1 frame per second, RGB565, double buffered (yoffset flip). */
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/klog.h>
#include <sys/mman.h>
#include <unistd.h>
#include "font8x16.h"

static unsigned char *fb;
static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;
static int cols, rows, page;

static void put(int col, int row, unsigned char c, unsigned short fg)
{
    for (int y = 0; y < 16; y++) {
        unsigned char bits = font8x16[c][y];
        unsigned short *line = (unsigned short *)(fb + (size_t)(page * vi.yres + row * 16 + y) * fi.line_length);
        for (int x = 0; x < 8; x++)
            line[col * 8 + x] = (bits & (0x80 >> x)) ? fg : 0;
    }
}

static void clear(void)
{
    for (unsigned y = 0; y < vi.yres; y++)
        memset(fb + (size_t)(page * vi.yres + y) * fi.line_length, 0, fi.line_length);
}

/* Draw the last `n` lines of text into rows starting at `row0`. */
static void draw_tail(const char *text, int row0, int n, unsigned short fg)
{
    const char *lines[512]; int cnt = 0;
    const char *p = text;
    while (*p && cnt < 512) {
        lines[cnt++] = p;
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    int start = cnt > n ? cnt - n : 0;
    for (int i = start; i < cnt; i++) {
        const char *l = lines[i];
        /* strip the "<6>[ 1.2]" style priority prefix */
        if (l[0] == '<' && l[2] == '>') l += 3;
        int row = row0 + (i - start);
        for (int c = 0; c < cols && l[c] && l[c] != '\n'; c++)
            put(c, row, (unsigned char)l[c], fg);
    }
}

int main(void)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) { perror("fb0"); return 1; }
    ioctl(fd, FBIOGET_VSCREENINFO, &vi);
    ioctl(fd, FBIOGET_FSCREENINFO, &fi);
    fb = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) { perror("mmap"); return 1; }
    cols = vi.xres / 8; rows = vi.yres / 16;
    int pages = vi.yres_virtual / vi.yres;
    if (pages < 1) pages = 1;
    static char klog[1 << 16], diag[4096];
    int tick = 0;
    for (;;) {
        page = pages > 1 ? (tick & 1) : 0;
        clear();
        int n = klogctl(3, klog, sizeof(klog) - 1);
        klog[n > 0 ? n : 0] = 0;
        draw_tail(klog, 0, rows - 14, 0xFFFF);
        FILE *f = fopen("/diag.log", "r");
        size_t m = 0;
        if (f) { m = fread(diag, 1, sizeof(diag) - 1, f); fclose(f); }
        diag[m] = 0;
        char hdr[64]; snprintf(hdr, sizeof hdr, "--- bravo-diag t=%ds ---", tick);
        for (int c = 0; hdr[c]; c++) put(c, rows - 13, hdr[c], 0x07E0);
        draw_tail(diag, rows - 12, 12, 0x07FF);
        vi.yoffset = page * vi.yres;
        ioctl(fd, FBIOPUT_VSCREENINFO, &vi);
        tick++;
        sleep(1);
    }
}
