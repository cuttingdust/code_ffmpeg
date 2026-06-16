#include "XAudioPlayTask.h"

#include "AVLog.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

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

void XAudioPlayTask::setSpeed(double speed)
{
    if (speed <= 0.0)
    {
        speed = 1.0;
    }
    speed_ = speed;
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

auto XAudioPlayTask::framePtsSec(const AVFrame* frame) -> double
{
    if (frame && frame->pts != AV_NOPTS_VALUE)
    {
        return frame->pts * av_q2d(frame->time_base);
    }

    const double pts = synthetic_pts_sec_;
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
        if (behind_ms > kCatchUpLagMs)
        {
            LOGW("音频 PTS 落后 " << behind_ms << "ms，追帧播放");
        }
        return;
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
