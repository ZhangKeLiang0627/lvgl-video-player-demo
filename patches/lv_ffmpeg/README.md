# lv_ffmpeg patch for LVGL v9.5.0

LVGL 自带的 `lv_ffmpeg` 播放器（`src/libs/ffmpeg/lv_ffmpeg.c`）只提供了
`create` / `set_src` / `start`，**缺少播放控制与进度查询接口**，且解码即上屏、
按视频原生分辨率渲染，在 RK3566 上既卡又慢。本项目维护了一组针对
**LVGL v9.5.0**（commit `85aa60d`）的补丁，解决 RK3566 播放性能、播放节奏与
播放控制三大问题。

## 补丁做了什么

`lv_ffmpeg_v9.5.0.patch` 是相对 `lvgl/` 仓库根的统一 diff（`stock v9.5.0 → 当前
工作树`，2571 行）。主要包括：

### 1. 播放控制 API（新增 5 个函数）
- `lv_ffmpeg_player_pause(lv_obj_t*)`
- `lv_ffmpeg_player_resume(lv_obj_t*)`
- `lv_ffmpeg_player_seek(lv_obj_t*, int32_t ms)`
- `lv_ffmpeg_player_get_time(lv_obj_t*) -> int32_t`（毫秒，播放墙钟）
- `lv_ffmpeg_player_get_duration(lv_obj_t*) -> int32_t`（毫秒，取最长流时长兜底）

对应 `struct ffmpeg_context_s` 新增播放时钟字段：`play_start_us`、`paused_accum_us`、
`pause_start_us`、`paused`、`tb` 等。

### 2. 渲染缓冲跟随播放器对象尺寸
`lv_ffmpeg` 原本把 LVGL 图像缓冲和 YUV→RGB 转换都按**视频原生分辨率**做，大分辨率
视频既被裁切又因每帧数百万像素转换而只有 1–2fps。补丁改为每帧用 `sws_scale` 把
解码帧缩放到**播放器对象的实际尺寸**（上层按源宽高比 contain 进屏幕后设置），
对象尺寸变化时在定时器回调里自动重分配缓冲（`render_w/render_h`）。

### 3. RGA 2D 加速（Rockchip）
`LV_FFMPEG_USE_RGA=1`（默认）时，YUV420P→NV12 打包与 NV12→RGB 缩放走 RGA 2D
硬件（dma_buf 导入，~2ms/帧），替代 CPU `sws_scale`（1080p ~47ms/帧）。非
Rockchip 构建设 `-DLV_FFMPEG_USE_RGA=0` 自动回退软解路径。

### 4. 硬件解码优先（RKMPP）
优先 `h264_rkmpp`/`hevc_rkmpp` 硬解，找不到自动回退软解。**刻意不尝试
`v4l2m2m`**：桌面发行版带这些 wrapper 但无 `/dev/video` 节点，open 失败会污染
解码器上下文，之后 `avcodec_send_packet` 一直返回 `AVERROR_INVALIDDATA`，播放
进入无限 auto-restart 循环（表现为换片源/seek 后"卡死"，SDL 仿真实测）。

### 5. 帧时钟锁帧 pace（播放节奏）
播放器定时器以细粒度（2ms）采样，每帧**解码超前**、仅当播放时钟跨过帧边界才上屏：
`present_due_us += frame_period_us`（`frame_period_us` 由片源 `avg_frame_rate` 全精度
微秒计算，如 23.976fps → 41708µs）。上屏节奏与解码耗时解耦，23.976/24/25/30fps
不再因解码抖动而一顿一顿，且对无 PTS 片源鲁棒（不依赖 PTS 比较）。

### 6. 帧周期定时器（快进修复）
原实现把播放器定时器硬编码为 8ms（tight-loop 时代遗留），LVGL 定时器按墙钟触发，
8ms 周期 = 解码多快放多快（30fps 片源约 2× 快进）。补丁改为 `FRAME_PACE_MS=2`
细采样 + 上面第 5 条的 pacer 锁帧。

### 7. seek 强化
- 目标时间 clamp 到容器时长，越界 seek 不再破坏 demuxer；
- `av_seek_frame` 后 `avformat_flush` + `avcodec_flush_buffers`，清掉 seek 残留的
  陈旧包/解码帧；
- 同时重锚 pacer 的 `present_due_us`，避免"往回拖进度条画面冻结"（只增不减的
  累加器在新位置前会永不满足 present 守卫）。

### 8. (re)start / 换片源重置
`CMD_START` 重置全部时钟（`play_start_us`、pacer、`paused_accum_us`/`paused`），
暂停过再换片源也不会因残留暂停累计导致播放时钟错乱。

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
