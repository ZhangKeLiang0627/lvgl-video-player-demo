/*
 * LVGL configuration for the lvgl-video-player project.
 *
 * Used by the CMake build (LVGL as a submodule). On the target device the
 * existing lv_conf.h inside the local LVGL tree is used instead, so this file
 * is only consulted when building from a fresh checkout via CMake.
 */
#if defined(LV_CONF_H) && (LV_CONF_H != 1)
    #error "lv_conf.h included multiple times"
#endif
#define LV_CONF_H 1

#define LV_USE_OS          LV_OS_NONE
#define LV_USE_DRAW_SW     1

/* Color depth of the display (the panel is 32bpp). */
#define LV_COLOR_DEPTH     32

/* Enable the FFmpeg video player library. */
#define LV_USE_FFMPEG       1

/* Linux framebuffer display + evdev touch input. */
#define LV_USE_LINUX_FBDEV  1
#define LV_USE_EVDEV        1

/* System monitors (FPS + memory) shown on screen. */
#define LV_USE_SYSMON       1
#define LV_USE_PERF_MONITOR 1
#define LV_USE_MEM_MONITOR  1

/* Observers (used by sysmon). */
#define LV_USE_OBSERVER     1

/* Tick period; we drive lv_tick_inc() ourselves from CLOCK_MONOTONIC. */
#ifndef LV_TICK_PERIOD_MS
    #define LV_TICK_PERIOD_MS 5
#endif

#define LV_MEM_CUSTOM       0
#define LV_USE_LOG          1
#define LV_LOG_LEVEL         LV_LOG_LEVEL_WARN
