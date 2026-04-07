#pragma once
#include "XCodec_Global.h"
#include "XDemuxTask.h"
#include "XDecodeTask.h"
#include "XDisplayTask.h"
#include "XRecordTask.h"

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
class XCODEC_EXPORT RtspClient
{
public:
    RtspClient();
    ~RtspClient();

public:
    /// 设置 RTSP URL
    auto setUrl(const std::string &url) -> void;

    /// 获取当前状态
    auto getState() const -> RtspState;

    /// 开始播放
    auto start() -> bool;

    /// 停止播放
    auto stop() -> void;

    /// 等待所有线程结束
    auto wait() -> void;

    /// 检查是否正在运行
    auto isRunning() const -> bool;

    /// 设置重连间隔（秒）
    auto setReconnectInterval(int seconds) -> void;

    /// 设置最大重连次数（0表示无限）
    auto set_max_reconnects(int count) -> void;

    /// 设置自定义渲染回调
    auto setRenderCallback(XDisplayTask::RenderCallback cb) -> void;

    //////////////////////////////////////////////////////////////////

    auto startRecording(const std::string &filename, int duration_sec = 0) -> bool;
    auto stopRecording() -> void;
    auto isRecording() const -> bool;
    auto getRecordingStatus() const -> XRecordTask::Status;

    //////////////////////////////////////////////////////////////////

    /// 获取解码器（用于高级配置）
    auto getDecoder() -> VideoDecoder *;

    /// 获取解封装任务（用于高级配置）
    auto getDemuxTask() -> XDemuxTask::Ptr;

    /// 获取解码任务（用于高级配置）
    auto getDecodeTask() -> XDecodeTask::Ptr;

    /// 获取显示任务（用于高级配置）
    auto getDisplayTask() -> XDisplayTask::Ptr;

    auto setFirstFrameCallback(XDisplayTask::FirstFrameCallback cb) -> void;

private:
    /// 重连逻辑
    void reconnect();

private:
    std::string            url_;
    std::atomic<RtspState> state_{ RtspState::DISCONNECTED };

    /// 任务链
    XDemuxTask::Ptr   demux_task_;
    XDecodeTask::Ptr  decode_task_;
    XDisplayTask::Ptr display_task_;
    XRecordTask::Ptr  record_task_;

    /// 重连配置
    int                                   reconnect_interval_ = 3;
    int                                   max_reconnects_     = 0;
    int                                   reconnect_count_    = 0;
    std::chrono::steady_clock::time_point last_reconnect_time_;

    std::atomic<bool> use_hardware_{ true }; /// 是否使用硬件解码
};
