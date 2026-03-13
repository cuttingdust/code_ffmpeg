#include "XDecodeTask.h"
#include "AVLog.h"

XDecodeTask::XDecodeTask()
{
    setName("DecodeTask");
    LOGD("解码任务创建");
}

XDecodeTask::~XDecodeTask()
{
    LOGD("解码任务销毁");
}

bool XDecodeTask::initDecoder(AVCodecID codec_id, AVStream* stream)
{
    try
    {
        DecoderConfig config;
        config.codec_id                      = codec_id;
        config.thread_count                  = 16;
        config.hardware.enable               = true;
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
        return false;
    }
}

void XDecodeTask::process()
{
    LOGI("解码任务开始运行");

    std::vector<AVFrame*> frames;

    while (!shouldStop())
    {
        auto pkt = popPacket();
        if (!pkt)
        {
            if (eof_reached_)
                break;
            continue;
        }

        decoder_->decode_packet(*pkt, frames);

        for (auto* frame : frames)
        {
            // 清除帧类型信息，让编码器自己决定
            frame->pict_type = AV_PICTURE_TYPE_NONE;

            if (frame_cb_)
            {
                // 直接回调渲染
                frame_cb_(frame, false);
                av_frame_free(&frame);
            }
            else if (next_)
            {
                // 传递给下游任务
                next_->pushFrame(frame);
            }
            else
            {
                av_frame_free(&frame);
            }
        }
        frames.clear();
    }

    // 刷新解码器
    LOGI("刷新解码器...");
    decoder_->flush(frames);
    for (auto* frame : frames)
    {
        frame->pict_type = AV_PICTURE_TYPE_NONE;

        if (frame_cb_)
        {
            frame_cb_(frame, false);
            av_frame_free(&frame);
        }
        else if (next_)
        {
            next_->pushFrame(frame);
        }
        else
        {
            av_frame_free(&frame);
        }
    }

    LOGI("解码任务结束");
}

IMPLEMENT_CREATE(XDecodeTask)
