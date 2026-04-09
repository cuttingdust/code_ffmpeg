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

    /// 开始分段录制
    /// \param prefix 文件前缀（会自动添加序号和时间）
    /// \param segment_sec 每段时长（秒）
    /// \param total_sec 总录制时长（秒），0表示无限
    /// \return 成功返回true
    bool startSegmentRecording(const std::string& prefix, int segment_sec, int total_sec = 0);

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
    /// 打开 URL 并获取视频流
    bool openUrlAndGetStream();

    /// 初始化编码器和封装器
    bool initEncoderAndMuxer(const std::string& output_file);

    /// 重置所有任务并重新打开
    bool resetAndReopen(const std::string& new_output_file);

    /// 时长监控线程
    void durationMonitorThread();

    /// 分段监控线程
    void segmentMonitorThread();

    /// 切换文件
    void switchSegment();

    /// 生成分段文件名
    auto generateSegmentFilename() -> std::string;

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

    /// 分段录制相关
    int               segment_duration_ = 0;
    std::string       segment_prefix_;
    int               segment_index_ = 1;
    std::thread       segment_thread_;
    std::atomic<bool> segment_monitor_running_{ false };
    int               total_segment_sec_ = 0;

    /// 原始流帧率（避免修改 encode_config_）
    AVRational src_framerate_{ 25, 1 };
};
