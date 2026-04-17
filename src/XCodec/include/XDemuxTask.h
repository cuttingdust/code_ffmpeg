#pragma once

#include "XTask.h"
#include "Demuxer.h"
#include <atomic>
#include <chrono>

class XDemuxTask : public XTask
{
    DECLARE_CREATE(XDemuxTask)
public:
    XDemuxTask();
    ~XDemuxTask() override;

    auto open(const std::string& url) -> bool;

    auto close() -> void;

    auto getVideoStream() const -> AVStream*;

    auto getAudioStream() const -> AVStream*;

    auto getStreams() const -> const std::vector<AVStream*>&;

    auto getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>;

    auto getDuration() const -> double;

    auto getFilename() const -> std::string;

    auto seek(double timestamp, int stream_index = -1) -> bool;

    auto setRtspOptions(bool use_tcp, int timeout_ms) -> void;

    struct Stats
    {
        int64_t total_packets = 0;
        int64_t video_packets = 0;
        int64_t audio_packets = 0;
    };

    auto getStats() const -> Stats;

    auto reset() -> void override;

    void setSpeed(double speed)
    {
        speed_ = speed;
    }

    double getSpeed() const
    {
        return speed_.load();
    }

    void setPaused(bool paused) override;

    double getCurrentTime() const
    {
        return current_time_.load();
    }


protected:
    auto process() -> void override;

private:
    void resetFrameTime();

private:
    Demuxer::Ptr demuxer_;
    std::string  url_;

    AVStream*              video_stream_ = nullptr;
    AVStream*              audio_stream_ = nullptr;
    std::vector<AVStream*> streams_;

    std::atomic<int64_t> total_packets_{ 0 };
    std::atomic<int64_t> video_packets_{ 0 };
    std::atomic<int64_t> audio_packets_{ 0 };

    std::atomic<double> speed_{ 1.0 };
    std::atomic<double> current_time_{ 0.0 };

    std::chrono::steady_clock::time_point next_frame_time_;
};
