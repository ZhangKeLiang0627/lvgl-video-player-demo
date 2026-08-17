#pragma once

#include <cstdint>

/*
 * Runtime screen geometry.
 *
 * `w` / `h` are the LOGICAL resolution after rotation is applied: on a
 * 800x1280 portrait panel rotated 90 degrees they become 1280x800, and on
 * a different panel (e.g. 320x240) they simply track the framebuffer. All UI
 * layout must use these values (as percentages) instead of hardcoded pixels,
 * so the same build renders correctly on any resolution / orientation.
 *
 * `phys_w` / `phys_h` are the raw framebuffer resolution (rotation 0).
 * `rotation` is the requested rotation angle: 0 / 90 / 180 / 270.
 */
struct ScreenInfo {
    int w = 800;       /* logical width after rotation  */
    int h = 1280;      /* logical height after rotation */
    int phys_w = 800;  /* physical framebuffer width    */
    int phys_h = 1280; /* physical framebuffer height   */
    int rotation = 0;  /* 0 / 90 / 180 / 270            */
};

extern ScreenInfo g_screen;

/* Percent-of-screen helper: returns (value * pct / 100). */
static inline int spct(int value, int pct)
{
    return (int)(((int64_t)value * pct) / 100);
}
