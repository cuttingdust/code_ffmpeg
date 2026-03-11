#include "XDemuxTask.h"
#include "AVLog.h"
#include <chrono>

XDemuxTask::XDemuxTask()
{
    setName("DemuxTask");
    LOGD("XDemuxTask 创建");
}

XDemuxTask::~XDemuxTask()
{
    LOGD("XDemuxTask 销毁");
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

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!packet_queue_.empty())
        {
            packet_queue_.pop();
        }
    }

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

std::unique_ptr<PacketWrapper> XDemuxTask::getPacket()
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (packet_queue_.empty())
    {
        return nullptr;
    }

    auto pkt = std::move(packet_queue_.front());
    packet_queue_.pop();
    return pkt;
}

auto XDemuxTask::getPacketBlocking(int timeout_ms) -> std::unique_ptr<PacketWrapper>
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    auto predicate = [this]() { return !packet_queue_.empty() || eof_reached_; };

    if (timeout_ms > 0)
    {
        if (!queue_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), predicate))
        {
            return nullptr;
        }
    }
    else
    {
        queue_cv_.wait(lock, predicate);
    }

    if (packet_queue_.empty())
    {
        return nullptr;
    }

    auto pkt = std::move(packet_queue_.front());
    packet_queue_.pop();
    return pkt;
}

size_t XDemuxTask::getQueueSize() const
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return packet_queue_.size();
}

bool XDemuxTask::seek(double timestamp, int stream_index)
{
    if (!demuxer_)
        return false;

    bool ret = demuxer_->seek(timestamp, stream_index, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    if (ret)
    {
        LOGI("定位到 " << timestamp << " 秒");

        // 清空队列
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            while (!packet_queue_.empty())
            {
                packet_queue_.pop();
            }
        }

        eof_reached_ = false;
    }

    return ret;
}

auto XDemuxTask::setMaxQueueSize(size_t size) -> void
{
    max_queue_size_ = size;
}

void XDemuxTask::clearQueue()
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!packet_queue_.empty())
    {
        packet_queue_.pop();
    }
    LOGD("清空队列");
}

void XDemuxTask::run()
{
    LOGI("解封装线程开始运行");


    eof_reached_ = false;

    while (!shouldStop())
    {
        /// 检查队列是否已满
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (packet_queue_.size() >= max_queue_size_)
            {
                lock.unlock();
                sleep(10);
                continue;
            }
        }

        PacketWrapper pkt; /// 创建一个新的包

        /// 读取数据包
        int ret = demuxer_->readPacket(pkt);

        if (ret == AVERROR_EOF)
        {
            LOGI("文件读取完成，共 " << total_packets_ << " 个包");
            eof_reached_ = true;
            queue_cv_.notify_all();
            break;
        }
        else if (ret < 0)
        {
            LOGE("读取数据包错误，错误码: " << ret);
            eof_reached_ = true;
            queue_cv_.notify_all();
            break;
        }

        ++total_packets_;

        /// 统计
        if (AVStream* stream = demuxer_->getStream(pkt->stream_index))
        {
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                ++video_packets_;
            }
            else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                ++audio_packets_;
            }
        }

        /// 使用移动语义加入队列
        {
            std::scoped_lock lock(queue_mutex_);
            auto new_pkt = std::make_unique<PacketWrapper>(std::move(pkt));
            packet_queue_.push(std::move(new_pkt));
        }

        queue_cv_.notify_one();

        /// pkt 已经通过移动语义转移了所有权，需要重新创建
        /// 但 PacketWrapper 在移动后处于有效但未指定状态，可以直接继续使用
        /// 不需要 pkt = PacketWrapper();
    }

    LOGI("解封装线程结束，视频包: " << video_packets_ << ", 音频包: " << audio_packets_);
}
