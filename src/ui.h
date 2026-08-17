#pragma once

#include "lvgl.h"
#include <cstdint>
#include <cstdio>

class App;   /* forward declaration; UI callbacks dispatch into App */

/*
 * Builds and owns every on-screen widget: the play/pause button, the playlist
 * (file-browser) button, the now-playing title chip, the seek bar, the time
 * label, and the volume slider. All LVGL event callbacks are static trampolines
 * that recover the App* from the widget's user data and forward to App methods.
 */
class Ui {
public:
    Ui(App & app) : app_(app) {}

    void build(lv_obj_t * screen);

    /* Refresh the progress bar + time text (called from the periodic timer). */
    void setProgress(int32_t cur_ms, int32_t total_ms);
    /* While the user is dragging, only update the time text (not the bar). */
    void previewSeek(int32_t ms);

    void setPlaying(bool playing);
    void setNowPlaying(const char * path);
    void setVolumeLabel(int v);

    /* Map a raw slider value (0..1000) to milliseconds, using the live total. */
    int32_t sliderToMs(int32_t v) const;

private:
    int32_t msToSlider(int32_t ms) const;
    void    setTimeText(int32_t cur_ms, int32_t total_ms);

    static void playBtnCb(lv_event_t * e);
    static void volSliderCb(lv_event_t * e);
    static void seekSliderCb(lv_event_t * e);
    static void openBtnCb(lv_event_t * e);
    static void shotBtnCb(lv_event_t * e);

    App &     app_;
    lv_obj_t * screen_      = nullptr;
    lv_obj_t * playBtn_     = nullptr;
    lv_obj_t * playBtnLabel_= nullptr;
    lv_obj_t * shotBtn_     = nullptr;
    lv_obj_t * seekSlider_  = nullptr;
    lv_obj_t * timeLabel_   = nullptr;
    lv_obj_t * titleLabel_  = nullptr;
    lv_obj_t * volLabel_    = nullptr;
    int32_t    totalMs_     = 0;
};

static inline void format_time(char * buf, size_t n, int32_t ms)
{
    int s  = ms / 1000;
    int m  = s / 60;
    s %= 60;
    snprintf(buf, n, "%d:%02d", m, s);
}
