# lv_ffmpeg patch (v3)

LVGL's bundled `lv_ffmpeg` player (in `lvgl/src/libs/ffmpeg/lv_ffmpeg.c`) is a
"fire and forget" soft-decoder: it exposes `create / set_src / set_cmd(START)`
but **no pause, no seek, and no duration/time query**. This project needs all
three, so we patch the file to add a wall-clock-driven player API on top of the
existing decoder.

## What it adds

Applied to `lv_ffmpeg.c` (and forward-declared in `include/Player.h`):

| function                              | purpose                                              |
|---------------------------------------|------------------------------------------------------|
| `lv_ffmpeg_player_pause(obj)`         | freeze the video wall-clock                          |
| `lv_ffmpeg_player_resume(obj)`        | resume, subtracting the paused duration              |
| `lv_ffmpeg_player_seek(obj, ms)`      | re-anchor the video clock to `ms`                    |
| `lv_ffmpeg_player_get_time(obj)`      | current position, ms (wall-clock)                    |
| `lv_ffmpeg_player_get_duration(obj)`  | total duration, ms (falls back to longest stream)    |

The duration getter falls back to the **longest stream's duration** because
`fmt_ctx->duration` is often `AV_NOPTS_VALUE` under the custom IO the player
uses.

## How to apply

Run the patcher inside your LVGL source tree (it edits `lv_ffmpeg.c` in place
and prints `PATCH OK` when successful):

```sh
cd /path/to/lvgl
python3 /path/to/lvgl-video-player/patches/lv_ffmpeg/patch_lvffmpeg_v3.py
```

The CMake build applies this automatically at configure time when `lvgl` is
present as a submodule. Re-running is safe (it detects the already-patched
markers).
