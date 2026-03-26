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

    return demuxer_->seek(timestamp, stream_index);
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

auto XDemuxTask::process() -> void
{
    LOGI("解封装线程开始运行");

    eof_reached_         = false;
    int       fail_count = 0;
    const int max_fails  = 5;

    while (!shouldStop())
    {
        if (next_ && next_->getQueueSize() > max_queue_size_)
        {
            sleep(10);
            continue;
        }

        PacketWrapper pkt;
        int           ret = demuxer_->readPacket(pkt);
        if (ret == AVERROR_EOF)
        {
            LOGI("文件读取完成");
            notifyEof();
            break;
        }
        else if (ret == -2) /// 需要重建
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
        ++total_packets_;

        if (const AVStream* stream = demuxer_->getStream(pkt->stream_index))
        {
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                ++video_packets_;

                auto pkt_clone = pkt.clone();
                notifyObservers(std::move(pkt_clone));

                if (next_)
                {
                    next_->pushPacket(std::make_unique<PacketWrapper>(std::move(pkt)));
                }
            }
        }
    }
}


IMPLEMENT_CREATE(XDemuxTask)
