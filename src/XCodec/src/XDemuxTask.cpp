#include "XDemuxTask.h"
#include "AVLog.h"

XDemuxTask::XDemuxTask()
{
    setName("DemuxTask");
    LOGD("解封装任务创建");
}

XDemuxTask::~XDemuxTask()
{
    LOGD("解封装任务销毁");
    close();
}

void XDemuxTask::reset()
{
    XTask::reset();

    LOGD("重置解封装任务");

    if (demuxer_)
    {
        demuxer_->close();
        demuxer_.reset();
    }

    video_stream_ = nullptr;
    audio_stream_ = nullptr;
    streams_.clear();

    total_packets_ = 0;
    video_packets_ = 0;
    audio_packets_ = 0;
}

auto XDemuxTask::open(const std::string& url) -> bool
{
    url_ = url;

    try
    {
        demuxer_ = Demuxer::create(url);

        if (!demuxer_->open())
        {
            LOGE("打开文件失败: " << url);
            return false;
        }

        demuxer_->dumpInfo();

        video_stream_ = demuxer_->getVideoStream();
        audio_stream_ = demuxer_->getAudioStream();
        streams_      = demuxer_->getStreams();

        LOGI("成功打开文件: " << url);
        if (video_stream_)
        {
            LOGI("视频流: " << video_stream_->codecpar->width << "x" << video_stream_->codecpar->height);
        }
        if (audio_stream_)
        {
            LOGI("音频流: " << audio_stream_->codecpar->sample_rate << "Hz");
        }

        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("打开文件异常: " << e.what());
        return false;
    }
}

auto XDemuxTask::close() -> void
{
    stop();
    wait();

    if (demuxer_)
    {
        demuxer_->close();
        demuxer_.reset();
    }

    LOGI("解封装任务关闭");
}

auto XDemuxTask::getVideoStream() const -> AVStream*
{
    return video_stream_;
}

auto XDemuxTask::getAudioStream() const -> AVStream*
{
    return audio_stream_;
}

auto XDemuxTask::getStreams() const -> const std::vector<AVStream*>&
{
    return streams_;
}

auto XDemuxTask::getDuration() const -> double
{
    if (!demuxer_)
    {
        return 0.0;
    }
    return demuxer_->getDuration();
}

auto XDemuxTask::getFilename() const -> std::string
{
    return url_;
}

auto XDemuxTask::getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>
{
    return demuxer_ ? demuxer_->getCodecParameters(stream_index) : nullptr;
}

auto XDemuxTask::seek(double timestamp, int stream_index) -> bool
{
    if (!demuxer_)
    {
        return false;
    }

    LOGI("开始同步 Seek 到: " << timestamp << "秒");

    // 1. 记录当前暂停状态
    bool was_paused = isPaused();

    // 2. 如果没有暂停，先暂停
    if (!was_paused)
    {
        setPaused(true);
        LOGI("Seek: 暂停解封装器");
    }

    // 3. 等待下游队列清空
    int wait_count = 0;
    while (wait_count < 30)
    {
        size_t decode_queue = (next_ && next_->getQueueSize()) ? next_->getQueueSize() : 0;
        if (decode_queue == 0)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        wait_count++;
    }
    LOGI("Seek: 下游队列已清空");

    // ✅ 4. 使用 clear() 只清空队列，不停止线程
    if (next_)
    {
        next_->clear();
    }

    // 5. 执行 seek
    bool ret = demuxer_->seek(timestamp, stream_index);
    if (ret)
    {
        current_time_ = timestamp;
        LOGI("Seek: 定位成功");
    }
    else
    {
        LOGE("Seek: 定位失败");
    }

    // 6. 重置帧率时间基准
    resetFrameTime();

    // 7. 恢复之前的暂停状态
    if (!was_paused)
    {
        setPaused(false);
        LOGI("Seek: 恢复解封装器");
    }

    LOGI("同步 Seek 完成");
    return ret;
}

auto XDemuxTask::setRtspOptions(bool use_tcp, int timeout_ms) -> void
{
    if (demuxer_)
    {
        demuxer_->setRtspOptions(use_tcp, timeout_ms);
    }
}

auto XDemuxTask::getStats() const -> XDemuxTask::Stats
{
    Stats stats;
    stats.total_packets = total_packets_.load();
    stats.video_packets = video_packets_.load();
    stats.audio_packets = audio_packets_.load();
    return stats;
}

void XDemuxTask::setPaused(bool paused)
{
    XTask::setPaused(paused);
    if (!paused)
    {
        resetFrameTime();
    }
}

void XDemuxTask::resetFrameTime()
{
    next_frame_time_ = std::chrono::steady_clock::now();
}

auto XDemuxTask::process() -> void
{
    LOGI("解封装线程开始运行");

    eof_reached_           = false;
    int       fail_count   = 0;
    const int max_fails    = 5;
    int       packet_count = 0;

    /// 帧率控制
    next_frame_time_ = std::chrono::steady_clock::now();

    while (!shouldStop())
    {
        /// 检查暂停
        if (shouldPause())
        {
            continue;
        }

        if (next_ && next_->getQueueSize() > max_queue_size_)
        {
            sleep(10);
            continue;
        }

        PacketWrapper pkt;
        int           ret = demuxer_->readPacket(pkt);

        if (ret == AVERROR_EOF)
        {
            LOGI("文件读取完成，共读取 " << packet_count << " 个包");
            notifyEof();
            break;
        }
        else if (ret == -2)
        {
            LOGE("解封装器需要重建");
            if (demuxer_->rebuild())
            {
                LOGI("解封装器重建成功");
                fail_count = 0;
                continue;
            }
            else
            {
                handleError("解封装器重建失败");
                break;
            }
        }
        else if (ret < 0)
        {
            fail_count++;
            LOGE("读取错误: " << ret << " (连续失败: " << fail_count << ")");
            if (fail_count >= max_fails)
            {
                handleError("读取连续失败");
                break;
            }
            sleep(100);
            continue;
        }

        fail_count = 0;
        packet_count++;
        ++total_packets_;

        if (const AVStream* stream = demuxer_->getStream(pkt->stream_index))
        {
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                ++video_packets_;

                /// 更新当前播放时间
                if (pkt->pts != AV_NOPTS_VALUE)
                {
                    AVRational time_base = stream->time_base;
                    double     time_sec  = pkt->pts * av_q2d(time_base);
                    current_time_        = time_sec;
                }

                /// 计算这一帧应该等待的时间（毫秒）
                int64_t wait_ms = 40;

                if (pkt->duration > 0)
                {
                    AVRational time_base = stream->time_base;
                    wait_ms              = av_rescale_q(pkt->duration, time_base, { 1, 1000 });
                }

                /// 倍速控制
                double speed = speed_.load();
                if (speed > 0)
                {
                    wait_ms = (int64_t)(wait_ms / speed);
                }

                if (wait_ms > 0 && wait_ms < 500)
                {
                    next_frame_time_ += std::chrono::milliseconds(wait_ms);
                }
                else
                {
                    next_frame_time_ += std::chrono::milliseconds(40);
                }

                auto pkt_clone = pkt.clone();
                notifyObservers(std::move(pkt_clone));

                if (next_)
                {
                    next_->pushPacket(std::make_unique<PacketWrapper>(std::move(pkt)));
                }

                std::this_thread::sleep_until(next_frame_time_);
            }
        }
    }

    LOGI("解封装线程结束");
}

IMPLEMENT_CREATE(XDemuxTask)
