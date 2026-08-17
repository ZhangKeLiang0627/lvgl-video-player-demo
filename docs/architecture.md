# 架构说明 / Architecture

本工程把原本单文件的 `main.c`（700+ 行、所有逻辑耦合在全局变量里）重构成
职责清晰的 C++ 模块。所有 LVGL / FFmpeg / ALSA / RGA 的 C API 都通过 `extern "C"`
或 LVGL 自带封装在 `.cpp` 中调用，模块之间只依赖 C++ 头文件的明确接口。

## 模块划分 / Modules

```
            ┌─────────────────────────────────────────────┐
            │                  App  (编排)                  │
            │  init() 建显示/触摸/ffmpeg，run() 主循环        │
            │  uiRefreshCb() 周期发布视频墙钟给音频           │
            └──────┬──────────┬──────────┬────────────┬────┘
                   │          │          │            │
              ┌────▼───┐ ┌─────▼─────┐ ┌──▼────────┐ ┌─▼──────────┐
              │ Player │ │AudioEngine│ │FileBrowser│ │ display_rga│
              │ (视频) │ │  (音频)   │ │(播放列表) │ │(RGA 上屏) │
              └────────┘ └─────┬─────┘ └─────┬──────┘ └────────────┘
                   │           │            │
                   └───────────┼────────────┘
                               │ 控件回调
                          ┌────▼────┐
                          │   Ui    │
                          │ (控件层)│
                          └─────────┘
```

### `App` (`app.h/.cpp`)
顶层编排者，持有 `Player`、`AudioEngine`、`FileBrowser`、`Ui` 四个成员。
- `init()`：初始化 LVGL、显示（fbdev + RGA 或 SDL）、evdev 触摸（含旋转校准与
  逆变换）、ffmpeg；创建播放器与音频引擎；构建 UI；注册 200ms 周期定时器。
- `run()`：主循环。设备端为**无 syscall 紧凑轮询** `while(1){ lv_timer_handler(); }`
  （RK3566 内核每次 syscall 约 10ms CFS 调度惩罚，紧凑轮询把帧间隔从 52ms 降到
  ~28ms）；`PLAYER_USE_SDL` 构建则每轮 `sleep_for(16ms)` 让桌面窗口响应、不占满
  CPU。LVGL tick 由实时时钟回调驱动（无需 `lv_tick_inc`）。
- `uiRefresh()`：读视频当前位置 → `audio.publishVideoMs()`（视频是主时钟）→
  在非拖动/非 seek 稳定期内更新进度条。
- 接收 UI 回调：`onTogglePlay / onVolume / onSeekPress / onSeekRelease / playFile / toggleBrowser`。

### `Player` (`player.h/.cpp`)
对 `lv_ffmpeg` 播放器对象的薄封装：`create / setSrc / start / pause / resume / seek /
getTime / getDuration`。头文件里用 `extern "C"` 前向声明了打补丁后新增的
`lv_ffmpeg_player_*` 函数，业务代码无需触碰 `lv_ffmpeg.h`。

### `AudioEngine` (`audio_engine.h/.cpp`)
进程内音频播放引擎，自带 `std::thread` 工作线程，是视频的**从时钟**。
- 用 FFmpeg 独立解封装音频流 → AAC 解码 → `swr_convert` 转 S16LE/44.1k →
  `snd_pcm_writei` 阻塞写入 ALSA（默认 ~46ms 低延迟缓冲）。
- 跨线程状态用 `std::atomic`：`videoMs_`(主钟)、`paused_`、`audioSeekMs_`、
  `audioReopen_`、`volume_`。
- 在 **暂停 / 恢复 / 用户 seek / 漂移 >800ms / 切换文件** 五种情况下，把自身 demux
  `av_seek_frame` 重新对齐到当前视频时间，并 `flush` 解码器 + 重置重采样 + 清空
  PCM 缓冲，保证音画永远在同一位置。
- `reopen(path)` 通过原子标志让工作线程关闭旧文件、热打开新文件，**无需重启进程**。

### `FileBrowser` (`file_browser.h/.cpp`)
可进入子文件夹的文件浏览器 / 播放列表。
- 从 `ROOT_DIR` 出发，懒扫描**当前目录**（不在启动时递归，避免卡顿）。
- 只显示目录与视频扩展名文件（`.mp4/.mkv/.avi/...`），其余隐藏。
- 点目录 → 进入下一层；点 `.. (up)` → 回上一级（不越出 `ROOT_DIR`）；点文件 →
  `App::playFile()`。UI 文本为英文（默认字体 Montserrat 不含 CJK 字形）。
- 列表项用 `user_data` 存下标，回调里查 `entries_`，避免堆字符串泄漏。

### `Ui` (`ui.h/.cpp`)
构建并持有所有控件：左上播放/暂停按钮、右上 📂 列表按钮、顶部居中文件名/路径
标签、底部进度条（归一化 0..1000）、进度条正上方时长标签、底部居中音量滑块、
Play 右侧的一键截图按钮。所有 LVGL 事件回调是 static 跳板函数，从控件
`user_data` 取回 `App*` 再转发到 `App` 的对应方法。

### `display_rga` (`src/display_rga.cpp`) —— RGA 硬件上屏驱动
`LVGL_ROTATE`/`-r` 旋转后，若直接让 LVGL 做软件转置，RK3566 内存带宽会被整帧
转置压垮（横屏掉到 1~2fps）。本驱动让 LVGL 以**逻辑横屏**（rotation=0）渲染
（Flush 纯 memcpy），再用 **RGA 2D `imrotate()` 把整帧旋转写入 `/dev/fb0`**，
每帧 <2ms。SDL 构建只编译空桩（不依赖 librga）。

### `utils/lv_snapshot` (`src/utils/lv_snapshot.*`)
LVGL 快照 → PNG 截屏工具：`lv_snapshot_take()` 取 RGB888 draw buffer，自写
zlib PNG 编码（IHDR/IDAT/IEND + CRC32，BGR→RGB 交换）。CLI 触发：
`--shot <file>`（启动 2s 后单张）/ `--shot-dir <dir> [--shot-period <sec>]`（周期）。

## 旋转与触摸 / Rotation & touch

- 旋转：`-r 90/180/270` 或 `LVGL_ROTATE` 环境变量；`screen.h` 的 `g_screen` 保存
  旋转后逻辑分辨率，UI 全部用 `spct(宽/高, 百分比)` 自适应。
- 显示：RGA `IM_HAL_TRANSFORM_ROT_90/180/270` 旋转上屏（见 `display_rga`）。
- 触摸：`lv_evdev_create` 后先 `lv_evdev_set_calibration(0,0,RAW_W,RAW_H)` 喂真实
  原生分辨率（goodix 多点屏单点轴为 0/0，LVGL 自动探测会把长轴截断导致右半屏
  点不到），再包一层 `touch_read_rotated` 按 RGA 上屏方向做逆变换
  （90°→`(ry, H-1-rx)` 等），让触点与画面一一对应。

## 音画同步原理 / A/V sync

视频呈现由 `lv_ffmpeg` 补丁的**帧时钟 pace** 驱动：播放器定时器以 2ms 细粒度
采样，每帧**解码超前一帧**，仅当播放时钟（`now - play_start - paused_accum`）
跨过帧边界（`present_due_us += frame_period_us`，按片源 `avg_frame_rate` 全精度
微秒计算）才上屏——上屏节奏与解码耗时解耦，23.976/24/25/30fps 均稳定。UI 定时器
每 200ms 把视频位置 `publishVideoMs()` 给 `AudioEngine`，音频线程把它当作**主时钟**：

- 暂停：`snd_pcm_drop` 立即静音；恢复时 `seekTo(当前视频时间)` 重新对齐。
- 拖动进度条：视频 `seek(ms)` 重锚时钟（同时重锚 pacer 的 `present_due_us` 并
  `avformat_flush` + `avcodec_flush_buffers`，避免 seek 残留状态），再
  `requestSeek(ms)` 通知音频；音频 `seekTo` 会**排空所有 PTS 早于目标的音频帧**，
  避免从 GOP 起点滞后几秒。
- 漂移：音频位置与主钟偏差 >800ms 时自动 `seekTo` 校正。
- (re)start / 换片源：`CMD_START` 重置全部时钟（`play_start_us`、pacer、暂停累计），
  从干净状态开始。

> 进度条用 0..1000 归一化范围而非毫秒值，是因为 LVGL `lv_map()` 内部用 `int32_t`
> 做乘法；100 分钟的毫秒值平方会溢出，导致拖到后半段被钳回 0。

## 编译 / Build

见仓库根 `README.md`：`Makefile` 用于板子本地构建（已验证，硬解链 RKMPP + RGA），
`CMakeLists.txt` 用于带 LVGL submodule 的干净检出（configure 阶段自动打补丁），
`-DENABLE_SDL=ON` 构建桌面 SDL 仿真（x86_64 软解，UI/交互与真机一致）。
