# lv_ffmpeg patch for LVGL v9.5.0

LVGL 自带的 `lv_ffmpeg` 播放器（`src/libs/ffmpeg/lv_ffmpeg.c`）只提供了
`create` / `set_src` / `start`，**缺少播放控制与进度查询接口**。本项目需要
暂停 / 继续 / 拖拽 seek / 取当前时间 / 取总时长，因此这里维护了一组针对
**LVGL v9.5.0**（commit `85aa60d`）的补丁。

## 补丁做了什么

`lv_ffmpeg_v9.5.0.patch` 是一个相对 `lvgl/` 仓库根的统一 diff，包含 6 个 hunk：

1. `#include <time.h>`（用于 wall-clock 帧计时）。
2. 在私有结构体 `lv_ffmpeg_player_t` 中新增字段：`paused`、
   `play_start_us`、`tb`、`video_stream` 等。
3. 在解码/渲染主循环里引入基于 `clock()` 的 wall-clock 帧计时逻辑。
4. 新增 5 个控制函数（函数声明见 `include/Player.h` 的 `extern "C"` 段）：
   - `lv_ffmpeg_player_pause(lv_obj_t*)`
   - `lv_ffmpeg_player_resume(lv_obj_t*)`
   - `lv_ffmpeg_player_seek(lv_obj_t*, int32_t ms)`
   - `lv_ffmpeg_player_get_time(lv_obj_t*) -> int32_t`（毫秒）
   - `lv_ffmpeg_player_get_duration(lv_obj_t*) -> int32_t`（毫秒，
     取最长流的时长作为兜底）

5. **渲染缓冲跟随播放器对象尺寸**（`render_w` / `render_h`）：`lv_ffmpeg` 原本把
   LVGL 图像缓冲和 YUV→RGB 转换都按**视频原生分辨率**做，导致大分辨率视频既被裁切
   又因每帧做数百万像素的 YUV→RGB 而只有 1–2 帧。补丁改为在每帧把解码帧用
   `sws_scale` 缩放到**播放器对象的实际尺寸**（由上层按源宽高比 contain 进屏幕后设置），
   从而在不浪费算力的情况下实现「适配屏幕」并让高分辨率视频流畅播放。对象尺寸变化
   时会在定时器回调里自动重分配缓冲（见 `lv_ffmpeg_player_frame_update_cb`）。

## 自动应用（推荐）

`CMakeLists.txt` 在 `add_subdirectory(lvgl)` 之前会执行一次幂等的
`git apply --check` + `git apply`：

```cmake
execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --check ${PATCH_FILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
    RESULT_VARIABLE _rc)
if(_rc EQUAL 0)
    execute_process(COMMAND ${GIT_EXECUTABLE} apply ${PATCH_FILE}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lvgl)
endif()
```

若补丁已应用过，`--check` 会失败，CMake 自动跳过——不会把构建搞挂。

## 手动应用

```sh
cd lvgl                      # 你的 LVGL v9.5.0 源码树
git apply ../lvgl-video-player/patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch
# 或者在项目根目录：
# git apply -p1 patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch
```

## 版本约束

- 该补丁**仅验证于 LVGL v9.5.0**（commit `85aa60d`）。其它版本 `lv_ffmpeg.c`
  行号/结构体不同，`git apply` 会失败；届时请基于对应版本的 `lv_ffmpeg.c`
  重新生成 diff（参考 `docs/architecture.md` 的「A/V 同步」一节）。
- 仓库通过 git submodule 把 LVGL 固定在 v9.5.0，克隆者执行
  `git submodule update --init --recursive` 即可拿到正确版本。
