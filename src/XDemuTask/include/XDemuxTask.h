#pragma once

#include "XThread.h"
#include "Demuxer.h"
#include "PacketWrapper.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

/// 数据包队列类型
using PacketQueue = std::queue<std::unique_ptr<PacketWrapper>>;

/// ==================== 解封装任务类 ====================
class XDemuxTask : public XThread
{
public:
    XDemuxTask();
    ~XDemuxTask() override;

public:
    /// 设置输入文件
    auto open(const std::string& url) -> bool;

    /// 关闭
    auto close() -> void;

    /// 获取视频流
    auto getVideoStream() const -> AVStream*;

    /// 获取音频流
    auto getAudioStream() const -> AVStream*;

    /// 获取流信息
    auto getStreams() const -> const std::vector<AVStream*>&;

    /// 获取编码参数
    auto getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>;

    /// 获取下一个数据包（非阻塞）
    std::unique_ptr<PacketWrapper> getPacket();

    /// 获取下一个数据包（阻塞）消费
    std::unique_ptr<PacketWrapper> getPacketBlocking(int timeout_ms = 1000);

    /// 获取队列大小
    size_t getQueueSize() const;

    /// 定位到指定时间（秒）
    bool seek(double timestamp, int stream_index = -1);

    /// 获取文件总时长
    double getDuration() const
    {
        return demuxer_ ? demuxer_->getDuration() : 0.0;
    }

    /// 获取文件名
    std::string getFilename() const
    {
        return url_;
    }

    /// 设置队列最大大小
    auto setMaxQueueSize(size_t size) -> void;

    /// 清空队列
    void clearQueue();

protected:
    /// 线程主函数
    void run() override;

private:
    Demuxer::Ptr demuxer_;
    std::string  url_;

    AVStream*              video_stream_ = nullptr;
    AVStream*              audio_stream_ = nullptr;
    std::vector<AVStream*> streams_;

    /// 数据包队列
    PacketQueue             packet_queue_;
    mutable std::mutex      queue_mutex_;
    std::condition_variable queue_cv_;

    /// 队列控制
    size_t            max_queue_size_ = 100;
    std::atomic<bool> eof_reached_{ false };

    /// 统计
    std::atomic<int64_t> total_packets_{ 0 };
    std::atomic<int64_t> video_packets_{ 0 };
    std::atomic<int64_t> audio_packets_{ 0 };
};
