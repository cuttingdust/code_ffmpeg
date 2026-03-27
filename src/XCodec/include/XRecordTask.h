#pragma once

#include "XTask.h"
#include "Muxer.h"
#include <chrono>

/**
 * @brief 录制任务类
 * 
 * 作为观察者接收数据包，直接写入文件
 * 支持：
 * - 指定录制时长
 * - 自动关键帧检测
 * - 自动停止
 */
class XRecordTask : public XTask
{
    DECLARE_CREATE(XRecordTask)

public:
    /// 录制状态信息
    struct Status
    {
        bool        is_recording = false; ///< 是否正在录制
        int         packet_count = 0;     ///< 已写入包数
        int         recorded_sec = 0;     ///< 已录制时长（秒）
        int         total_sec    = 0;     ///< 总录制时长（秒）
        std::string filename;             ///< 输出文件名
    };

    XRecordTask();
    ~XRecordTask() override;

    // ==================== 录制控制 ====================

    /// \brief 开始录制
    /// \param filename 输出文件名
    /// \param video_stream 视频流信息
    /// \param duration_sec 录制时长（秒），0表示无限录制
    /// \return 成功返回true
    auto beginRecord(const std::string& filename, AVStream* video_stream, int duration_sec = 0) -> bool;

    /// \brief 结束录制
    void endRecord();

    /// \brief 是否正在录制
    bool isRecording() const
    {
        return muxer_ != nullptr;
    }

    /// \brief 获取已写入包数
    int getPacketCount() const
    {
        return packet_count_;
    }

    /// \brief 获取录制状态
    Status getStatus() const;

    // ==================== 数据接收 ====================

    /// \brief 接收数据包（由观察者调用）
    void feedPacket(PacketWrapper::Ptr pkt);

    // ==================== 任务重置 ====================

    void reset() override;

protected:
    void process() override;

private:
    std::unique_ptr<Muxer>                muxer_;
    AVStream*                             video_stream_ = nullptr;
    int64_t                               start_pts_    = AV_NOPTS_VALUE;
    int64_t                               end_pts_      = AV_NOPTS_VALUE;
    AVRational                            time_base_{ 0, 0 };
    std::atomic<int>                      packet_count_{ 0 };
    int                                   duration_sec_ = 0;
    std::string                           filename_;
    std::chrono::steady_clock::time_point start_time_;
    bool                                  need_stop_ = false;
};
