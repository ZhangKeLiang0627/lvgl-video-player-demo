#pragma once

#include "lvgl.h"
#include <string>
#include <cstdint>

#include "player.h"
#include "audio_engine.h"
#include "file_browser.h"
#include "ui.h"

/*
 * Top-level application orchestrator.
 *
 * Owns the video Player, the audio AudioEngine (a slave clock to the video),
 * the FileBrowser playlist, and the Ui. Wires the periodic refresh timer that
 * publishes the video position to the audio engine and drives the progress bar.
 */
class App {
public:
    App();
    bool init();
    void run();

    /* Called from UI callbacks. */
    void onTogglePlay();
    void onVolume(int v);
    void onSeekPress(int32_t ms);
    void onSeekRelease(int32_t ms);
    void playFile(const std::string & path);
    void toggleBrowser();

    /* Save a PNG snapshot of the current screen under a timestamped name
     * (shot_YYYYMMDD_HHMMSS_mmm_WxH.png). Bound to the "Shot" button in the
     * top bar; the same path is used internally by --shot / --shot-dir. */
    void onSnapshot();

    /* Screen capture (utils/lv_snapshot):
     *   startSnapshot(dir, sec)  - save <dir>/shot_000.png every `sec` seconds
     *   takeSnapshotOnce(path)   - save one shot after the UI settles (~2 s) */
    void startSnapshot(const std::string & dir, int periodSec);
    void takeSnapshotOnce(const std::string & path);

    Player      & player()  { return player_;  }
    AudioEngine & audio()   { return audio_;   }
    Ui          & ui()      { return ui_;      }
    FileBrowser & browser() { return browser_; }

private:
    static void uiRefreshCb(lv_timer_t * t);
    void        uiRefresh();
    static uint32_t nowMs();

    static void snapshotTimerCb(lv_timer_t * t);
    void        snapshotTick();

    /* Compute a contain-fit rectangle of (vw x vh) inside the logical screen
     * (g_screen.w x g_screen.h, rotation-aware), preserving aspect ratio. */
    void fitVideo(int vw, int vh, int * rw, int * rh);

    /* Probe `path` for native size and resize the player widget to its
     * contain-fit; fall back to full screen if probing fails. */
    void fitToScreen(const char * path);

    Player      player_;
    AudioEngine audio_;
    FileBrowser browser_;
    Ui          ui_;

    lv_obj_t * screen_ = nullptr;
    int       userSeeking_     = 0;
    int32_t    seekSettleUntil_ = 0;   /* ignore bar override until this ms */
    bool      playing_          = true;

    /* Screen-capture state. */
    std::string shotDir_;
    std::string shotOncePath_;
    int         shotSeq_ = 0;
};
