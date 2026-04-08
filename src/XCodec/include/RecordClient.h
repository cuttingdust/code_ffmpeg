#pragma once
#include "XCodec_Global.h"
#include "XMediaClient.h"
#include "XEncodeTask.h"
#include "XMuxerTask.h"
#include "EncoderConfig.h"

/// 录制客户端
class XCODEC_EXPORT RecordClient : public XMediaClient
{
public:
    static auto create() -> std::shared_ptr<RecordClient>;
    explicit RecordClient();
    ~RecordClient() override;

    // ==================== 基本控制 ====================

    bool start() override;
    void stop() override;
    void wait() override;

    // ==================== 录制控制 ====================

    /// 开始录制
    /// \param output_file 输出文件路径
    /// \param duration_sec 录制时长（秒），0表示手动停止
    /// \return 成功返回true
    bool startRecording(const std::string& output_file, int duration_sec = 0);

    /// 停止录制
    void stopRecording();

    /// 是否正在录制
    bool isRecording() const;

    /// 获取已写入包数
    int getPacketCount() const;

    // ==================== 编码配置 ====================

    void setEncodeConfig(const EncoderConfig& config);


protected:
    void initTasks() override;
    void startTasks() override;
    void stopTasks() override;
    void resetTasks() override;
    void reconnectImpl() override;

private:
    void durationMonitorThread();

private:
    XEncodeTask::Ptr encode_task_;
    XMuxerTask::Ptr  muxer_task_;

    EncoderConfig                         encode_config_;
    std::string                           output_file_;
    int                                   duration_sec_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    std::thread                           duration_thread_;
    std::atomic<bool>                     duration_monitor_running_{ false };
    std::atomic<bool>                     is_recording_{ false };
};
