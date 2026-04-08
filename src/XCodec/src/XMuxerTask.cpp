#include "XMuxerTask.h"
#include "AVLog.h"

XMuxerTask::XMuxerTask()
{
    setName("MuxerTask");
    LOGD("封装任务创建");
}

XMuxerTask::~XMuxerTask()
{
    LOGD("封装任务销毁");
    close();
}

bool XMuxerTask::init(const std::string& filename, AVCodecContext* enc_ctx, AVRational time_base, AVRational frame_rate)
{
    if (muxer_)
    {
        LOGW("已经在封装中");
        return false;
    }

    if (!enc_ctx)
    {
        LOGE("编码器上下文为空");
        return false;
    }

    filename_     = filename;
    frame_rate_   = frame_rate;
    start_pts_    = AV_NOPTS_VALUE;
    frame_index_  = 0;
    packet_count_ = 0;

    // 使用标准 MP4 时间基（1/90000）
    AVRational mux_time_base = { 1, 90000 };
    time_base_               = mux_time_base;

    try
    {
        muxer_ = std::make_unique<Muxer>(filename);
        if (!muxer_->open())
        {
            LOGE("无法创建输出文件: " << filename);
            muxer_.reset();
            return false;
        }

        // 添加视频流，使用封装时间基
        int out_idx = muxer_->addStream(enc_ctx, mux_time_base);
        if (out_idx < 0)
        {
            LOGE("添加视频流失败");
            muxer_.reset();
            return false;
        }

        // 写入文件头
        if (muxer_->writeHeader() < 0)
        {
            LOGE("写入文件头失败");
            muxer_.reset();
            return false;
        }

        LOGI("封装器初始化成功: " << filename);
        LOGI("封装时间基: " << time_base_.num << "/" << time_base_.den);
        LOGI("帧率: " << frame_rate_.num << "/" << frame_rate_.den);
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("创建封装器异常: " << e.what());
        muxer_.reset();
        return false;
    }
}

void XMuxerTask::close()
{
    if (!muxer_)
    {
        return;
    }

    LOGI("封装器关闭，共写入 " << packet_count_ << " 个包");
    muxer_->writeTrailer();
    muxer_.reset();
    packet_count_ = 0;
    start_pts_    = AV_NOPTS_VALUE;
    frame_index_  = 0;
}

void XMuxerTask::reset()
{
    XTask::reset();
    close();
}

void XMuxerTask::process()
{
    LOGI("封装任务开始运行");

    // 计算每帧的持续时间（基于封装时间基）
    // 封装时间基 1/90000，帧率 25fps → 90000/25 = 3600
    int64_t frame_duration = 0;
    if (frame_rate_.num > 0 && frame_rate_.den > 0)
    {
        frame_duration = av_rescale_q(1, av_make_q(frame_rate_.den, frame_rate_.num), time_base_);
        LOGI("=== 调试信息 ===");
        LOGI("封装时间基: " << time_base_.num << "/" << time_base_.den);
        LOGI("帧率: " << frame_rate_.num << "/" << frame_rate_.den);
        LOGI("帧持续时间: " << frame_duration);
    }
    else
    {
        frame_duration = 3600; // 默认 25fps
        LOGW("使用默认帧持续时间: " << frame_duration);
    }

    while (!shouldStop())
    {
        // 从队列获取编码后的包
        auto pkt = popPacket();

        if (!pkt)
        {
            if (eof_reached_ && packet_queue_.empty())
            {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (!muxer_)
        {
            LOGE("封装器未初始化");
            break;
        }

        AVPacket* av_pkt = pkt->get();

        // 等待关键帧才开始写入
        if (start_pts_ == AV_NOPTS_VALUE)
        {
            if (av_pkt->flags & AV_PKT_FLAG_KEY)
            {
                start_pts_   = av_pkt->pts;
                frame_index_ = 0;
                LOGI("找到关键帧，开始写入，起始PTS: " << start_pts_);
            }
            else
            {
                continue; // 丢弃非关键帧
            }
        }

        // 重新计算 PTS/DTS，确保时间戳连续递增
        int64_t pts = frame_index_ * frame_duration;
        int64_t dts = frame_index_ * frame_duration;

        av_pkt->pts      = pts;
        av_pkt->dts      = dts;
        av_pkt->duration = frame_duration;

        // 调试：打印前几帧
        if (frame_index_ < 10)
        {
            LOGI("帧 " << frame_index_ << " PTS: " << pts << ", duration: " << frame_duration);
        }

        frame_index_++;

        // 写入文件
        int ret = av_interleaved_write_frame(muxer_->getContext(), av_pkt);
        if (ret < 0)
        {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOGE("写入包失败: " << err_buf);
        }
        else
        {
            packet_count_++;
            if (packet_count_ % 50 == 0)
            {
                LOGD("已写入包数: " << packet_count_ << ", 当前帧索引: " << frame_index_);
            }
        }
    }

    LOGI("封装任务结束");
}

IMPLEMENT_CREATE(XMuxerTask)
