#include "XDecodeTask.h"

#include <utility>
#include "AVException.h"
#include "AVLog.h"
#include "FrameWrapper.h"

XDecodeTask::XDecodeTask()
{
    setName("DecodeTask");
    LOGD("解码任务创建");
}

XDecodeTask::~XDecodeTask()
{
    LOGD("解码任务销毁");
}

auto XDecodeTask::setHardwareDecode(bool enable) -> void
{
    use_hardware_ = enable;
}

void XDecodeTask::reset()
{
    XTask::reset();

    LOGD("重置解码任务");

    if (decoder_)
    {
        decoder_->close();
        decoder_.reset();
    }

    frame_cb_ = nullptr;
}

bool XDecodeTask::initDecoder(AVCodecID codec_id, AVStream* stream)
{
    try
    {
        DecoderConfig config;
        config.codec_id                      = codec_id;
        config.thread_count                  = 16;
        config.hardware.enable               = use_hardware_;
        config.hardware.auto_select          = true;
        config.hardware.preferred_type       = HardwareContext::Type::D3D11VA;
        config.hardware.transfer_to_software = true;

        decoder_ = VideoDecoder::create(config);

        if (!decoder_->set_parameters_from_stream(stream))
        {
            LOGE("设置解码器参数失败");
            return false;
        }

        decoder_->open();
        LOGI("解码器初始化成功");
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("解码器初始化异常: " << e.what());

        LOGI("尝试软件解码...");
        try
        {
            DecoderConfig sw_config;
            sw_config.codec_id        = codec_id;
            sw_config.thread_count    = 16;
            sw_config.hardware.enable = false;

            decoder_ = VideoDecoder::create(sw_config);

            if (!decoder_->set_parameters_from_stream(stream))
            {
                LOGE("软件解码器参数设置失败");
                return false;
            }

            decoder_->open();
            LOGI("软件解码器初始化成功");
            return true;
        }
        catch (const std::exception& sw_e)
        {
            LOGE("软件解码也失败: " << sw_e.what());
            return false;
        }
    }
}

auto XDecodeTask::getDecoder() const -> VideoDecoder*
{
    return decoder_.get();
}

auto XDecodeTask::setFrameCallback(DecoderConfig::FrameCallback cb) -> void
{
    frame_cb_ = std::move(cb);
}

auto XDecodeTask::getStats() const -> VideoDecoder::Stats
{
    return decoder_ ? decoder_->get_stats() : VideoDecoder::Stats();
}

void XDecodeTask::process()
{
    LOGI("解码任务开始运行");

    std::vector<AVFrame*> raw_frames;
    int                   consecutive_errors       = 0;
    constexpr int         max_consecutive_errors   = 3;
    int                   consecutive_timeouts     = 0;
    constexpr int         max_consecutive_timeouts = 30;
    bool                  decoder_corrupted        = false;

    while (!shouldStop() && !decoder_corrupted)
    {
        auto pkt = popPacket();
        if (!pkt)
        {
            consecutive_timeouts++;

            if (consecutive_timeouts >= max_consecutive_timeouts)
            {
                LOGE("解码任务连续超时 " << consecutive_timeouts << " 次，触发重连");
                handleError("上游可能卡死");
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
            int ret = decoder_->decode_packet(*pkt, raw_frames);

            if (ret < 0)
            {
                consecutive_errors++;
                LOGE("解码包错误: " << ret << " (连续错误: " << consecutive_errors << ")");

                if (consecutive_errors >= max_consecutive_errors)
                {
                    LOGE("连续解码错误太多，触发重连");
                    handleError("解码器连续失败");
                    break;
                }
                continue;
            }

            consecutive_errors = 0;

            for (auto* raw_frame : raw_frames)
            {
                FrameWrapper frame(raw_frame);
                frame->pict_type = AV_PICTURE_TYPE_NONE;

                if (frame_cb_)
                {
                    frame_cb_(frame, false);
                }
                else if (next_)
                {
                    next_->pushFrame(frame.release());
                }
            }
            raw_frames.clear();
        }
        catch (const AVException& e)
        {
            LOGE("解码异常: " << e.what());

            if (std::string(e.what()).find("解码器状态损坏") != std::string::npos)
            {
                decoder_corrupted = true;
                handleError("解码器损坏，需要重建");
                break;
            }

            handleError(std::string("解码异常: ") + e.what());
            break;
        }
        catch (const std::exception& e)
        {
            LOGE("标准异常: " << e.what());
            handleError(std::string("异常: ") + e.what());
            break;
        }
    }

    if (!decoder_corrupted && decoder_)
    {
        LOGI("刷新解码器...");
        try
        {
            decoder_->flush(raw_frames);
            for (auto* raw_frame : raw_frames)
            {
                FrameWrapper frame(raw_frame);
                frame->pict_type = AV_PICTURE_TYPE_NONE;

                if (frame_cb_)
                {
                    frame_cb_(frame, false);
                }
                else if (next_)
                {
                    next_->pushFrame(frame.release());
                }
            }
        }
        catch (const std::exception& e)
        {
            LOGE("刷新解码器异常: " << e.what());
        }
    }
    else
    {
        LOGW("解码器已损坏，跳过刷新");
    }

    if (next_)
    {
        next_->notifyEof();
    }

    LOGI("解码任务结束");
}

IMPLEMENT_CREATE(XDecodeTask)
