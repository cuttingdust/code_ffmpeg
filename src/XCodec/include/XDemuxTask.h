#pragma once

#include "XTask.h"
#include "Demuxer.h"
#include <atomic>

class XDemuxTask : public XTask
{
    DECLARE_CREATE(XDemuxTask)
public:
    XDemuxTask();
    ~XDemuxTask() override;

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

    /// 获取文件总时长
    auto getDuration() const -> double;

    /// 获取文件名
    auto getFilename() const -> std::string;

    /// 定位到指定时间（秒）
    auto seek(double timestamp, int stream_index = -1) -> bool;

    /// 设置RTSP选项
    auto setRtspOptions(bool use_tcp, int timeout_ms) -> void;

    /// 获取统计信息
    struct Stats
    {
        int64_t total_packets = 0;
        int64_t video_packets = 0;
        int64_t audio_packets = 0;
    };
    auto getStats() const -> Stats;

    /// 重置任务
    auto reset() -> void override;

    /// 设置播放速度
    void setSpeed(double speed)
    {
        speed_ = speed;
    }

    /// 获取当前播放速度
    double getSpeed() const
    {
        return speed_.load();
    }

    void setPaused(bool paused) override;

protected:
    auto process() -> void override;

    void resetFrameTime();

private:
    Demuxer::Ptr demuxer_;
    std::string  url_;

    AVStream*              video_stream_ = nullptr;
    AVStream*              audio_stream_ = nullptr;
    std::vector<AVStream*> streams_;

    /// 统计信息
    std::atomic<int64_t> total_packets_{ 0 };
    std::atomic<int64_t> video_packets_{ 0 };
    std::atomic<int64_t> audio_packets_{ 0 };

    /// 倍速控制
    std::atomic<double>                   speed_{ 1.0 };
    std::chrono::steady_clock::time_point next_frame_time_;
};
