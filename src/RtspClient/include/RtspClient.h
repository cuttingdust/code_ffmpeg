#pragma once

#include "XThread.h"
#include "Demuxer.h"
#include "VideoDecoder.h"
#include "PacketWrapper.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

/// RTSP 客户端状态
enum class RtspState
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

/// 断开连接的原因
enum class DisconnectReason
{
    NONE,
    NORMAL_EOF,    /// 正常播放结束
    NETWORK_ERROR, /// 网络错误
    SERVER_CLOSED, /// 服务器关闭
    MANUAL         /// 手动断开
};

/// ==================== RTSP 客户端类 ====================
class RtspClient : public XThread
{
public:
    RtspClient();
    ~RtspClient() override;

public:
    /// 设置 RTSP URL
    void setUrl(const std::string& url)
    {
        url_ = url;
    }

    /// 获取当前状态
    RtspState getState() const
    {
        return state_;
    }

    /// 获取视频流
    AVStream* getVideoStream() const
    {
        return video_stream_;
    }

    /// 获取解码器
    VideoDecoder* getDecoder() const
    {
        return decoder_.get();
    }

    /// 获取队列大小
    size_t getQueueSize() const;

    /// 获取下一个数据包（阻塞）
    std::unique_ptr<PacketWrapper> getPacketBlocking(int timeout_ms = 1000);

    /// 设置重连间隔（秒）
    void setReconnectInterval(int seconds)
    {
        reconnect_interval_ = seconds;
    }

    /// 设置最大重连次数（0表示无限）
    void setMaxReconnects(int count)
    {
        max_reconnects_ = count;
    }

    /// 手动触发重连
    void forceReconnect();

    /// 检查是否正常结束
    bool isNormalEOF() const
    {
        return disconnect_reason_ == DisconnectReason::NORMAL_EOF;
    }

protected:
    /// 线程主函数
    void run() override;

private:
    /// 连接 RTSP
    bool connect();

    /// 断开连接
    void disconnect(DisconnectReason reason = DisconnectReason::MANUAL);

    /// 读取数据包循环
    void readLoop();

    /// 处理连接失败
    void handleConnectionFailure();

private:
    std::string            url_;
    std::atomic<RtspState> state_{ RtspState::DISCONNECTED };
    DisconnectReason       disconnect_reason_{ DisconnectReason::NONE };

    /// 解封装和解码
    std::unique_ptr<Demuxer> demuxer_;
    VideoDecoder::Ptr        decoder_;
    AVStream*                video_stream_ = nullptr;

    /// 数据包队列
    std::queue<std::unique_ptr<PacketWrapper>> packet_queue_;
    mutable std::mutex                         queue_mutex_;
    std::condition_variable                    queue_cv_;
    std::atomic<bool>                          eof_reached_{ false };

    /// 统计
    std::atomic<int64_t> total_packets_{ 0 };
    std::atomic<int64_t> video_packets_{ 0 };

    /// 重连配置
    int                                   reconnect_interval_ = 3;
    int                                   max_reconnects_     = 0;
    int                                   reconnect_count_    = 0;
    std::chrono::steady_clock::time_point last_reconnect_time_;
};
