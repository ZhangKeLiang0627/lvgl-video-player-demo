#pragma once

#include "lvgl.h"
#include <cstdint>

/*
 * Thin C++ wrapper around the (patched) lv_ffmpeg player object.
 *
 * The patched helpers pause/resume/seek/get_time/get_duration are NOT part of
 * upstream lv_ffmpeg; they are added by patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch.
 * We forward-declare them here (as extern "C") so the rest of the project never
 * has to touch lv_ffmpeg.h or the patch internals.
 */
extern "C" {
    lv_result_t lv_ffmpeg_player_pause(lv_obj_t * player);
    lv_result_t lv_ffmpeg_player_resume(lv_obj_t * player);
    lv_result_t lv_ffmpeg_player_seek(lv_obj_t * player, int32_t ms);
    int32_t     lv_ffmpeg_player_get_time(lv_obj_t * player);
    int32_t     lv_ffmpeg_player_get_duration(lv_obj_t * player);
}

class Player {
public:
    Player() = default;

    /* Create the player widget, fill it to w x h and center it on the screen. */
    bool create(lv_obj_t * parent, int w, int h);

    /* Load a video file. Returns true on success. */
    bool setSrc(const char * path);

    /* Start (or restart) playback. Also enables auto-restart on EOF. */
    void start();

    void pause();
    void resume();
    void seek(int32_t ms);

    int32_t getTime() const;       /* current position, ms (wall-clock driven) */
    int32_t getDuration() const;   /* total duration, ms (0 if unknown) */

    lv_obj_t * obj() const { return obj_; }

private:
    lv_obj_t * obj_ = nullptr;
};
