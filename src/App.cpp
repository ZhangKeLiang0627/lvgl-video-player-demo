#include "App.h"
#include "config.h"
#include "debugging/sysmon/lv_sysmon.h"

#include <cstdio>
#include <ctime>
#include <unistd.h>

App::App() : browser_(*this, ROOT_DIR), ui_(*this)
{
}

uint32_t App::nowMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

bool App::init()
{
    /* Enable the rk809 speaker output (runtime mixer; lost on reboot). */
    system("amixer -c 0 cset numid=12 SPK >/dev/null 2>&1");
    system("amixer -c 0 cset numid=15 252,252 >/dev/null 2>&1");

    lv_init();

    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    lv_indev_t * touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, TOUCH_DEV);
    if (touch == nullptr)
        LV_LOG_WARN("evdev touch open failed: " TOUCH_DEV);

    lv_ffmpeg_init();

    if (!player_.create(lv_screen_active(), 800, 1280)) {
        LV_LOG_WARN("lv_ffmpeg_player_create failed");
    }

    screen_ = lv_screen_active();

    lv_result_t res = lv_ffmpeg_player_set_src(player_.obj(), VIDEO_PATH);
    if (res != LV_RESULT_OK) {
        lv_obj_t * label = lv_label_create(screen_);
        lv_label_set_text(label, "video src fail:\n" VIDEO_PATH);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_center(label);
    } else {
        player_.start();
    }

    int32_t dur = player_.getDuration();
    fprintf(stderr, "[init] duration_ms=%" LV_PRId32 "\n", dur);

    audio_.open(VIDEO_PATH);
    audio_.start();

    lv_sysmon_show_performance(disp);
    lv_sysmon_show_memory(disp);

    ui_.build(screen_);
    ui_.setNowPlaying(VIDEO_PATH);

    lv_timer_create(uiRefreshCb, 200, this);
    return true;
}

void App::run()
{
    uint32_t last = nowMs();
    while (1) {
        uint32_t now = nowMs();
        uint32_t dt = now - last;
        if (dt == 0) dt = 1;
        lv_tick_inc(dt);
        last = now;
        lv_timer_handler();
        usleep(2000);
    }
}

void App::uiRefresh()
{
    if (player_.obj() == nullptr) return;
    int32_t cur   = player_.getTime();
    int32_t total = player_.getDuration();

    audio_.publishVideoMs(cur);          /* video is the master clock */

    uint32_t now = nowMs();
    if (!userSeeking_ && now >= (uint32_t)seekSettleUntil_)
        ui_.setProgress(cur, total);
}

void App::uiRefreshCb(lv_timer_t * t)
{
    App * a = (App *)lv_timer_get_user_data(t);
    if (a) a->uiRefresh();
}

void App::onTogglePlay()
{
    if (player_.obj() == nullptr) return;
    if (playing_) {
        player_.pause();
        audio_.setPaused(true);
        ui_.setPlaying(false);
        playing_ = false;
        fprintf(stderr, "[ui] paused\n");
    } else {
        player_.resume();
        audio_.setPaused(false);
        ui_.setPlaying(true);
        playing_ = true;
        fprintf(stderr, "[ui] resumed\n");
    }
}

void App::onVolume(int v)
{
    audio_.setVolume((float)v / 100.0f * VOL_MAX_GAIN);
    ui_.setVolumeLabel(v);
}

void App::onSeekPress(int32_t ms)
{
    LV_UNUSED(ms);
    userSeeking_ = 1;
    ui_.previewSeek(ms);
}

void App::onSeekRelease(int32_t ms)
{
    userSeeking_ = 0;
    if (player_.obj()) {
        player_.seek(ms);                 /* re-anchor video wall-clock */
        audio_.requestSeek(ms);           /* signal audio thread to follow */
        seekSettleUntil_ = (int32_t)nowMs() + 300;
        fprintf(stderr, "[ui] seek -> %" LV_PRId32 " ms\n", ms);
    }
}

void App::playFile(const std::string & path)
{
    if (player_.obj()) {
        player_.setSrc(path.c_str());
        player_.start();
        ui_.setNowPlaying(path.c_str());
        fprintf(stderr, "[ui] play -> %s\n", path.c_str());
    }
    audio_.reopen(path);
    browser_.close();
}

void App::toggleBrowser()
{
    browser_.toggle();
}
