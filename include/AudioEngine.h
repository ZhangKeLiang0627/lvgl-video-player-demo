#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>

/* ALSA uses a typedef (snd_pcm_t), not a forward-declarable struct. */
#include <alsa/asoundlib.h>

/*
 * In-process audio playback engine.
 *
 * Decodes the AAC (or other) audio stream of the playing file with FFmpeg and
 * writes signed-16 stereo PCM to an ALSA device. The engine runs its own worker
 * thread and acts as a SLAVE to the video wall-clock: the UI thread publishes the
 * current video position via publishVideoMs(), and the audio thread re-seeks its
 * own demuxer whenever it is paused, asked to seek, switches files, or drifts
 * from the video. A small ALSA buffer keeps volume changes responsive.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /* Open the given file and spawn the worker thread. Safe to call once. */
    void open(const std::string & path);
    void start();

    /* Master clock: UI thread calls this ~5x/sec with the video position. */
    void publishVideoMs(int32_t ms) { videoMs_.store(ms); }

    /* Pause / resume the audio in lock-step with the video. */
    void setPaused(bool paused) { paused_.store(paused ? 1 : 0); }

    /* Request a seek to `ms` (video and audio re-anchor together). */
    void requestSeek(int32_t ms) { audioSeekMs_.store(ms); }

    /* Hot-swap to a different file without restarting the process. */
    void reopen(const std::string & path);

    /* Software volume gain (e.g. 0.0 .. VOL_MAX_GAIN). */
    void setVolume(float gain) { volume_.store(gain); }
    float volume() const { return volume_.load(); }

    bool running() const { return running_.load(); }

private:
    void worker();
    bool openFile(const std::string & path);
    void closeFile();
    void seekTo(int32_t target_ms);

    /* FFmpeg / ALSA handles (touched only by the worker thread). */
    struct AVFormatContext * fmt_ = nullptr;
    int                      ast_ = -1;
    struct AVCodecContext  * ctx_ = nullptr;
    struct SwrContext      * swr_ = nullptr;
    snd_pcm_t              * pcm_ = nullptr;
    struct AVPacket        * pkt_ = nullptr;
    struct AVFrame         * frame_ = nullptr;
    int16_t                * outBuf_ = nullptr;

    /* Slave-clock state, owned by the worker thread. */
    int64_t audioPosMs_  = 0;
    int     audioPrimed_ = 0;
    int     audioSuspended_ = 0;

    /* Cross-thread flags (atomics). */
    std::atomic<int32_t> videoMs_{0};     /* master clock from UI */
    std::atomic<int>     paused_{0};
    std::atomic<int32_t> audioSeekMs_{-1};
    std::atomic<int>     audioReopen_{0};
    std::atomic<float>   volume_{1.0f};
    std::atomic<bool>    stop_{false};
    std::atomic<bool>    running_{false};

    std::string reopenPath_;
    std::mutex  reopenMtx_;

    std::thread thread_;
};
