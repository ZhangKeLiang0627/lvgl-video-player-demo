#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Create a Linux framebuffer display whose final present is rotated by the
 * Rockchip RGA 2D unit instead of LVGL's CPU transpose.
 *
 * Why: lv_display_set_rotation() makes LVGL's fbdev driver rotate the whole
 * framebuffer (including the video) in software on every flush. On RK3566 that
 * collapses landscape playback to ~1-2 fps. Here LVGL renders at the logical
 * (post-rotation) size with rotation=0 (flush is a plain memcpy into the draw
 * buffer), and this driver rotates the full composited frame with RGA (<2 ms)
 * when copying it to /dev/fb0. Works for both soft- and hard-decode builds
 * because RGA is independent of the VPU.
 *
 * angle: 0 / 90 / 180 / 270 (clockwise, matching lv_display_set_rotation).
 * Returns the LVGL display (logical size already set). */
lv_display_t * disp_rga_create(const char * fbdev, int angle);

/* Physical panel size (pre-rotation). Call after disp_rga_create. */
void disp_rga_get_phys_size(int * w, int * h);

#ifdef __cplusplus
}
#endif
