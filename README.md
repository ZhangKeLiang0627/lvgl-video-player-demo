# LVGL Video Player

一个运行在嵌入式 Linux 帧缓冲（framebuffer）上的 **LVGL 视频播放器**，用 C++ 编写，
支持软解播放（FFmpeg）→ 屏幕显示 + ALSA 音频输出、音画同步、播放/暂停、可拖动进度条、
音量调节，以及一个可进入子文件夹的**文件浏览器 / 播放列表**。

> A C++ LVGL video player for embedded Linux framebuffers: soft-decoded video
> (FFmpeg) to screen + ALSA audio, with A/V sync, play/pause, a seekable
> progress bar, volume control, and a folder-navigable file-browser playlist.

---

## 特性 / Features

- 🎞️ 基于 `lv_ffmpeg` 的软解视频播放（H.264 等），直接上屏到 `/dev/fb0`
- 🔊 进程内独立音频线程（FFmpeg 解码 AAC → ALSA `plughw`），**音画从属同一墙钟**
- ⏯️ 播放 / 暂停（音画一起停、一起恢复，不回跳）
- 🎚️ 可拖动进度条（归一化 0..1000 范围，避开 LVGL `lv_map` 的 int32 溢出）
- 🔉 音量滑块（软件增益 0..2.0×，低延迟 ALSA 缓冲，响应跟手）
- 📂 文件浏览器 / 播放列表：从 `ROOT_DIR` 起层层进入子文件夹，只显示视频文件
- 📊 屏幕右上帧率 / 左下内存监视器（LVGL sysmon）
- 🖥️ 屏幕正上方居中显示当前文件名 + 完整路径

## 目录结构 / Layout

```
lvgl-video-player/
├── CMakeLists.txt              # 开源构建（LVGL 作为 git submodule）
├── Makefile                    # 板子本地构建（已在 RK3566 验证）
├── lv_conf.h                   # LVGL 配置（供 CMake 构建使用）
├── include/                    # 公共头文件
│   ├── config.h                # 编译期配置（VIDEO_PATH / TOUCH_DEV / ...）
│   ├── App.h                   # 顶层编排
│   ├── Player.h                # 视频播放器封装（含 lv_ffmpeg 补丁的前向声明）
│   ├── AudioEngine.h           # 音频引擎（FFmpeg 解码 + ALSA 播放）
│   ├── FileBrowser.h           # 文件浏览器 / 播放列表
│   └── Ui.h                    # 所有 LVGL 控件与事件回调
├── src/                        # 实现
│   ├── main.cpp
│   ├── App.cpp
│   ├── Player.cpp
│   ├── AudioEngine.cpp
│   ├── FileBrowser.cpp
│   └── Ui.cpp
├── patches/lv_ffmpeg/          # 给 lv_ffmpeg.c 打的补丁（pause/seek/duration）
│   ├── lv_ffmpeg_v9.5.0.patch
│   └── README.md
└── docs/
    └── architecture.md         # 模块设计与音画同步原理
```

## 构建 / Build

### 方式 A：板子本地构建（已验证 / tested on RK3566）

把本仓库放在 LVGL 源码树的**同级目录**（例如 `/home/cat/lvgl_video_player/`
与 `/home/cat/lvgl/` 并列），先给 `lv_ffmpeg.c` 打补丁，再编译：

```sh
# 1) 应用补丁（只需一次；LVGL 需为 v9.5.0）
cd /home/cat/lvgl
git apply -p1 /home/cat/lvgl_video_player/patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch
# 若 lvgl 不是 git 树： patch -p1 -N -i /home/cat/lvgl_video_player/patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch

# 2) 编译
cd /home/cat/lvgl_video_player
make
./demo
```

依赖：`libavformat / libavcodec / libavutil / libswresample`（开发包）、
`libasound2-dev`、LVGL 9.x 源码树。

### 方式 B：CMake（干净检出 / fresh checkout）

```sh
git clone --recurse-submodules <this-repo> lvgl-video-player
cd lvgl-video-player
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

CMake 会在 configure 阶段自动给 LVGL 子模块的 `lv_ffmpeg.c` 打补丁。

## 配置 / Configuration

在 `include/config.h` 或用编译宏覆盖：

| 宏           | 默认值                 | 说明                          |
|--------------|------------------------|-------------------------------|
| `VIDEO_PATH` | `/userdata/my_test.mp4`| 默认播放文件                  |
| `TOUCH_DEV`  | `/dev/input/event1`    | 触摸屏输入设备                |
| `AUDIO_DEV`  | `plughw:0,0`           | ALSA 播放设备                 |
| `ROOT_DIR`   | `/userdata`            | 文件浏览器根目录（不可越出）  |
| `VOL_MAX_GAIN` | `2.0f`               | 音量滑块最大增益              |

## 已知限制 / Notes

- 视频软解，CPU 占用较高（RK3566 上约 90%+）；如需硬解 H.264 需另接
  RKMPP / GStreamer 管线（不在本工程范围）。
- 补丁只针对 LVGL 的 `lv_ffmpeg`；升级 LVGL 时需重新评估补丁是否仍适用。

## License

MIT —— 见 [LICENSE](LICENSE)。
