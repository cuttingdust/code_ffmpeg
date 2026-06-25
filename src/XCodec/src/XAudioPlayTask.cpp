#include "XAudioPlayTask.h"

#include "AVLog.h"

#include <thread>

XAudioPlayTask::XAudioPlayTask()
{
    setName("AudioPlayTask");
    LOGD("音频播放任务创建");
}

XAudioPlayTask::~XAudioPlayTask()
{
    reset();
    LOGD("音频播放任务销毁");
}

void XAudioPlayTask::reset()
{
    XTask::reset();

    stop();
    wait();

    if (player_)
    {
        player_->close();
        player_.reset();
    }

    resetPtsClock();
    device_started_ = false;
    sample_rate_    = 44100;
    channels_       = 2;
    speed_          = 1.0;
}

auto XAudioPlayTask::openFromDecoder(const AudioDecoder* decoder, int device_samples) -> bool
{
    if (!decoder)
    {
        LOGE("openFromDecoder: decoder 为空");
        return false;
    }
    return openPlayer(decoder->output_sample_rate(), decoder->output_channels(), device_samples);
}

auto XAudioPlayTask::openPlayer(int sample_rate, int channels, int device_samples) -> bool
{
    if (sample_rate <= 0 || channels <= 0)
    {
        LOGE("openPlayer: 无效参数");
        return false;
    }

    if (!player_)
    {
        player_.reset(XAudioPlay::create());
    }
    if (!player_)
    {
        LOGE("XAudioPlay::create 失败");
        return false;
    }

    XAudioPlay::Spec spec;
    spec.sample_rate = sample_rate;
    spec.channels    = channels;
    spec.samples     = device_samples;

    if (!player_->open(spec))
    {
        LOGE("XAudioPlay::open 失败: " << player_->lastError());
        return false;
    }

    player_->setSpeed(1.0);

    sample_rate_      = sample_rate;
    channels_         = channels;
    device_started_   = false;
    resetPtsClock();

    LOGI("音频播放设备已打开: " << sample_rate << "Hz, " << channels << "ch");
    return true;
}

auto XAudioPlayTask::getPlayer() const -> XAudioPlay*
{
    return player_.get();
}

void XAudioPlayTask::setSpeed(double speed, double media_pts_sec)
{
    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    const double old_speed = speed_.load(std::memory_order_relaxed);

    if (player_ && std::abs(old_speed - speed) > 1e-6)
    {
        player_->clearQueue();
    }

    if (clock_started_ && std::abs(old_speed - speed) > 1e-6)
    {
        const auto now = std::chrono::steady_clock::now();
        if (media_pts_sec >= 0.0)
        {
            first_pts_sec_ = media_pts_sec;
        }
        else
        {
            const double elapsed_sec =
                    std::chrono::duration<double>(now - playback_start_wall_).count();
            first_pts_sec_ += elapsed_sec * old_speed;
        }
        playback_start_wall_ = now;
    }

    speed_.store(speed, std::memory_order_relaxed);
}

void XAudioPlayTask::setVolume(double volume)
{
    if (player_)
    {
        player_->setVolume(volume);
    }
}

void XAudioPlayTask::flushDownstream()
{
    if (player_)
    {
        player_->clearQueue();
    }
    resetPtsClock();
    device_started_ = false;
}

void XAudioPlayTask::setPaused(bool paused)
{
    const bool was_paused = isPaused();
    XTask::setPaused(paused);

    if (!player_ || !player_->isOpen() || was_paused == paused)
    {
        return;
    }

    if (paused)
    {
        pause_wall_ = std::chrono::steady_clock::now();
        player_->pause();
    }
    else if (device_started_)
    {
        if (clock_started_)
        {
            const auto paused_for = std::chrono::steady_clock::now() - pause_wall_;
            playback_start_wall_ += paused_for;
        }
        player_->start();
    }
}

void XAudioPlayTask::resetPtsClock()
{
    clock_started_       = false;
    first_pts_sec_       = 0.0;
    synthetic_pts_sec_   = 0.0;
    playback_start_wall_ = std::chrono::steady_clock::time_point{};
    pause_wall_          = std::chrono::steady_clock::time_point{};
}

void XAudioPlayTask::reanchorPtsClock(double frame_pts_sec)
{
    first_pts_sec_       = frame_pts_sec;
    synthetic_pts_sec_   = frame_pts_sec;
    playback_start_wall_ = std::chrono::steady_clock::now();
    clock_started_       = true;
}

/// \brief 将当前 PCM 帧映射为「媒体时间轴上的秒数」，供 waitUntilPts 做 sleep 同步
///
/// 两条路径：
/// 1. 正常：frame->pts 有效（AudioDecoder 重采样后会保留 pts 并带上 stream time_base）→ 秒 = pts * time_base
/// 2. 兜底：pts == AV_NOPTS_VALUE（部分流/flush 帧无时间戳）→ 用 synthetic_pts_sec_ 按样本数累加伪时间戳
///
/// \note 仅兜底路径会推进 synthetic_pts_sec_；有真实 pts 时不改 synthetic，避免与文件时间轴混用
/// \note 返回值为「该帧起始时刻」的秒数，不是帧结束时刻
auto XAudioPlayTask::framePtsSec(const AVFrame* frame) -> double
{
    /// 主路径：上游解码器提供的 DTS/PTS（AudioDecoder 在 resample 后写入 decoded->pts + config_.time_base）
    if (frame && frame->pts != AV_NOPTS_VALUE)
    {
        return frame->pts * av_q2d(frame->time_base);
    }

    /// 兜底路径：无 pts 时，用已播放样本累计时长作为当前帧的「合成 pts」
    /// synthetic_pts_sec_ 表示下一帧若仍无 pts，应从哪个秒数开始（单调递增）
    const double pts = synthetic_pts_sec_;

    /// 按本帧样本数推进合成时钟：duration = nb_samples / sample_rate_（与 open 后 PCM 采样率一致）
    if (frame && frame->nb_samples > 0 && sample_rate_ > 0)
    {
        synthetic_pts_sec_ += static_cast<double>(frame->nb_samples) / static_cast<double>(sample_rate_);
    }
    return pts;
}

void XAudioPlayTask::waitUntilPts(double frame_pts_sec)
{
    if (!clock_started_)
    {
        first_pts_sec_       = frame_pts_sec;
        playback_start_wall_ = std::chrono::steady_clock::now();
        clock_started_       = true;
        return;
    }

    double speed = speed_.load(std::memory_order_relaxed);
    if (speed <= 0.0)
    {
        speed = 1.0;
    }

    const auto target = playback_start_wall_
                        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                std::chrono::duration<double>((frame_pts_sec - first_pts_sec_) / speed));

    const auto now = std::chrono::steady_clock::now();
    if (now >= target)
    {
        const auto behind_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - target).count();
        if (behind_ms > kResyncLagMs)
        {
            LOGW("音频 PTS 落后 " << behind_ms << "ms，重新对齐时钟");
            reanchorPtsClock(frame_pts_sec);
        }
        else if (behind_ms > kCatchUpLagMs)
        {
            LOGW("音频 PTS 落后 " << behind_ms << "ms，追帧播放");
        }
        return;
    }

    const auto ahead_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(target - now).count();
    if (ahead_ms > kResyncLagMs)
    {
        LOGW("音频 PTS 超前 " << ahead_ms << "ms，重新对齐时钟");
        reanchorPtsClock(frame_pts_sec);
    }

    std::this_thread::sleep_until(target);
}

void XAudioPlayTask::tryStartDevice()
{
    if (device_started_ || !player_)
    {
        return;
    }

    if (player_->queuedBytes() >= kPrebufferBytes)
    {
        player_->start();
        device_started_ = true;
        LOGI("音频设备已开始播放, 预缓冲 " << player_->queuedBytes() << " bytes");
    }
}

auto XAudioPlayTask::pushPcmFrame(AVFrame* frame) -> bool
{
    if (!player_ || !frame || frame->nb_samples <= 0)
    {
        return false;
    }

    const int bytes = frame->nb_samples * channels_
                      * av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
    if (bytes <= 0 || !frame->data[0])
    {
        return false;
    }

    if (!player_->push(frame->data[0], bytes))
    {
        LOGE("XAudioPlay::push 失败");
        return false;
    }

    tryStartDevice();
    return true;
}

void XAudioPlayTask::process()
{
    LOGI("音频播放任务开始运行");

    if (!player_ || !player_->isOpen())
    {
        LOGE("音频播放设备未打开");
        handleError("XAudioPlay 未 open");
        return;
    }

    int consecutive_timeouts     = 0;
    constexpr int max_timeouts   = 30;

    while (!shouldStop())
    {
        if (shouldPause())
        {
            continue;
        }

        AVFrame* frame = popFrame();
        if (!frame)
        {
            consecutive_timeouts++;
            if (consecutive_timeouts >= max_timeouts)
            {
                handleError("音频播放上游超时");
                break;
            }

            if (shouldStop() || (eof_reached_ && getQueueSize() == 0))
            {
                break;
            }
            continue;
        }

        consecutive_timeouts = 0;

        const double pts_sec = framePtsSec(frame);
        waitUntilPts(pts_sec);

        if (!pushPcmFrame(frame))
        {
            av_frame_free(&frame);
            handleError("推送 PCM 失败");
            break;
        }

        av_frame_free(&frame);
    }

    if (!device_started_ && player_ && player_->queuedBytes() > 0)
    {
        player_->start();
        device_started_ = true;
    }

    while (player_ && player_->queuedBytes() > 0 && !shouldStop())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kDrainPollMs));
    }

    if (player_)
    {
        player_->pause();
    }

    LOGI("音频播放任务结束");
}

IMPLEMENT_CREATE(XAudioPlayTask)
