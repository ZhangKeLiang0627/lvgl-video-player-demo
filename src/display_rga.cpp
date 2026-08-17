#include "display_rga.h"

#if PLAYER_USE_SDL
/* SDL simulator uses lv_sdl_window_create(); the RGA framebuffer driver is
 * only compiled into on-device (Linux fbdev) builds. Provide empty stubs so
 * the source file can still be globbed by CMake without requiring librga. */
lv_display_t * disp_rga_create(const char * fbdev, int angle)
{
    (void)fbdev;
    (void)angle;
    return NULL;
}

void disp_rga_get_phys_size(int * w, int * h)
{
    if (w) *w = 0;
    if (h) *h = 0;
}

#else /* Linux framebuffer device build with Rockchip RGA */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "rga/rga.h"
#include "rga/im2d.h"

typedef struct {
    int          fd;
    void *       mem;          /* mmap of /dev/fb0 (physical panel) */
    int          phys_w;       /* panel width  (e.g. 800) */
    int          phys_h;       /* panel height (e.g. 1280) */
    int          phys_stride;  /* panel stride in pixels (line_length / 4) */
    int          log_w;        /* logical width  (e.g. 1280 for 90/270) */
    int          log_h;        /* logical height (e.g. 800  for 90/270) */
    int          angle;
    uint8_t *    draw_buf;
} rga_disp_t;

static rga_disp_t g_rga;

/* Real-time clock tick source (replaces the one the stock fbdev driver
 * installs). Needed because LV_USE_OS=0 and no LV_TICK_CUSTOM. */
static uint32_t tick_get_cb(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int rga_rot_flag(int angle)
{
    switch (angle) {
        case 90:  return IM_HAL_TRANSFORM_ROT_90;
        case 180: return IM_HAL_TRANSFORM_ROT_180;
        case 270: return IM_HAL_TRANSFORM_ROT_270;
        default:  return 0;
    }
}

/* Present the composited LVGL frame (logical) to the panel, rotated by RGA. */
static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    (void)area; /* full-refresh: always rotate the entire frame */
    rga_disp_t * d = &g_rga;
    int rot = rga_rot_flag(d->angle);

    if (rot == 0) {
        /* Identity: logical == physical size, just memcpy. */
        size_t n = (size_t)d->phys_w * d->phys_h * 4;
        memcpy(d->mem, px_map, n);
        lv_display_flush_ready(disp);
        return;
    }

    rga_buffer_t src = wrapbuffer_virtualaddr_t(px_map, d->log_w, d->log_h,
                                                d->log_w, d->log_h,
                                                RK_FORMAT_BGRA_8888);
    rga_buffer_t dst = wrapbuffer_virtualaddr_t(d->mem, d->phys_w, d->phys_h,
                                                d->phys_stride, d->phys_h,
                                                RK_FORMAT_BGRA_8888);
    IM_STATUS st = imrotate(src, dst, rot, 1);
    if (st != IM_STATUS_SUCCESS)
        LV_LOG_WARN("rga present rotate failed (%d)", (int)st);
    lv_display_flush_ready(disp);
}

lv_display_t * disp_rga_create(const char * fbdev, int angle)
{
    int fd = open(fbdev, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        perror("disp_rga: open fb");
        return NULL;
    }
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0) {
        perror("disp_rga: fb ioctl");
        close(fd);
        return NULL;
    }
    int phys_w = v.xres;
    int phys_h = v.yres;
    int stride_px = f.line_length / (v.bits_per_pixel / 8);
    void * mem = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mem == MAP_FAILED) {
        perror("disp_rga: mmap fb");
        close(fd);
        return NULL;
    }

    int log_w = phys_w, log_h = phys_h;
    if (angle == 90 || angle == 270) { log_w = phys_h; log_h = phys_w; }

    memset(&g_rga, 0, sizeof(g_rga));
    g_rga.fd = fd;
    g_rga.mem = mem;
    g_rga.phys_w = phys_w;
    g_rga.phys_h = phys_h;
    g_rga.phys_stride = stride_px;
    g_rga.log_w = log_w;
    g_rga.log_h = log_h;
    g_rga.angle = angle;

    lv_tick_set_cb(tick_get_cb);

    lv_display_t * disp = lv_display_create(log_w, log_h);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);

    size_t buf_size = (size_t)log_w * log_h * 4;
    g_rga.draw_buf = (uint8_t *)malloc(buf_size);
    if (!g_rga.draw_buf) {
        LV_LOG_ERROR("disp_rga: draw buffer alloc failed (%dx%d)", log_w, log_h);
        return NULL;
    }
    lv_display_set_buffers(disp, g_rga.draw_buf, NULL, buf_size,
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_user_data(disp, &g_rga);
    return disp;
}

void disp_rga_get_phys_size(int * w, int * h)
{
    if (w) *w = g_rga.phys_w;
    if (h) *h = g_rga.phys_h;
}

#endif /* PLAYER_USE_SDL */
