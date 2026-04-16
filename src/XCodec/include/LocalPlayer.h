#pragma once

#include "XCodec_Global.h"
#include "XDemuxTask.h"
#include "XDecodeTask.h"
#include "XDisplayTask.h"
#include <string>
#include <memory>
#include <atomic>
#include <thread>

class XCODEC_EXPORT LocalPlayer
{
public:
    LocalPlayer();
    ~LocalPlayer();

    // ========== 基本控制 ==========

    /// 打开录像文件
    /// @param filepath 文件路径
    /// @param winId 显示窗口句柄（传 nullptr 则自己创建窗口）
    bool open(const std::string& filepath, void* winId = nullptr);

    /// 开始播放
    void play();

    /// 暂停播放
    void pause();

    /// 恢复播放
    void resume();

    /// 停止播放
    void stop();

    // ========== 状态查询 ==========

    /// 是否正在播放
    bool isPlaying() const
    {
        return is_playing_;
    }

    /// 是否已暂停
    bool isPaused() const
    {
        return is_paused_;
    }

    /// 是否播放结束
    bool isFinished() const
    {
        return is_finished_;
    }

    // ========== 播放控制 ==========

    /// 跳转到指定位置（秒）
    void seek(double seconds);

    /// 设置播放速度（1.0 = 正常速度）
    void setSpeed(double speed);

    // ========== 信息获取 ==========

    /// 获取总时长（秒）
    double getDuration() const;

    /// 获取当前播放位置（秒）
    double getCurrentTime() const;

    /// 获取视频宽度
    int getWidth() const
    {
        return video_width_;
    }

    /// 获取视频高度
    int getHeight() const
    {
        return video_height_;
    }

private:
    void controlLoop();

private:
    XDemuxTask::Ptr   demux_task_;
    XDecodeTask::Ptr  decode_task_;
    XDisplayTask::Ptr display_task_;

    std::string filepath_;
    void*       window_       = nullptr;
    double      duration_     = 0.0;
    double      frame_rate_   = 25.0;
    int         video_width_  = 0;
    int         video_height_ = 0;

    std::atomic<bool>   is_playing_{ false };
    std::atomic<bool>   is_paused_{ false };
    std::atomic<bool>   is_finished_{ false };
    std::atomic<bool>   should_stop_{ false };
    std::atomic<bool>   seek_request_{ false };
    std::atomic<double> seek_target_{ 0.0 };
    std::atomic<double> speed_{ 1.0 };

    std::thread control_thread_;
};
