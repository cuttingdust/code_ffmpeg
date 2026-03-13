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

        /// 获取流信息
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

bool XDemuxTask::seek(double timestamp, int stream_index)
{
    if (!demuxer_)
        return false;
    return demuxer_->seek(timestamp, stream_index);
}

void XDemuxTask::setRtspOptions(bool use_tcp, int timeout_ms)
{
    if (demuxer_)
    {
        demuxer_->setRtspOptions(use_tcp, timeout_ms);
    }
}

XDemuxTask::Stats XDemuxTask::getStats() const
{
    Stats stats;
    stats.total_packets = total_packets_.load();
    stats.video_packets = video_packets_.load();
    stats.audio_packets = audio_packets_.load();
    return stats;
}

void XDemuxTask::process()
{
    LOGI("解封装线程开始运行");

    eof_reached_ = false;

    while (!shouldStop())
    {
        /// 背压控制：如果下游队列太大，等待
        if (next_ && next_->getQueueSize() > max_queue_size_)
        {
            sleep(10);
            continue;
        }

        PacketWrapper pkt;

        /// 读取数据包
        int ret = demuxer_->readPacket(pkt);

        if (ret == AVERROR_EOF)
        {
            LOGI("文件读取完成，共 " << total_packets_ << " 个包");

            // 使用 notifyEof 通知所有下游任务
            notifyEof();
            break;
        }
        else if (ret < 0)
        {
            LOGE("读取数据包错误，错误码: " << ret);
            handleError("读取数据包失败");
            break;
        }

        total_packets_++;

        // 统计
        AVStream* stream = demuxer_->getStream(pkt->stream_index);
        if (stream)
        {
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                video_packets_++;

                // 只传递视频包给下游
                if (next_)
                {
                    next_->pushPacket(std::make_unique<PacketWrapper>(std::move(pkt)));
                }
            }
            else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audio_packets_++;
                // 音频包暂不处理
            }
        }
    }

    LOGI("解封装线程结束，视频包: " << video_packets_ << ", 音频包: " << audio_packets_);
}

IMPLEMENT_CREATE(XDemuxTask)
