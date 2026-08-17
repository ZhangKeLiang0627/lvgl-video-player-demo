#include "Ui.h"
#include "App.h"
#include "config.h"
#include "screen.h"

#include <cstdio>
#include <cstring>

void Ui::build(lv_obj_t * screen)
{
    screen_ = screen;

    /* All sizes / offsets are percentages of the logical screen size
     * (g_screen.w x g_screen.h, rotation-aware), so the layout stays
     * proportional on any panel resolution / orientation. */
    const int W = g_screen.w;
    const int H = g_screen.h;

    /* top-left: play / pause toggle */
    playBtn_ = lv_button_create(screen);
    lv_obj_set_size(playBtn_, spct(W, 14), spct(H, 5));
    lv_obj_align(playBtn_, LV_ALIGN_TOP_LEFT, spct(W, 3), spct(H, 2));
    lv_obj_set_style_bg_color(playBtn_, lv_color_make(0x37, 0x6E, 0x37), 0);
    lv_obj_set_style_radius(playBtn_, 12, 0);
    playBtnLabel_ = lv_label_create(playBtn_);
    lv_label_set_text(playBtnLabel_, LV_SYMBOL_PAUSE " Pause");
    lv_obj_center(playBtnLabel_);
    lv_obj_add_event_cb(playBtn_, playBtnCb, LV_EVENT_CLICKED, &app_);

    /* top-right: open playlist / file browser */
    lv_obj_t * pl_btn = lv_button_create(screen);
    lv_obj_set_size(pl_btn, spct(W, 9), spct(H, 5));
    lv_obj_align(pl_btn, LV_ALIGN_TOP_RIGHT, -spct(W, 3), spct(H, 2));
    lv_obj_set_style_bg_color(pl_btn, lv_color_make(0x15, 0x65, 0xC0), 0);
    lv_obj_set_style_radius(pl_btn, 12, 0);
    lv_obj_t * pl_lbl = lv_label_create(pl_btn);
    lv_label_set_text(pl_lbl, LV_SYMBOL_DIRECTORY);
    lv_obj_center(pl_lbl);
    lv_obj_add_event_cb(pl_btn, openBtnCb, LV_EVENT_CLICKED, &app_);

    /* top-center: now-playing chip (filename + full path) */
    titleLabel_ = lv_label_create(screen);
    lv_label_set_text(titleLabel_, "-\n-");
    lv_obj_set_style_text_color(titleLabel_, lv_color_white(), 0);
    lv_obj_set_style_text_align(titleLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(titleLabel_, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(titleLabel_, 140, 0);
    lv_obj_set_style_pad_hor(titleLabel_, 14, 0);
    lv_obj_set_style_pad_ver(titleLabel_, 6, 0);
    lv_obj_set_style_radius(titleLabel_, 10, 0);
    lv_obj_align(titleLabel_, LV_ALIGN_TOP_MID, 0, spct(H, 2));
    lv_obj_move_foreground(titleLabel_);

    /* bottom: progress bar, normalized 0..1000 range (avoids int32 overflow) */
    seekSlider_ = lv_slider_create(screen);
    lv_obj_set_size(seekSlider_, spct(W, 93), spct(H, 1));
    lv_obj_align(seekSlider_, LV_ALIGN_BOTTOM_MID, 0, -spct(H, 9));
    lv_slider_set_range(seekSlider_, 0, 1000);
    lv_slider_set_value(seekSlider_, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(seekSlider_, lv_color_make(0x55, 0x55, 0x55), LV_PART_MAIN);
    lv_obj_set_style_bg_color(seekSlider_, lv_color_make(0x42, 0xA5, 0xF5), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(seekSlider_, lv_color_make(0x90, 0xCA, 0xF9), LV_PART_KNOB);
    lv_obj_add_event_cb(seekSlider_, seekSliderCb,
                        (lv_event_code_t)(LV_EVENT_PRESSING | LV_EVENT_RELEASED), &app_);

    /* time label: directly above the progress bar, with a dark backing */
    timeLabel_ = lv_label_create(screen);
    lv_label_set_text(timeLabel_, "0:00 / 0:00");
    lv_obj_set_style_text_color(timeLabel_, lv_color_white(), 0);
    lv_obj_set_style_text_font(timeLabel_, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_bg_color(timeLabel_, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(timeLabel_, 140, 0);
    lv_obj_set_style_pad_hor(timeLabel_, 10, 0);
    lv_obj_set_style_pad_ver(timeLabel_, 4, 0);
    lv_obj_set_style_radius(timeLabel_, 8, 0);
    lv_obj_align(timeLabel_, LV_ALIGN_BOTTOM_MID, 0, -spct(H, 11));
    lv_obj_move_foreground(timeLabel_);

    /* bottom-center: volume slider (0..100 -> software gain 0..VOL_MAX_GAIN) */
    lv_obj_t * vol_slider = lv_slider_create(screen);
    lv_obj_set_size(vol_slider, spct(W, 45), spct(H, 1));
    lv_obj_align(vol_slider, LV_ALIGN_BOTTOM_MID, 0, -spct(H, 4));
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, VOL_DEFAULT, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(vol_slider, lv_color_make(0x15, 0x65, 0xC0), LV_PART_MAIN);
    lv_obj_set_style_bg_color(vol_slider, lv_color_make(0x42, 0xA5, 0xF5), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vol_slider, lv_color_make(0x42, 0xA5, 0xF5), LV_PART_KNOB);
    volLabel_ = lv_label_create(screen);
    lv_label_set_text_fmt(volLabel_, "Vol %d", VOL_DEFAULT);
    lv_obj_align_to(volLabel_, vol_slider, LV_ALIGN_OUT_TOP_MID, 0, -spct(H, 1));
    lv_obj_add_event_cb(vol_slider, volSliderCb, LV_EVENT_VALUE_CHANGED, &app_);
}

/* ---------------- progress / time helpers ---------------- */

int32_t Ui::sliderToMs(int32_t v) const
{
    if (totalMs_ <= 0) return 0;
    return (int32_t)((int64_t)v * totalMs_ / 1000);
}

int32_t Ui::msToSlider(int32_t ms) const
{
    if (totalMs_ <= 0) return 0;
    int32_t v = (int32_t)((int64_t)ms * 1000 / totalMs_);
    if (v < 0) v = 0;
    if (v > 1000) v = 1000;
    return v;
}

void Ui::setTimeText(int32_t cur_ms, int32_t total_ms)
{
    char cb[16], tb[16];
    format_time(cb, sizeof(cb), cur_ms);
    format_time(tb, sizeof(tb), total_ms > 0 ? total_ms : totalMs_);
    lv_label_set_text_fmt(timeLabel_, "%s / %s", cb, tb);
}

void Ui::setProgress(int32_t cur_ms, int32_t total_ms)
{
    if (total_ms > 0) totalMs_ = total_ms;
    if (totalMs_ > 0)
        lv_slider_set_value(seekSlider_, msToSlider(cur_ms), LV_ANIM_OFF);
    setTimeText(cur_ms, total_ms);
}

void Ui::previewSeek(int32_t ms)
{
    setTimeText(ms, totalMs_);
}

void Ui::setPlaying(bool playing)
{
    if (playBtnLabel_)
        lv_label_set_text(playBtnLabel_,
                          playing ? LV_SYMBOL_PAUSE " Pause"
                                  : LV_SYMBOL_PLAY   " Play");
}

void Ui::setNowPlaying(const char * path)
{
    if (!titleLabel_ || !path) return;
    const char * bn = strrchr(path, '/');
    bn = bn ? bn + 1 : path;
    lv_label_set_text_fmt(titleLabel_, "%s\n%s", bn, path);
}

void Ui::setVolumeLabel(int v)
{
    if (volLabel_) lv_label_set_text_fmt(volLabel_, "Vol %d", v);
}

/* ---------------- event trampolines ---------------- */

void Ui::playBtnCb(lv_event_t * e)
{
    App * a = (App *)lv_event_get_user_data(e);
    if (a) a->onTogglePlay();
}

void Ui::openBtnCb(lv_event_t * e)
{
    App * a = (App *)lv_event_get_user_data(e);
    if (a) a->toggleBrowser();
}

void Ui::volSliderCb(lv_event_t * e)
{
    App * a = (App *)lv_event_get_user_data(e);
    if (!a) return;
    int v = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    a->onVolume(v);
}

void Ui::seekSliderCb(lv_event_t * e)
{
    App * a = (App *)lv_event_get_user_data(e);
    if (!a) return;
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t ms = a->ui().sliderToMs(lv_slider_get_value(slider));
    if (code == LV_EVENT_PRESSING)
        a->onSeekPress(ms);
    else if (code == LV_EVENT_RELEASED)
        a->onSeekRelease(ms);
}
