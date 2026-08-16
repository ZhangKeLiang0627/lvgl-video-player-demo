/*
 * LVGL configuration for the lvgl-video-player project.
 *
 * This file targets the RK3566 / LubanCat board with an 800x1280 portrait
 * DSI panel driven by the Linux framebuffer (/dev/fb0, 32bpp) and an rk809
 * audio codec. It is consumed by the on-device Makefile build
 * (-DLV_CONF_INCLUDE_SIMPLE) and also by the CMake build via LV_CONF_PATH.
 *
 * For a different board, adjust LV_COLOR_DEPTH / LV_MEM_SIZE / backends.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ===== basic ===== */
#define LV_USE_OS           0          /* no OS thread, manual lv_tick_inc */
#define LV_COLOR_DEPTH      32         /* fb0 is 32bpp */
#define LV_COLOR_16_SWAP    0
#define LV_DPI_DEF          160

/* ===== display backend: Linux framebuffer (/dev/fb0) ===== */
#define LV_USE_LINUX_FBDEV  1

/* ===== video playback: LVGL official ffmpeg soft decode ===== */
#define LV_USE_FFMPEG       1
#define LV_FFMPEG_DUMP_FORMAT       0
#define LV_FFMPEG_PLAYER_USE_LV_FS  0

/* ===== system monitors: FPS/CPU + memory ===== */
#define LV_USE_OBSERVER     1          /* sysmon depends on observer */
#define LV_USE_SYSMON       1
#define LV_USE_PERF_MONITOR 1          /* frame-rate / CPU monitor */
#define LV_USE_MEM_MONITOR  1          /* memory monitor */

/* ===== input: touchscreen via evdev ===== */
#define LV_USE_EVDEV        1

/* ===== memory / refresh =====
 * LV_MEM_SIZE MUST be large enough for the full-frame display buffer
 * (w*h*bpp = 800*1280*4 ~= 4 MB) plus LVGL objects, or lv_display_set_buffers
 * will spin inside the allocator. 32 MB is comfortable for this panel. */
#define LV_MEM_SIZE         (32 * 1024 * 1024)
#define LV_MEM_ADR          0
#define LV_DEF_REFR_PERIOD   33

#endif /* LV_CONF_H */
