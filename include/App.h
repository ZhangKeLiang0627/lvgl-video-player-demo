#pragma once

#include "lvgl.h"
#include <string>
#include <cstdint>

#include "Player.h"
#include "AudioEngine.h"
#include "FileBrowser.h"
#include "Ui.h"

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

    Player      & player()  { return player_;  }
    AudioEngine & audio()   { return audio_;   }
    Ui          & ui()      { return ui_;      }
    FileBrowser & browser() { return browser_; }

private:
    static void uiRefreshCb(lv_timer_t * t);
    void        uiRefresh();
    static uint32_t nowMs();

    Player      player_;
    AudioEngine audio_;
    FileBrowser browser_;
    Ui          ui_;

    lv_obj_t * screen_ = nullptr;
    int       userSeeking_     = 0;
    int32_t    seekSettleUntil_ = 0;   /* ignore bar override until this ms */
    bool      playing_          = true;
};
