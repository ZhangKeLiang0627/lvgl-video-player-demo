#include "Player.h"

bool Player::create(lv_obj_t * parent, int w, int h)
{
    obj_ = lv_ffmpeg_player_create(parent);
    if (obj_ == nullptr) return false;
    lv_obj_set_size(obj_, w, h);
    lv_obj_center(obj_);
    return true;
}

bool Player::setSrc(const char * path)
{
    if (obj_ == nullptr) return false;
    lv_result_t res = lv_ffmpeg_player_set_src(obj_, path);
    return res == LV_RESULT_OK;
}

void Player::start()
{
    if (obj_ == nullptr) return;
    lv_ffmpeg_player_set_auto_restart(obj_, true);
    lv_ffmpeg_player_set_cmd(obj_, LV_FFMPEG_PLAYER_CMD_START);
}

void Player::pause()
{
    if (obj_) lv_ffmpeg_player_pause(obj_);
}

void Player::resume()
{
    if (obj_) lv_ffmpeg_player_resume(obj_);
}

void Player::seek(int32_t ms)
{
    if (obj_) lv_ffmpeg_player_seek(obj_, ms);
}

int32_t Player::getTime() const
{
    return (obj_ == nullptr) ? 0 : lv_ffmpeg_player_get_time(obj_);
}

int32_t Player::getDuration() const
{
    return (obj_ == nullptr) ? 0 : lv_ffmpeg_player_get_duration(obj_);
}
