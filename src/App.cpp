#include "App.h"
#include "config.h"
#include "screen.h"
#include "utils/lv_snapshot.h"
#include "debugging/sysmon/lv_sysmon.h"
#if PLAYER_USE_SDL
    #include "drivers/sdl/lv_sdl_window.h"
#else
    #include "drivers/display/fb/lv_linux_fbdev.h"
    #include "drivers/evdev/lv_evdev.h"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <algorithm>

App::App() : browser_(*this, ROOT_DIR), ui_(*this)
{
}

uint32_t App::nowMs()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool App::init()
{
    /* Enable the rk809 speaker output (runtime mixer; lost on reboot). */
    system("amixer -c 0 cset numid=12 SPK >/dev/null 2>&1");
    system("amixer -c 0 cset numid=15 252,252 >/dev/null 2>&1");

    lv_init();

    /* Backend: SDL window (desktop simulator, ENABLE_SDL=ON) or the Linux
     * framebuffer (/dev/fb0, device build). Selected at compile time via
     * PLAYER_USE_SDL (defined by CMake; 0 on the device Makefile build). */
#if PLAYER_USE_SDL
    lv_display_t * disp = lv_sdl_window_create(1280, 800);
#else
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");
#endif

    /* Screen geometry: physical framebuffer size + requested rotation.
     * g_screen.w/h become the logical resolution every UI layout uses. */
    g_screen.phys_w = lv_display_get_horizontal_resolution(disp);
    g_screen.phys_h = lv_display_get_vertical_resolution(disp);
    switch (g_screen.rotation) {
        case 90:  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);  break;
        case 180: lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180); break;
        case 270: lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270); break;
        default:  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);   break;
    }
    if (g_screen.rotation == 90 || g_screen.rotation == 270) {
        g_screen.w = g_screen.phys_h;
        g_screen.h = g_screen.phys_w;
    } else {
        g_screen.w = g_screen.phys_w;
        g_screen.h = g_screen.phys_h;
    }
    std::cerr << "[init] fb=" << g_screen.phys_w << "x" << g_screen.phys_h
              << " rotation=" << g_screen.rotation
              << " logical=" << g_screen.w << "x" << g_screen.h << "\n";

#if !PLAYER_USE_SDL
    lv_indev_t * touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, TOUCH_DEV);
    if (touch == nullptr)
        LV_LOG_WARN("evdev touch open failed: " TOUCH_DEV);
#endif
    /* SDL backend: lv_sdl_window_create() installs its own mouse pointer. */

    lv_ffmpeg_init();

    /* Allow overriding the default clip at runtime: PLAYER_VIDEO env var
     * (handy for the SDL simulator where the board path doesn't exist). */
    const char * video = getenv("PLAYER_VIDEO");
    if (!video || !*video) video = VIDEO_PATH;

    if (!player_.create(lv_screen_active(), g_screen.w, g_screen.h)) {
        LV_LOG_WARN("lv_ffmpeg_player_create failed");
    }

    fitToScreen(video);   /* probe native size, size widget to contain-fit */

    screen_ = lv_screen_active();

    lv_result_t res = lv_ffmpeg_player_set_src(player_.obj(), video);
    if (res != LV_RESULT_OK) {
        lv_obj_t * label = lv_label_create(screen_);
        std::string errMsg = std::string("video src fail:\n") + video;
        lv_label_set_text(label, errMsg.c_str());
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_center(label);
    } else {
        player_.start();
    }

    int32_t dur = player_.getDuration();
    std::cerr << "[init] duration_ms=" << dur << "\n";

    audio_.open(video);
    audio_.start();

#if LV_USE_PERF_MONITOR
    lv_sysmon_show_performance(disp);
#endif
#if LV_USE_MEM_MONITOR
    lv_sysmon_show_memory(disp);
#endif

    ui_.build(screen_);
    ui_.setNowPlaying(VIDEO_PATH);

    lv_timer_create(uiRefreshCb, 200, this);
    return true;
}

void App::run()
{
    /* Tight main loop — no syscalls between lv_timer_handler() calls.
     *
     * Why: on this RK3566 kernel every user->kernel transition (clock_gettime,
     * usleep, fprintf) in the hot path costs ~10ms of CFS scheduling penalty.
     * The old loop had 4+ syscalls/iteration, adding ~20ms of pure overhead and
     * capping playback at ~18fps. A pure tight loop eliminates that entirely.
     *
     * The player timer period is set by lv_ffmpeg.c to the file's real frame
     * so one decode per loop. LVGL's tick is driven by the real-time clock
     * callback installed by fbdev init (no lv_tick_inc needed). Measured frame
     * interval ≈ 28ms (~35fps) — smooth, no stutter. */
    while (1) {
        lv_timer_handler();
#if PLAYER_USE_SDL
        /* Desktop simulator: yield so the SDL window stays responsive and the
         * host CPU isn't pegged. The ~10ms/syscall penalty that forced the
         * tight loop on RK3566 does not apply to a desktop PC. */
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
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
        std::cerr << "[ui] paused\n";
    } else {
        player_.resume();
        audio_.setPaused(false);
        ui_.setPlaying(true);
        playing_ = true;
        std::cerr << "[ui] resumed\n";
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
        std::cerr << "[ui] seek -> " << ms << " ms\n";
    }
}

void App::playFile(const std::string & path)
{
    if (player_.obj()) {
        fitToScreen(path.c_str());   /* re-fit for the new clip's aspect ratio */
        player_.setSrc(path.c_str());
        player_.start();
        ui_.setNowPlaying(path.c_str());
        std::cerr << "[ui] play -> " << path << "\n";
    }
    audio_.reopen(path);
    browser_.close();
}

void App::fitVideo(int vw, int vh, int * rw, int * rh)
{
    const int DW = g_screen.w, DH = g_screen.h;   /* logical screen size */
    if (vw <= 0 || vh <= 0) { *rw = DW; *rh = DH; return; }
    /* contain: scale so the whole video fits inside the screen, preserving
     * aspect ratio (letterbox bars show the black background, never cropped). */
    double scale = std::min((double)DW / vw, (double)DH / vh);
    int w = (int)(vw * scale);
    int h = (int)(vh * scale);
    if (w > DW) w = DW;
    if (h > DH) h = DH;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    *rw = w; *rh = h;
}

void App::fitToScreen(const char * path)
{
    int vw = 0, vh = 0;
    if (player_.probe(path, &vw, &vh)) {
        int rw = 0, rh = 0;
        fitVideo(vw, vh, &rw, &rh);
        player_.resize(rw, rh);
        std::cerr << "[fit] " << path << " " << vw << "x" << vh
                  << " -> render " << rw << "x" << rh << "\n";
    } else {
        player_.resize(g_screen.w, g_screen.h);   /* unknown size: fill the screen */
        std::cerr << "[fit] " << path << " probe failed -> full screen\n";
    }
}

void App::toggleBrowser()
{
    browser_.toggle();
}

void App::startSnapshot(const std::string & dir, int periodSec)
{
    shotDir_ = dir;
    int period = periodSec > 0 ? periodSec : 5;
    lv_timer_t * t = lv_timer_create(snapshotTimerCb, (uint32_t)period * 1000, this);
    lv_timer_set_repeat_count(t, -1);   /* fire forever */
    std::cerr << "[snapshot] periodic: " << dir << " every " << period << "s\n";
}

void App::takeSnapshotOnce(const std::string & path)
{
    shotOncePath_ = path;
    lv_timer_t * t = lv_timer_create(snapshotTimerCb, 2000, this);   /* let UI settle */
    lv_timer_set_repeat_count(t, 1);
    std::cerr << "[snapshot] one-shot: " << path << " (after 2s)\n";
}

void App::snapshotTimerCb(lv_timer_t * t)
{
    App * a = (App *)lv_timer_get_user_data(t);
    if (a) a->snapshotTick();
}

void App::snapshotTick()
{
    lv_obj_t * target = screen_ ? screen_ : lv_screen_active();
    if (!shotDir_.empty()) {
        std::ostringstream oss;
        oss << shotDir_ << "/shot_" << std::setw(3) << std::setfill('0')
            << shotSeq_++ << ".png";
        lv_snapshot_save_png(target, oss.str().c_str());
    }
    if (!shotOncePath_.empty()) {
        lv_snapshot_save_png(target, shotOncePath_.c_str());
        shotOncePath_.clear();
    }
}

void App::onSnapshot()
{
    /* Filename scheme: shot_YYYYMMDD_HHMMSS_mmm_WxH.png
     *   - YYYYMMDD_HHMMSS = local-time wall clock (so files sort by time)
     *   - mmm             = milliseconds, keeps rapid bursts unique
     *   - WxH            = current logical (post-rotation) screen size, so
     *                       the file tells you its orientation at a glance */
    namespace ch = std::chrono;
    const auto now   = ch::system_clock::now();
    const auto tt    = ch::system_clock::to_time_t(now);
    const auto epoch = now.time_since_epoch();
    const int  ms    = (int)(ch::duration_cast<ch::milliseconds>(epoch).count() % 1000);

    std::tm tm = *std::localtime(&tt);   /* C standard <ctime>; safe at our call rate */

    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm);

    char path[96];
    std::snprintf(path, sizeof(path), "/tmp/shot_%s_%03d_%dx%d.png",
                  ts, ms, g_screen.w, g_screen.h);

    lv_obj_t * target = screen_ ? screen_ : lv_screen_active();
    lv_snapshot_save_png(target, path);
    std::cerr << "[snapshot] button -> " << path << "\n";
}
