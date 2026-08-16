#include "AudioEngine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <unistd.h>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/avutil.h>
    #include <libswresample/swresample.h>
    #include <alsa/asoundlib.h>
}

#include "config.h"

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
    closeFile();
}

/* ------------------------------------------------------------------ */
/* FFmpeg / ALSA open & close                                         */
/* ------------------------------------------------------------------ */

bool AudioEngine::openFile(const std::string & path)
{
    AVFormatContext * fmt = nullptr;
    if (avformat_open_input(&fmt, path.c_str(), NULL, NULL) != 0) {
        fprintf(stderr, "[audio] avformat_open_input failed: %s\n", path.c_str());
        return false;
    }
    if (avformat_find_stream_info(fmt, NULL) < 0) {
        fprintf(stderr, "[audio] find_stream_info failed\n");
        avformat_close_input(&fmt);
        return false;
    }
    int ast = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ast < 0) {
        fprintf(stderr, "[audio] no audio stream in %s\n", path.c_str());
        avformat_close_input(&fmt);
        return false;
    }
    AVCodecParameters * par = fmt->streams[ast]->codecpar;
    const AVCodec * dec = avcodec_find_decoder(par->codec_id);
    if (!dec) {
        fprintf(stderr, "[audio] no decoder for codec_id=%d\n", par->codec_id);
        avformat_close_input(&fmt);
        return false;
    }
    AVCodecContext * ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(ctx, par);
    if (avcodec_open2(ctx, dec, NULL) < 0) {
        fprintf(stderr, "[audio] avcodec_open2 failed\n");
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }

    snd_pcm_t * pcm = nullptr;
    int rc = snd_pcm_open(&pcm, AUDIO_DEV, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "[audio] snd_pcm_open %s failed: %s\n", AUDIO_DEV, snd_strerror(rc));
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }
    snd_pcm_hw_params_t * hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
    unsigned rate = 44100;
    snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
    snd_pcm_hw_params_set_channels(pcm, hw, 2);
    snd_pcm_uframes_t period = 512;
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, 0);
    snd_pcm_uframes_t buf_frames = period * 4;   /* 2048 frames ~= 46 ms */
    snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buf_frames);
    if (snd_pcm_hw_params(pcm, hw) < 0) {
        fprintf(stderr, "[audio] snd_pcm_hw_params failed\n");
        snd_pcm_close(pcm);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }
    snd_pcm_prepare(pcm);

    SwrContext * swr = swr_alloc();
    int64_t in_ch = ctx->channel_layout ? ctx->channel_layout
                                        : av_get_default_channel_layout(ctx->channels);
    av_opt_set_channel_layout(swr, "in_channel_layout",  in_ch, 0);
    av_opt_set_int(swr, "in_sample_rate",  ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", ctx->sample_fmt, 0);
    av_opt_set_channel_layout(swr, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(swr, "out_sample_rate", 44100, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    if (swr_init(swr) < 0) {
        fprintf(stderr, "[audio] swr_init failed\n");
        swr_free(&swr);
        snd_pcm_close(pcm);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt);
        return false;
    }

    /* Success: adopt the handles. */
    fmt_  = fmt;
    ast_  = ast;
    ctx_  = ctx;
    swr_  = swr;
    pcm_  = pcm;
    audioPosMs_  = 0;
    audioPrimed_ = 0;
    audioSuspended_ = 0;
    return true;
}

void AudioEngine::closeFile()
{
    if (pcm_)  { snd_pcm_drop(pcm_);  snd_pcm_close(pcm_);  pcm_  = nullptr; }
    if (swr_)  { swr_free(&swr_);     swr_  = nullptr; }
    if (ctx_)  { avcodec_free_context(&ctx_); ctx_ = nullptr; }
    if (fmt_)  { avformat_close_input(&fmt_); fmt_ = nullptr; }
    ast_ = -1;
}

/* ------------------------------------------------------------------ */
/* Seek: re-anchor the audio demuxer to `target_ms` and DRAIN every    */
/* frame whose PTS is still before the target. Without the drain, a     */
/* BACKWARD seek lands on the video keyframe (possibly seconds earlier) */
/* and the audio would stay permanently behind the picture.             */
/* ------------------------------------------------------------------ */

void AudioEngine::seekTo(int32_t target_ms)
{
    if (!fmt_) return;
    int64_t target_us = (int64_t)target_ms * 1000;
    AVRational ab = fmt_->streams[ast_]->time_base;
    int64_t ts = av_rescale_q(target_us, AV_TIME_BASE_Q, ab);
    av_seek_frame(fmt_, ast_, ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(ctx_);
    swr_init(swr_);
    snd_pcm_drop(pcm_);
    snd_pcm_prepare(pcm_);

    AVPacket * dpkt = av_packet_alloc();
    while (av_read_frame(fmt_, dpkt) >= 0) {
        if (dpkt->stream_index != ast_) { av_packet_unref(dpkt); continue; }
        int64_t pms = (dpkt->pts != AV_NOPTS_VALUE)
            ? av_rescale_q(dpkt->pts, ab, AV_TIME_BASE_Q) / 1000
            : -1;
        if (pms >= 0 && pms < (int64_t)target_ms - 60) { av_packet_unref(dpkt); continue; }
        av_packet_unref(dpkt);   /* first frame at/after target: drop it,
                                    the main loop reads the next one */
        break;
    }
    av_packet_free(&dpkt);

    audioPosMs_   = target_ms;
    audioPrimed_  = 0;
    fprintf(stderr, "[audio] seek -> target=%" PRId32 "\n", target_ms);
}

void AudioEngine::open(const std::string & path)
{
    if (!openFile(path)) {
        fprintf(stderr, "[audio] initial open failed; running without audio\n");
    }
}

void AudioEngine::reopen(const std::string & path)
{
    {
        std::lock_guard<std::mutex> lk(reopenMtx_);
        reopenPath_ = path;
    }
    audioReopen_.store(1);
}

void AudioEngine::start()
{
    if (running_.load()) return;
    pkt_    = av_packet_alloc();
    frame_  = av_frame_alloc();
    outBuf_ = (int16_t *)malloc(8192 * 2 * sizeof(int16_t));
    running_.store(true);
    thread_ = std::thread(&AudioEngine::worker, this);
}

/* ------------------------------------------------------------------ */
/* Worker thread                                                       */
/* ------------------------------------------------------------------ */

void AudioEngine::worker()
{
    for (;;) {
        if (stop_.load()) break;

        /* Hot-swap to a newly selected file. */
        if (audioReopen_.load()) {
            std::string path;
            {
                std::lock_guard<std::mutex> lk(reopenMtx_);
                path = reopenPath_;
            }
            closeFile();
            if (openFile(path)) {
                audioPosMs_ = 0; audioPrimed_ = 0; audioSuspended_ = 0;
                fprintf(stderr, "[audio] reopened -> %s\n", path.c_str());
            } else {
                fprintf(stderr, "[audio] reopen failed: %s\n", path.c_str());
            }
            audioReopen_.store(0);
            usleep(20000);
            continue;
        }
        if (!fmt_) { usleep(50000); continue; }

        int32_t vt = videoMs_.load();   /* master clock */

        if (paused_.load()) {
            if (audioSeekMs_.load() >= 0) {
                seekTo(audioSeekMs_.load());
                audioSeekMs_.store(-1);
            }
            if (!audioSuspended_) {
                snd_pcm_drop(pcm_);
                audioSuspended_ = 1;
                fprintf(stderr, "[audio] suspended\n");
            }
            usleep(20000);
            continue;
        } else if (audioSuspended_) {
            seekTo(vt);
            audioSuspended_ = 0;
            fprintf(stderr, "[audio] resumed -> vt=%" PRId32 "\n", vt);
        }

        if (audioPrimed_ &&
            (vt > audioPosMs_ + 800 || audioPosMs_ > vt + 800)) {
            seekTo(vt);
            fprintf(stderr, "[audio] drift-correct -> vt=%" PRId32 " audio=%" PRId64 "\n",
                    vt, audioPosMs_);
        }

        while (av_read_frame(fmt_, pkt_) >= 0) {
            if (stop_.load())    break;
            if (paused_.load())  break;
            if (audioReopen_.load()) break;
            if (audioSeekMs_.load() >= 0) {
                seekTo(audioSeekMs_.load());
                audioSeekMs_.store(-1);
                av_packet_unref(pkt_);
                continue;
            }
            if (pkt_->stream_index != ast_) {
                av_packet_unref(pkt_);
                continue;
            }
            if (avcodec_send_packet(ctx_, pkt_) == 0) {
                while (avcodec_receive_frame(ctx_, frame_) == 0) {
                    uint8_t * out_ptr = (uint8_t *)outBuf_;
                    int out_samples = swr_convert(swr_, &out_ptr, 8192,
                                                  (const uint8_t **)frame_->data,
                                                  frame_->nb_samples);
                    if (out_samples > 0) {
                        float vol = volume_.load();
                        int16_t * p = outBuf_;
                        for (int s = 0; s < out_samples * 2; s++) {
                            int v = (int)(p[s] * vol);
                            if (v > 32767) v = 32767;
                            else if (v < -32768) v = -32768;
                            p[s] = (int16_t)v;
                        }
                        snd_pcm_sframes_t w = snd_pcm_writei(pcm_, outBuf_, out_samples);
                        if (w < 0) snd_pcm_recover(pcm_, (int)w, 1);
                        audioPosMs_ += (int64_t)out_samples * 1000 / 44100;
                        audioPrimed_ = 1;
                    }
                    av_frame_unref(frame_);
                }
            }
            av_packet_unref(pkt_);
        }

        /* End of file: loop back to the start (auto-restart mirror). */
        av_seek_frame(fmt_, ast_, 0, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(ctx_);
    }

    if (pkt_)   { av_packet_free(&pkt_);   pkt_ = nullptr; }
    if (frame_) { av_frame_free(&frame_);  frame_ = nullptr; }
    free(outBuf_);
    outBuf_ = nullptr;
    running_.store(false);
}
