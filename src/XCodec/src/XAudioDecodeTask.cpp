#include "XAudioDecodeTask.h"

#include "AVException.h"
#include "AVLog.h"
#include "FrameWrapper.h"

XAudioDecodeTask::XAudioDecodeTask()
{
    setName("AudioDecodeTask");
    LOGD("音频解码任务创建");
}

XAudioDecodeTask::~XAudioDecodeTask()
{
    LOGD("音频解码任务销毁");
}

void XAudioDecodeTask::reset()
{
    XTask::reset();

    LOGD("重置音频解码任务");

    stop();
    wait();

    if (decoder_)
    {
        decoder_->close();
        decoder_.reset();
    }

    tempo_filter_.close();
    speed_ = 1.0;
}

auto XAudioDecodeTask::initDecoder(AVStream* stream) -> bool
{
    if (!stream || !stream->codecpar)
    {
        LOGE("音频流无效");
        return false;
    }

    if (stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
    {
        LOGE("非音频流");
        return false;
    }

    try
    {
        AudioDecoderConfig config;
        config.thread_count = 8;

        decoder_ = AudioDecoder::create(config);

        if (!decoder_->set_parameters_from_stream(stream))
        {
            LOGE("设置音频解码参数失败");
            decoder_.reset();
            return false;
        }

        decoder_->open();

        if (!tempo_filter_.open(decoder_->output_sample_rate(),
                                decoder_->output_channels(),
                                decoder_->output_sample_format()))
        {
            LOGE("AudioAtempoFilter 初始化失败");
            decoder_.reset();
            return false;
        }
        tempo_filter_.setSpeed(speed_.load(std::memory_order_relaxed));

        LOGI("音频解码器初始化成功: " << decoder_->output_sample_rate() << "Hz, "
                                       << decoder_->output_channels() << "ch");
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("音频解码器初始化异常: " << e.what());
        decoder_.reset();
        return false;
    }
}

auto XAudioDecodeTask::getDecoder() const -> AudioDecoder*
{
    return decoder_.get();
}

void XAudioDecodeTask::flushDownstream()
{
    need_flush_decoder_ = true;
    tempo_filter_.resetPipeline();
}

void XAudioDecodeTask::setSpeed(double speed)
{
    if (speed <= 0.0)
    {
        speed = 1.0;
    }
    speed_.store(speed, std::memory_order_relaxed);
    tempo_filter_.setSpeed(speed);
}

auto XAudioDecodeTask::getStats() const -> AudioDecoder::Stats
{
    return decoder_ ? decoder_->get_stats() : AudioDecoder::Stats{};
}

void XAudioDecodeTask::pushPcmFrames(std::vector<AVFrame*>& frames)
{
    for (auto* raw_frame : frames)
    {
        if (!raw_frame)
        {
            continue;
        }

        std::vector<AVFrame*> filtered;
        try
        {
            if (tempo_filter_.process(raw_frame, filtered) < 0)
            {
                av_frame_free(&raw_frame);
                continue;
            }
        }
        catch (const std::exception& e)
        {
            LOGE("AudioAtempoFilter 处理失败: " << e.what());
            av_frame_free(&raw_frame);
            continue;
        }

        for (AVFrame* pcm : filtered)
        {
            FrameWrapper frame(pcm);
            if (next_)
            {
                next_->pushFrame(frame.release());
            }
        }
    }
    frames.clear();
}

void XAudioDecodeTask::process()
{
    LOGI("音频解码任务开始运行");

    if (!decoder_)
    {
        LOGE("音频解码器未初始化");
        handleError("AudioDecoder 未 init");
        return;
    }

    std::vector<AVFrame*> pcm_frames;
    int                   consecutive_errors       = 0;
    constexpr int         max_consecutive_errors   = 3;
    int                   consecutive_timeouts     = 0;
    constexpr int         max_consecutive_timeouts = 30;

    while (!shouldStop())
    {
        if (shouldPause())
        {
            continue;
        }

        if (need_flush_decoder_.load())
        {
            need_flush_decoder_ = false;
            decoder_->flushBuffers();
            LOGI("音频解码器缓存已刷新");
        }

        auto pkt = popPacket();
        if (!pkt)
        {
            consecutive_timeouts++;

            if (consecutive_timeouts >= max_consecutive_timeouts)
            {
                LOGE("音频解码任务连续超时 " << consecutive_timeouts << " 次");
                handleError("音频上游可能卡死");
                break;
            }

            if (shouldStop() || (eof_reached_ && packet_queue_.empty()))
            {
                break;
            }
            continue;
        }

        consecutive_timeouts = 0;

        try
        {
            const int ret = decoder_->decode_packet(*pkt, pcm_frames);
            if (ret < 0)
            {
                consecutive_errors++;
                LOGE("音频解码包错误: " << ret << " (连续: " << consecutive_errors << ")");

                if (consecutive_errors >= max_consecutive_errors)
                {
                    handleError("音频解码连续失败");
                    break;
                }
                continue;
            }

            consecutive_errors = 0;
            pushPcmFrames(pcm_frames);
        }
        catch (const AVException& e)
        {
            LOGE("音频解码异常: " << e.what());
            handleError(std::string("音频解码异常: ") + e.what());
            break;
        }
        catch (const std::exception& e)
        {
            LOGE("音频解码标准异常: " << e.what());
            handleError(std::string("音频解码异常: ") + e.what());
            break;
        }
    }

    try
    {
        decoder_->flush(pcm_frames);
        pushPcmFrames(pcm_frames);

        std::vector<AVFrame*> tail_frames;
        tempo_filter_.flushOutput(tail_frames);
        pushPcmFrames(tail_frames);
    }
    catch (const std::exception& e)
    {
        LOGE("音频解码 flush 异常: " << e.what());
    }

    if (next_)
    {
        next_->notifyEof();
    }

    const auto stats = decoder_->get_stats();
    LOGI("音频解码任务结束, 包=" << stats.packets_processed << " 帧=" << stats.frames_decoded);
}

IMPLEMENT_CREATE(XAudioDecodeTask)
