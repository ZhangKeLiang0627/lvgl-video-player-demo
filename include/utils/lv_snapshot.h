#pragma once

/*
 * lv_snapshot — runtime screen capture for LVGL apps.
 *
 * Grabs the current rendering of any LVGL object (pass lv_screen_active()
 * for a full-screen shot), encodes it as a PNG file via a minimal
 * zlib-based encoder (no libpng dependency), and writes it to disk.
 *
 * Requirements:
 *   - LV_USE_SNAPSHOT = 1 in lv_conf.h
 *   - zlib (-lz)
 *
 * Typical use:
 *   ./demo --shot-dir /tmp/shots --shot-period 5    # periodic captures
 *   ./demo --shot /tmp/ui.png                       # one-shot after 2s
 *   lv_snapshot_save_png(lv_screen_active(), "/tmp/ui.png");  // in code
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * Capture `obj` (with all its children) and write it as a PNG to `path`.
 *
 * @param obj   target object; use lv_screen_active() for the full screen.
 * @param path  output file path; the directory must already exist.
 * @return      0 on success, -1 on failure (logs a reason to stderr).
 */
int lv_snapshot_save_png(lv_obj_t * obj, const char * path);

#ifdef __cplusplus
}
#endif
