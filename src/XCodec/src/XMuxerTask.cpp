#include "XMuxerTask.h"
#include "AVLog.h"

XMuxerTask::XMuxerTask()
{
    setName("MuxerTask");
    setMaxQueueSize(256);
    LOGD("封装任务创建");
}

XMuxerTask::~XMuxerTask()
{
    LOGD("封装任务销毁");
    close();
}

bool XMuxerTask::init(const std::string& filename, AVCodecContext* enc_ctx, AVRational time_base, AVRational frame_rate,
                      AVStream* audio_stream)
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

    filename_           = filename;
    frame_rate_         = frame_rate;
    start_pts_          = AV_NOPTS_VALUE;
    frame_index_        = 0;
    packet_count_       = 0;
    audio_pts_offset_   = AV_NOPTS_VALUE;
    last_audio_dts_     = AV_NOPTS_VALUE;
    audio_packet_count_ = 0;
    video_started_      = false;
    has_audio_          = false;
    in_audio_stream_index_  = -1;
    out_audio_stream_index_ = -1;

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

        int out_idx = muxer_->addStream(enc_ctx, mux_time_base);
        if (out_idx < 0)
        {
            LOGE("添加视频流失败");
            muxer_.reset();
            return false;
        }

        if (audio_stream)
        {
            out_audio_stream_index_ = muxer_->addAudioStream(audio_stream);
            if (out_audio_stream_index_ < 0)
            {
                LOGE("添加音频流失败");
                muxer_.reset();
                return false;
            }

            in_audio_stream_index_ = audio_stream->index;
            in_audio_time_base_    = audio_stream->time_base;
            has_audio_             = true;
            LOGI("录制音频: " << audio_stream->codecpar->sample_rate << "Hz, codec="
                              << avcodec_get_name(audio_stream->codecpar->codec_id));
        }

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

    LOGI("封装器关闭，共写入视频包 " << packet_count_ << "，音频包 " << audio_packet_count_);
    muxer_->writeTrailer();
    muxer_.reset();
    packet_count_       = 0;
    audio_packet_count_ = 0;
    start_pts_          = AV_NOPTS_VALUE;
    frame_index_        = 0;
    audio_pts_offset_   = AV_NOPTS_VALUE;
    last_audio_dts_     = AV_NOPTS_VALUE;
    video_started_      = false;
    has_audio_          = false;
    clearPendingAudio();
}

void XMuxerTask::reset()
{
    XTask::reset();
    close();
}

void XMuxerTask::clearPendingAudio()
{
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    while (!audio_queue_.empty())
    {
        audio_queue_.pop();
    }
}

void XMuxerTask::onVideoKeyFrameReady()
{
    clearPendingAudio();
    audio_pts_offset_ = AV_NOPTS_VALUE;
    last_audio_dts_   = AV_NOPTS_VALUE;
    video_started_    = true;
    LOGI("视频关键帧就绪，开始接收音频");
}

void XMuxerTask::pushAudioPacket(PacketWrapper::Ptr pkt)
{
    if (!pkt || !has_audio_ || !video_started_.load())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    if (audio_queue_.size() >= max_queue_size_)
    {
        audio_queue_.pop();
        LOGW("音频队列已满，丢弃最旧包");
    }
    audio_queue_.push(std::move(pkt));
}

int64_t XMuxerTask::previewOutputDts(const AVPacket* pkt) const
{
    if (!muxer_ || !pkt)
    {
        return AV_NOPTS_VALUE;
    }

    AVFormatContext* ctx = muxer_->getContext();
    if (!ctx || out_audio_stream_index_ < 0
        || std::cmp_greater_equal(out_audio_stream_index_, static_cast<int>(ctx->nb_streams)))
    {
        return AV_NOPTS_VALUE;
    }

    int64_t src_ts = AV_NOPTS_VALUE;
    if (pkt->dts != AV_NOPTS_VALUE)
    {
        src_ts = pkt->dts;
    }
    else if (pkt->pts != AV_NOPTS_VALUE)
    {
        src_ts = pkt->pts;
    }

    if (src_ts == AV_NOPTS_VALUE)
    {
        return AV_NOPTS_VALUE;
    }

    if (audio_pts_offset_ != AV_NOPTS_VALUE)
    {
        src_ts -= audio_pts_offset_;
    }

    AVRational out_tb = ctx->streams[out_audio_stream_index_]->time_base;
    return av_rescale_q_rnd(src_ts, in_audio_time_base_, out_tb,
                            static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
}

void XMuxerTask::writeAudioPacket(PacketWrapper::Ptr pkt)
{
    if (!muxer_ || !pkt || !has_audio_ || !video_started_.load())
    {
        return;
    }

    auto cloned = pkt->clone();
    if (!cloned)
    {
        return;
    }

    AVPacket* av_pkt = cloned->get();

    if (audio_pts_offset_ == AV_NOPTS_VALUE)
    {
        if (av_pkt->pts != AV_NOPTS_VALUE)
        {
            audio_pts_offset_ = av_pkt->pts;
        }
        else if (av_pkt->dts != AV_NOPTS_VALUE)
        {
            audio_pts_offset_ = av_pkt->dts;
        }
        else
        {
            return;
        }
        LOGI("音频起始 PTS 偏移: " << audio_pts_offset_);
    }

    int64_t out_dts = previewOutputDts(av_pkt);
    if (out_dts != AV_NOPTS_VALUE && last_audio_dts_ != AV_NOPTS_VALUE && out_dts <= last_audio_dts_)
    {
        return;
    }

    int ret = muxer_->writePacket(av_pkt, in_audio_stream_index_, out_audio_stream_index_, in_audio_time_base_,
                                  audio_pts_offset_);
    if (ret < 0)
    {
        char err_buf[256];
        av_strerror(ret, err_buf, sizeof(err_buf));
        LOGE("写入音频包失败: " << err_buf);
    }
    else
    {
        audio_packet_count_++;
        if (out_dts != AV_NOPTS_VALUE)
        {
            last_audio_dts_ = out_dts;
        }
    }
}

void XMuxerTask::drainAudioQueue()
{
    while (true)
    {
        PacketWrapper::Ptr audio_pkt;
        {
            std::lock_guard<std::mutex> lock(audio_queue_mutex_);
            if (audio_queue_.empty())
            {
                break;
            }
            audio_pkt = std::move(audio_queue_.front());
            audio_queue_.pop();
        }
        writeAudioPacket(std::move(audio_pkt));
    }
}

void XMuxerTask::process()
{
    LOGI("封装任务开始运行");

    int64_t frame_duration = 0;
    if (frame_rate_.num > 0 && frame_rate_.den > 0)
    {
        frame_duration = av_rescale_q(1, av_make_q(frame_rate_.den, frame_rate_.num), time_base_);
        LOGI("帧持续时间: " << frame_duration);
    }
    else
    {
        frame_duration = 3600;
        LOGW("使用默认帧持续时间: " << frame_duration);
    }

    while (!shouldStop())
    {
        bool did_work = false;

        auto pkt = popPacket();
        if (pkt)
        {
            did_work = true;

            if (!muxer_)
            {
                LOGE("封装器未初始化");
                break;
            }

            AVPacket* av_pkt = pkt->get();

            if (start_pts_ == AV_NOPTS_VALUE)
            {
                if (av_pkt->flags & AV_PKT_FLAG_KEY)
                {
                    start_pts_   = av_pkt->pts;
                    frame_index_ = 0;
                    onVideoKeyFrameReady();
                    LOGI("找到关键帧，开始写入，起始PTS: " << start_pts_);
                }
                else
                {
                    continue;
                }
            }

            int64_t pts      = frame_index_ * frame_duration;
            av_pkt->pts      = pts;
            av_pkt->dts      = pts;
            av_pkt->duration = frame_duration;
            av_pkt->stream_index = 0;
            frame_index_++;

            int ret = av_interleaved_write_frame(muxer_->getContext(), av_pkt);
            if (ret < 0)
            {
                char err_buf[256];
                av_strerror(ret, err_buf, sizeof(err_buf));
                LOGE("写入视频包失败: " << err_buf);
            }
            else
            {
                packet_count_++;
            }
        }

        if (video_started_.load())
        {
            size_t drained = 0;
            while (drained < 32)
            {
                PacketWrapper::Ptr audio_pkt;
                {
                    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
                    if (audio_queue_.empty())
                    {
                        break;
                    }
                    audio_pkt = std::move(audio_queue_.front());
                    audio_queue_.pop();
                }
                writeAudioPacket(std::move(audio_pkt));
                ++drained;
                did_work = true;
            }
        }

        if (!did_work)
        {
            if (eof_reached_ && packet_queue_.empty())
            {
                drainAudioQueue();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    LOGI("封装任务结束");
}

IMPLEMENT_CREATE(XMuxerTask)
