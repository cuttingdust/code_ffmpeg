#pragma once
#include "XCodec_Global.h"
#include "XMediaClient.h"
#include "XDisplayTask.h"
#include "XRecordTask.h"

/// RTSP 播放客户端
class XCODEC_EXPORT RtspClient : public XMediaClient
{
public:
    static auto create() -> std::shared_ptr<RtspClient>;
    explicit RtspClient();
    ~RtspClient() override;

    // ==================== 基本控制 ====================

    bool start() override;
    void stop() override;
    void wait() override;

    // ==================== 显示控制 ====================

    void setRenderWindow(void* winId);
    void setRenderCallback(XDisplayTask::RenderCallback cb);
    void setFirstFrameCallback(XDisplayTask::FirstFrameCallback cb);

    // ==================== 录制控制（按需启用）====================

    void enableRecord();
    bool startRecording(const std::string& filename, int duration_sec = 0);
    void stopRecording();
    bool isRecording() const;
    auto getRecordingStatus() const -> XRecordTask::Status;

    // ==================== 高级配置 ====================

    auto getDecoder() -> VideoDecoder*;
    auto getDemuxTask() -> XDemuxTask::Ptr;
    auto getDecodeTask() -> XDecodeTask::Ptr;
    auto getDisplayTask() -> XDisplayTask::Ptr;

    /// 设置录制指示器显示状态
    void setRecordingIndicator(bool show);

protected:
    void initTasks() override;
    void startTasks() override;
    void stopTasks() override;
    void resetTasks() override;
    void reconnectImpl() override;

private:
    void destroyTasks();
    void createTasks();

private:
    XDisplayTask::Ptr display_task_;
    XRecordTask::Ptr  record_task_;
    bool              record_enabled_ = false;
    void*             external_win_   = nullptr;
};
