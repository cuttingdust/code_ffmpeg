#pragma once
#include "XCodec_Global.h"
#include "XDemuxTask.h"
#include "XVideoDecodeTask.h"
#include <atomic>
#include <chrono>
#include <functional>

/// 客户端状态
enum class MediaClientState
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    RECONNECTING,
    ERROR
};

/// 媒体客户端基类
class XCODEC_EXPORT XMediaClient
{
public:
    virtual ~XMediaClient() = default;

    // ==================== 基本控制 ====================

    /// 设置 URL
    virtual void setUrl(const std::string& url);

    /// 获取当前状态
    MediaClientState getState() const
    {
        return state_;
    }

    /// 开始播放/录制
    virtual bool start() = 0;

    /// 停止
    virtual void stop() = 0;

    /// 等待结束
    virtual void wait() = 0;

    /// 是否正在运行
    bool isRunning() const
    {
        return state_ == MediaClientState::CONNECTED;
    }

    // ==================== 重连配置 ====================

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

    // ==================== 硬件解码配置 ====================

    /// 设置是否使用硬件解码
    void setHardwareDecode(bool enable)
    {
        use_hardware_ = enable;
    }

    /// 是否使用硬件解码
    bool isHardwareDecode() const
    {
        return use_hardware_;
    }

    // ==================== 回调设置 ====================

    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback cb)
    {
        error_cb_ = std::move(cb);
    }

protected:
    XMediaClient();

    /// 初始化任务链（由子类实现）
    virtual void initTasks() = 0;

    /// 启动所有任务
    virtual void startTasks();

    /// 停止所有任务
    virtual void stopTasks();

    /// 重置所有任务
    virtual void resetTasks();

    /// 重连逻辑（子类实现具体重连）
    virtual void reconnectImpl() = 0;

    /// 重连入口
    void reconnect();

    /// 获取视频流
    AVStream* getVideoStream() const;

    /// 设置状态
    void setState(MediaClientState state)
    {
        state_ = state;
    }

    /// 处理错误
    void handleError(const std::string& msg);

    /// 初始化解码器
    bool initDecoder();

protected:
    std::string                   url_;
    std::atomic<MediaClientState> state_{ MediaClientState::DISCONNECTED };

    /// 任务链（所有客户端都需要的）
    XDemuxTask::Ptr  demux_task_;
    XVideoDecodeTask::Ptr decode_task_;

    /// 流信息
    AVStream* video_stream_ = nullptr;

    /// 重连配置
    int                                   reconnect_interval_ = 3;
    int                                   max_reconnects_     = 0;
    int                                   reconnect_count_    = 0;
    std::chrono::steady_clock::time_point last_reconnect_time_;
    std::atomic<bool>                     reconnecting_{ false };

    /// 错误回调
    ErrorCallback error_cb_;

    /// 硬件解码开关
    std::atomic<bool> use_hardware_{ true };
};
