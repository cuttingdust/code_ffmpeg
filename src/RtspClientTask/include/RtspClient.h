#pragma once

#include "XDemuxTask.h"
#include "XDecodeTask.h"
#include "XDisplayTask.h"
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

/// ==================== RTSP 客户端类 ====================
class RtspClient
{
public:
    RtspClient();
    ~RtspClient();

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

    /// 开始播放
    auto start() -> void;

    /// 停止播放
    auto stop() -> void;

    /// 等待所有线程结束
    void wait();

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

    /// 设置自定义渲染回调
    void setRenderCallback(XDisplayTask::RenderCallback cb);

    /// 获取解码器（用于高级配置）
    VideoDecoder* getDecoder();

    /// 获取解封装任务（用于高级配置）
    auto getDemuxTask() -> XDemuxTask::Ptr
    {
        return demux_task_;
    }

    /// 获取解码任务（用于高级配置）
    auto getDecodeTask() -> XDecodeTask::Ptr
    {
        return decode_task_;
    }

    /// 获取显示任务（用于高级配置）
    auto getDisplayTask() -> XDisplayTask::Ptr
    {
        return display_task_;
    }

private:
    /// 重连逻辑
    void reconnect();

private:
    std::string            url_;
    std::atomic<RtspState> state_{ RtspState::DISCONNECTED };

    /// 任务链
    XDemuxTask::Ptr   demux_task_   = nullptr;
    XDecodeTask::Ptr  decode_task_  = nullptr;
    XDisplayTask::Ptr display_task_ = nullptr;

    /// 重连配置
    int                                   reconnect_interval_ = 3;
    int                                   max_reconnects_     = 0;
    int                                   reconnect_count_    = 0;
    std::chrono::steady_clock::time_point last_reconnect_time_;
};
