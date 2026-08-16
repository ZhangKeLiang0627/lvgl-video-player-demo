# 架构说明 / Architecture

本工程把原本单文件的 `main.c`（700+ 行、所有逻辑耦合在全局变量里）重构成
职责清晰的 C++ 模块。所有 LVGL / FFmpeg / ALSA 的 C API 都通过 `extern "C"`
或 LVGL 自带封装在 `.cpp` 中调用，模块之间只依赖 C++ 头文件的明确接口。

## 模块划分 / Modules

```
            ┌─────────────────────────────────────────────┐
            │                  App  (编排)                  │
            │  init() 建显示/触摸/ffmpeg，run() 主循环        │
            │  uiRefreshCb() 周期发布视频墙钟给音频           │
            └──────────┬──────────┬──────────┬─────────────┘
                       │          │          │
                  ┌────▼───┐ ┌─────▼─────┐ ┌──▼─────────┐
                  │ Player │ │AudioEngine│ │ FileBrowser│
                  │ (视频) │ │  (音频)   │ │ (播放列表) │
                  └────────┘ └─────┬─────┘ └─────┬──────┘
                       │           │            │
                       └───────────┼────────────┘
                                   │ 控件回调
                              ┌────▼────┐
                              │   Ui    │
                              │ (控件层)│
                              └─────────┘
```

### `App` (`App.h/.cpp`)
顶层编排者，持有 `Player`、`AudioEngine`、`FileBrowser`、`Ui` 四个成员。
- `init()`：初始化 LVGL、fbdev 显示、evdev 触摸、ffmpeg；创建播放器与音频引擎；
  构建 UI；注册 200ms 周期定时器。
- `run()`：主循环，`clock_gettime` 取真实 dt 喂 `lv_tick_inc`，再 `lv_timer_handler`。
- `uiRefresh()`：读视频当前位置 → `audio.publishVideoMs()`（视频是主时钟）→
  在非拖动/非 seek 稳定期内更新进度条。
- 接收 UI 回调：`onTogglePlay / onVolume / onSeekPress / onSeekRelease / playFile / toggleBrowser`。

### `Player` (`Player.h/.cpp`)
对 `lv_ffmpeg` 播放器对象的薄封装：`create / setSrc / start / pause / resume / seek /
getTime / getDuration`。头文件里用 `extern "C"` 前向声明了打补丁后新增的
`lv_ffmpeg_player_*` 函数，业务代码无需触碰 `lv_ffmpeg.h`。

### `AudioEngine` (`AudioEngine.h/.cpp`)
进程内音频播放引擎，自带 `std::thread` 工作线程，是视频的**从时钟**。
- 用 FFmpeg 独立解封装音频流 → AAC 解码 → `swr_convert` 转 S16LE/44.1k →
  `snd_pcm_writei` 阻塞写入 ALSA（默认 ~46ms 低延迟缓冲）。
- 跨线程状态用 `std::atomic`：`videoMs_`(主钟)、`paused_`、`audioSeekMs_`、
  `audioReopen_`、`volume_`。
- 在 **暂停 / 恢复 / 用户 seek / 漂移 >800ms / 切换文件** 五种情况下，把自身 demux
  `av_seek_frame` 重新对齐到当前视频时间，并 `flush` 解码器 + 重置重采样 + 清空
  PCM 缓冲，保证音画永远在同一位置。
- `reopen(path)` 通过原子标志让工作线程关闭旧文件、热打开新文件，**无需重启进程**。

### `FileBrowser` (`FileBrowser.h/.cpp`)
可进入子文件夹的文件浏览器 / 播放列表。
- 从 `ROOT_DIR` 出发，懒扫描**当前目录**（不在启动时递归，避免卡顿）。
- 只显示目录与视频扩展名文件（`.mp4/.mkv/.avi/...`），其余隐藏。
- 点目录 → 进入下一层；点 `..` → 回上一级（不越出 `ROOT_DIR`）；点文件 →
  `App::playFile()`。
- 列表项用 `user_data` 存下标，回调里查 `entries_`，避免堆字符串泄漏。

### `Ui` (`Ui.h/.cpp`)
构建并持有所有控件：左上播放/暂停按钮、右上 📂 列表按钮、顶部居中文件名/路径
标签、底部进度条（归一化 0..1000）、进度条正上方时长标签、底部居中音量滑块。
所有 LVGL 事件回调是 static 跳板函数，从控件 `user_data` 取回 `App*` 再转发到
`App` 的对应方法。

## 音画同步原理 / A/V sync

视频帧由 `lv_ffmpeg` 按**墙钟**驱动（`lv_ffmpeg_player_get_time` 基于
`play_start_us`，暂停时累计 `paused_accum_us`）。UI 定时器每 200ms 把视频位置
`publishVideoMs()` 给 `AudioEngine`，音频线程把它当作**主时钟**：

- 暂停：`snd_pcm_drop` 立即静音；恢复时 `seekTo(当前视频时间)` 重新对齐。
- 拖动进度条：视频 `seek(ms)` 重锚时钟，同时 `requestSeek(ms)` 通知音频；
  音频 `seekTo` 会**排空所有 PTS 早于目标的音频帧**，避免从 GOP 起点滞后几秒。
- 漂移：音频位置与主钟偏差 >800ms 时自动 `seekTo` 校正。

> 进度条用 0..1000 归一化范围而非毫秒值，是因为 LVGL `lv_map()` 内部用 `int32_t`
> 做乘法；100 分钟的毫秒值平方会溢出，导致拖到后半段被钳回 0。

## 编译 / Build

见仓库根 `README.md`：`Makefile` 用于板子本地构建（已验证），`CMakeLists.txt`
用于带 LVGL submodule 的干净检出（configure 阶段自动打补丁）。
