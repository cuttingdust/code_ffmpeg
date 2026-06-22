#pragma once
#include "XCodec_Global.h"
#include "XMediaClient.h"
#include "XVideoDisplayTask.h"
#include "XAudioDecodeTask.h"
#include "XAudioPlayTask.h"
#include "XRecordTask.h"
#include "XRenderBackend.h"
#include "XOverlayStyle.h"

class XOpenGLVideoWidget;

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
    void setOpenGLWidget(XOpenGLVideoWidget* widget);
    void setRenderBackend(RenderBackend backend);
    RenderBackend renderBackend() const
    {
        return render_backend_;
    }

    void setOverlayStyle(const XOverlayStyle& style);
    XOverlayStyle overlayStyle() const
    {
        return overlay_style_;
    }

    void setRenderCallback(XVideoDisplayTask::RenderCallback cb);
    void setFirstFrameCallback(XVideoDisplayTask::FirstFrameCallback cb);

    // ==================== 录制控制（按需启用）====================

    void enableRecord();
    bool startRecording(const std::string& filename, int duration_sec = 0);
    void stopRecording();
    bool isRecording() const;
    auto getRecordingStatus() const -> XRecordTask::Status;

    // ==================== 高级配置 ====================

    auto getDecoder() -> VideoDecoder*;
    auto getDemuxTask() -> XDemuxTask::Ptr;
    auto getDecodeTask() -> XVideoDecodeTask::Ptr;
    auto getDisplayTask() -> XVideoDisplayTask::Ptr;

    /// 设置录制指示器显示状态
    void setRecordingIndicator(bool show);

    // ==================== 音频控制 ====================

    auto hasAudio() const -> bool;
    auto setVolume(double volume) -> void;
    auto getVolume() const -> double;

    /// 按需开启音频（预览默认仅视频，用户开声时再 init）
    auto enableAudio() -> bool;

    /// 关闭预览音频
    auto disableAudio() -> void;

    /// 暂停预览音频（保持 RTSP / SDL 设备，供回放独占出声）
    auto pauseAudio() -> void;

    /// 恢复预览音频（pauseAudio 之后）
    auto resumeAudio() -> void;

protected:
    void initTasks() override;
    void startTasks() override;
    void stopTasks() override;
    void resetTasks() override;
    void reconnectImpl() override;

private:
    void destroyTasks();
    void createTasks();
    void applyDisplayRender();
    auto initAudio() -> bool;

private:
    XVideoDisplayTask::Ptr display_task_;
    XAudioDecodeTask::Ptr  audio_decode_task_;
    XAudioPlayTask::Ptr    audio_play_task_;
    double                 volume_ = 0.0;
    bool                   audio_ready_ = false;
    bool                   audio_enabled_ = false;
    bool                   audio_suspended_ = false;
    XRecordTask::Ptr  record_task_;
    bool              record_enabled_ = false;
    void*             external_win_   = nullptr;
    XOpenGLVideoWidget* opengl_widget_  = nullptr;
    RenderBackend       render_backend_ = RenderBackend::SDL;
    XOverlayStyle       overlay_style_;
    XVideoDisplayTask::RenderCallback custom_render_cb_;
};
