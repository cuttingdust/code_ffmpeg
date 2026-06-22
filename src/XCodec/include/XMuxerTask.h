#pragma once

#include "XTask.h"
#include "Muxer.h"
#include <atomic>
#include <queue>

/// 封装任务类
class XMuxerTask : public XTask
{
    DECLARE_CREATE(XMuxerTask)

public:
    XMuxerTask();
    ~XMuxerTask() override;

    // ==================== 封装器控制 ====================

    /// 初始化封装器
    /// \param filename 输出文件名
    /// \param enc_ctx 视频编码器上下文
    /// \param time_base 时间基
    /// \param frame_rate 帧率
    /// \param audio_stream 输入音频流（可选，remux 直通）
    /// \return 成功返回true
    bool init(const std::string& filename, AVCodecContext* enc_ctx, AVRational time_base, AVRational frame_rate,
              AVStream* audio_stream = nullptr);

    /// 关闭封装器
    void close();

    /// 是否正在录制
    bool isRecording() const
    {
        return muxer_ != nullptr;
    }

    /// 是否包含音频轨
    bool hasAudio() const
    {
        return has_audio_;
    }

    /// 获取已写入包数
    int getPacketCount() const
    {
        return packet_count_;
    }

    bool hasKeyFrameWritten() const
    {
        return video_started_.load();
    }

    /// 由音频支路推送压缩包（线程安全）
    void pushAudioPacket(PacketWrapper::Ptr pkt);

    // ==================== 任务重置 ====================

    void reset() override;

protected:
    void process() override;

private:
    void clearPendingAudio();
    void drainAudioQueue();
    void writeAudioPacket(PacketWrapper::Ptr pkt);
    void onVideoKeyFrameReady();
    int64_t previewOutputDts(const AVPacket* pkt) const;

    std::unique_ptr<Muxer> muxer_;
    std::string            filename_;
    AVRational             time_base_{ 0, 0 };
    AVRational             frame_rate_{ 0, 0 };
    std::atomic<int>       packet_count_{ 0 };
    int64_t                start_pts_   = AV_NOPTS_VALUE;
    int64_t                frame_index_ = 0;

    bool       has_audio_              = false;
    int        in_audio_stream_index_  = -1;
    int        out_audio_stream_index_ = -1;
    AVRational in_audio_time_base_{ 0, 0 };
    int64_t    audio_pts_offset_ = AV_NOPTS_VALUE;
    int64_t    last_audio_dts_   = AV_NOPTS_VALUE;
    int        audio_packet_count_ = 0;
    std::atomic<bool> video_started_{ false };

    std::queue<PacketWrapper::Ptr> audio_queue_;
    std::mutex                     audio_queue_mutex_;
};
