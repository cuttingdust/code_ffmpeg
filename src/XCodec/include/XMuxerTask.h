#pragma once

#include "XTask.h"
#include "Muxer.h"

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
    /// \param enc_ctx 编码器上下文
    /// \param time_base 时间基
    /// \param frame_rate 帧率
    /// \return 成功返回true
    bool init(const std::string& filename, AVCodecContext* enc_ctx, AVRational time_base, AVRational frame_rate);

    /// 关闭封装器
    void close();

    /// 是否正在录制
    bool isRecording() const
    {
        return muxer_ != nullptr;
    }

    /// 获取已写入包数
    int getPacketCount() const
    {
        return packet_count_;
    }

    // ==================== 任务重置 ====================

    void reset() override;

protected:
    void process() override;

private:
    std::unique_ptr<Muxer> muxer_;
    std::string            filename_;
    AVRational             time_base_{ 0, 0 };
    AVRational             frame_rate_{ 0, 0 };
    std::atomic<int>       packet_count_{ 0 };
    int64_t                start_pts_   = AV_NOPTS_VALUE;
    int64_t                frame_index_ = 0;
};
