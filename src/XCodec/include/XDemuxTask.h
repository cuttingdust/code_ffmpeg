#pragma once

#include "XTask.h"
#include "Demuxer.h"

class XDemuxTask : public XTask
{
    DECLARE_CREATE(XDemuxTask)
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

    /// 获取文件总时长
    auto getDuration() const -> double
    {
        return demuxer_ ? demuxer_->getDuration() : 0.0;
    }

    /// 获取文件名
    auto getFilename() const -> std::string
    {
        return url_;
    }

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

protected:
    auto process() -> void override;

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
    std::atomic<double>  speed_{ 1.0 }; ///<  播放速度
};
