#include "XEncodeTask.h"
#include "AVLog.h"
#include "FrameWrapper.h"
#include "HardwareContext.h"

XEncodeTask::XEncodeTask()
{
    setName("EncodeTask");
    LOGD("编码任务创建");
}

XEncodeTask::~XEncodeTask()
{
    LOGD("编码任务销毁");
    close();
}

bool XEncodeTask::init(const EncoderConfig& config)
{
    if (initialized_)
    {
        close();
    }

    config_ = config;

    try
    {
        encoder_     = std::make_unique<VideoEncoder>(config_.codec_id, config_);
        initialized_ = true;
        LOGI("编码器初始化成功: " << config_.width << "x" << config_.height << ", 码率: " << config_.bitrate / 1000
                                  << "kbps");
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("编码器初始化失败: " << e.what());
        encoder_.reset();
        initialized_ = false;
        return false;
    }
}

void XEncodeTask::close()
{
    if (encoder_)
    {
        LOGI("编码器关闭");
        encoder_.reset();
    }
    initialized_ = false;
}

AVCodecContext* XEncodeTask::getCodecContext() const
{
    return encoder_ ? encoder_->get_ctx() : nullptr;
}

void XEncodeTask::reset()
{
    XTask::reset();
    close();
}

void XEncodeTask::process()
{
    LOGI("编码任务开始运行");

    while (!shouldStop())
    {
        AVFrame* raw_frame = popFrame();

        if (!raw_frame)
        {
            if (eof_reached_ && frame_queue_.empty())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (!encoder_)
        {
            LOGE("编码器未初始化");
            av_frame_free(&raw_frame);
            break;
        }

        // 打印帧信息（只打印前几帧）
        static int frame_count = 0;
        if (frame_count < 5)
        {
            LOGI("帧 " << frame_count << " 格式: " << av_get_pix_fmt_name((AVPixelFormat)raw_frame->format)
                       << ", 宽度: " << raw_frame->width << ", 高度: " << raw_frame->height);

            // 保存第一帧到文件
            if (frame_count == 0)
            {
                LOGI("保存第一帧到 debug_frame.yuv");
                FILE* f = fopen("debug_frame.yuv", "wb");
                if (f)
                {
                    // Y 平面
                    for (int i = 0; i < raw_frame->height; i++)
                    {
                        fwrite(raw_frame->data[0] + i * raw_frame->linesize[0], 1, raw_frame->width, f);
                    }
                    // U 平面
                    for (int i = 0; i < raw_frame->height / 2; i++)
                    {
                        fwrite(raw_frame->data[1] + i * raw_frame->linesize[1], 1, raw_frame->width / 2, f);
                    }
                    // V 平面
                    for (int i = 0; i < raw_frame->height / 2; i++)
                    {
                        fwrite(raw_frame->data[2] + i * raw_frame->linesize[2], 1, raw_frame->width / 2, f);
                    }
                    fclose(f);
                    LOGI("debug_frame.yuv 保存成功");
                }
                else
                {
                    LOGE("无法创建 debug_frame.yuv 文件");
                }
            }
            frame_count++;
        }

        FrameWrapper frame(raw_frame);
        AVFrame*     encode_frame = frame;

        // 编码帧
        std::vector<AVPacket*> packets;
        try
        {
            encoder_->encode_frame(encode_frame, packets);

            for (auto* pkt : packets)
            {
                if (next_)
                {
                    auto wrapper = std::make_unique<PacketWrapper>();
                    av_packet_ref(wrapper->get(), pkt);
                    next_->pushPacket(std::move(wrapper));
                }
                av_packet_free(&pkt);
            }
        }
        catch (const std::exception& e)
        {
            LOGE("编码帧失败: " << e.what());
        }
    }


    // 刷新编码器
    if (encoder_)
    {
        LOGI("刷新编码器...");
        auto packets = encoder_->flush();
        for (auto* pkt : packets)
        {
            if (next_)
            {
                auto wrapper = std::make_unique<PacketWrapper>();
                av_packet_ref(wrapper->get(), pkt);
                next_->pushPacket(std::move(wrapper));
            }
            av_packet_free(&pkt);
        }
    }

    LOGI("编码任务结束");
}

IMPLEMENT_CREATE(XEncodeTask)
