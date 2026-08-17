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
/* Enlarge the fbdev draw buffer to full screen height. LVGL's default is 60
 * scanlines, which forces an 800x450 video to composite in ~8 partial bands
 * (~20 ms of redundant re-renders/frame). 1280 = full height -> 1 band. */
#define LV_LINUX_FBDEV_BUFFER_SIZE   1280

/* ===== desktop simulator backend (SDL) =====
 * OFF by default: the device build has no SDL2 headers. The CMake SDL build
 * enables it with -DLV_USE_SDL=1 (see ENABLE_SDL in CMakeLists.txt); the
 * application then selects the backend via PLAYER_USE_SDL. The default
 * simulator window is 1280x800. */
#ifndef LV_USE_SDL
    #define LV_USE_SDL          0
#endif

/* ===== video playback: LVGL official ffmpeg soft decode ===== */
#define LV_USE_FFMPEG       1
#define LV_FFMPEG_DUMP_FORMAT       0
#define LV_FFMPEG_PLAYER_USE_LV_FS  0

/* ===== system monitors: FPS/CPU + memory ===== */
#define LV_USE_OBSERVER     1          /* sysmon depends on observer */
#define LV_USE_SYSMON       1
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* ===== input: touchscreen via evdev ===== */
#define LV_USE_EVDEV        1

/* ===== screen capture: utils/lv_snapshot (PNG export) =====
 * Needed by lv_snapshot_take() so we can grab the current screen/object
 * rendering and write it to a PNG file (--shot / --shot-dir options). */
#define LV_USE_SNAPSHOT     1

/* ===== fonts =====
 * Default to the bundled Source Han Sans SC CJK font so Chinese UI text
 * (file browser ".. (上级目录)", titles, ...) renders real glyphs instead
 * of tofu boxes - the stock default (Montserrat 14) has no CJK glyphs.
 * The font also embeds the FontAwesome symbols used by the UI
 * (LV_SYMBOL_LEFT/PLAY/CLOSE/VIDEO/DIRECTORY etc.), so icons keep working. */
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK  1
#define LV_FONT_DEFAULT  &lv_font_source_han_sans_sc_16_cjk

/* ===== memory / refresh =====
 * LV_MEM_SIZE MUST be large enough for the full-frame display buffer
 * (w*h*bpp = 800*1280*4 ~= 4 MB) plus LVGL objects, or lv_display_set_buffers
 * will spin inside the allocator. 32 MB is comfortable for this panel. */
#define LV_MEM_SIZE         (32 * 1024 * 1024)
#define LV_MEM_ADR          0
#define LV_DEF_REFR_PERIOD   16

#endif /* LV_CONF_H */
