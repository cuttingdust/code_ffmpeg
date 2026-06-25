#pragma once

#include "XTask.h"
#include "XAudioPlay.h"
#include "AudioDecoder.h"

#include <atomic>
#include <chrono>
#include <memory>

/// \brief 音频播放 Task：接收 S16 AVFrame，PTS 同步后 push 到 XAudioPlay
class XCODEC_EXPORT XAudioPlayTask : public XTask
{
    DECLARE_CREATE(XAudioPlayTask)
public:
    XAudioPlayTask();
    ~XAudioPlayTask() override;

    /// 按解码器输出参数打开 SDL 设备（open 后设备暂停，预缓冲够再 start）
    auto openFromDecoder(const AudioDecoder* decoder, int device_samples = 1024) -> bool;

    auto openPlayer(int sample_rate, int channels, int device_samples = 1024) -> bool;

    auto getPlayer() const -> XAudioPlay*;

    /// 播放倍速：仅 PTS 墙钟调度（PCM 倍速由上游 atempo 处理，设备层固定 1.0）
    auto setSpeed(double speed, double media_pts_sec = -1.0) -> void;

    auto setVolume(double volume) -> void;

    void flushDownstream() override;

    void setPaused(bool paused) override;

    auto reset() -> void override;

protected:
    auto process() -> void override;

private:
    auto resetPtsClock() -> void;

    auto reanchorPtsClock(double frame_pts_sec) -> void;

    auto framePtsSec(const AVFrame* frame) -> double;

    auto waitUntilPts(double frame_pts_sec) -> void;

    auto tryStartDevice() -> void;

    auto pushPcmFrame(AVFrame* frame) -> bool;

private:
    std::unique_ptr<XAudioPlay> player_;

    int sample_rate_ = 44100;
    int channels_    = 2;

    std::atomic<double> speed_{ 1.0 };

    bool                                          clock_started_ = false;
    double                                        first_pts_sec_ = 0.0;
    double                                        synthetic_pts_sec_ = 0.0;
    std::chrono::steady_clock::time_point         playback_start_wall_{};
    std::chrono::steady_clock::time_point         pause_wall_{};

    bool device_started_ = false;

    static constexpr std::size_t kPrebufferBytes   = 4096 * 4;
    static constexpr int         kCatchUpLagMs     = 100;
    static constexpr int         kResyncLagMs        = 500;
    static constexpr int         kDrainPollMs      = 20;
};
