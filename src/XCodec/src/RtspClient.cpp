#include "RtspClient.h"
#include "AVLog.h"
#include "XRecordingOverlay.h"
#include "XOverlayUtil.h"
#include "XOpenGLDisplay.h"
#include "XOpenGLVideoWidget.h"
#include "FrameWrapper.h"
#include "XAudioDecodeTask.h"
#include "XAudioPlayTask.h"

RtspClient::RtspClient() : overlay_style_(defaultRecOverlayStyle())
{
    LOGI("RTSP客户端创建");
    createTasks();
}

RtspClient::~RtspClient()
{
    LOGI("RTSP客户端销毁 - 开始");
    stop();
    wait();
    destroyTasks();
    LOGI("RTSP客户端销毁 - 结束");
}

auto RtspClient::create() -> std::shared_ptr<RtspClient>
{
    return std::make_shared<RtspClient>();
}

void RtspClient::initTasks()
{
    createTasks();
}

void RtspClient::createTasks()
{
    audio_ready_      = false;
    audio_enabled_    = false;
    audio_suspended_  = false;
    demux_task_   = XDemuxTask::create();
    decode_task_  = XVideoDecodeTask::create();
    display_task_ = XVideoDisplayTask::create();
    audio_decode_task_ = XAudioDecodeTask::create();
    audio_play_task_   = XAudioPlayTask::create();

    demux_task_->setIdleTimeoutMs(5000);
    decode_task_->setIdleTimeoutMs(3000);
    display_task_->setIdleTimeoutMs(10000);
    audio_decode_task_->setIdleTimeoutMs(3000);
    audio_play_task_->setIdleTimeoutMs(10000);

    demux_task_->setNext(decode_task_);
    decode_task_->setNext(display_task_);
    /// 音频链按需 enableAudio() 时再 setAudioNext

    auto error_cb = [this](const std::string& msg) { handleError(msg); };

    demux_task_->setErrorCallback(error_cb);
    decode_task_->setErrorCallback(error_cb);
    display_task_->setErrorCallback(error_cb);
    audio_decode_task_->setErrorCallback(error_cb);
    audio_play_task_->setErrorCallback(error_cb);

    audio_decode_task_->setNext(audio_play_task_);

    applyDisplayRender();
}

auto RtspClient::initAudio() -> bool
{
    audio_ready_ = false;

    if (!demux_task_)
    {
        return false;
    }

    auto* audio_stream = demux_task_->getAudioStream();
    if (!audio_stream)
    {
        LOGI("RTSP 无音频流");
        return true;
    }

    if (!audio_decode_task_ || !audio_play_task_)
    {
        LOGE("音频 Task 未创建");
        return false;
    }

    if (!audio_decode_task_->initDecoder(audio_stream))
    {
        LOGE("RTSP 音频解码器初始化失败");
        return false;
    }

    if (!audio_play_task_->openFromDecoder(audio_decode_task_->getDecoder()))
    {
        LOGE("RTSP 音频播放设备打开失败");
        return false;
    }

    audio_play_task_->setVolume(volume_);
    if (demux_task_)
    {
        demux_task_->setAudioNext(audio_decode_task_);
    }
    audio_suspended_ = false;
    audio_ready_       = true;
    LOGI("RTSP 音频: " << audio_stream->codecpar->sample_rate << "Hz");
    return true;
}

void RtspClient::applyDisplayRender()
{
    if (!display_task_)
    {
        return;
    }

    if (custom_render_cb_)
    {
        display_task_->setRenderCallback(custom_render_cb_);
        return;
    }

    if (render_backend_ == RenderBackend::OpenGL && opengl_widget_)
    {
        bindOpenGLDisplayTask(display_task_.get(), opengl_widget_);
        return;
    }

    bindSdlDisplayTask(display_task_.get(), external_win_);
}

void RtspClient::destroyTasks()
{
    audio_ready_     = false;
    audio_suspended_ = false;
    if (audio_play_task_)
    {
        audio_play_task_->reset();
        audio_play_task_.reset();
    }
    if (audio_decode_task_)
    {
        audio_decode_task_->reset();
        audio_decode_task_.reset();
    }
    if (display_task_)
    {
        display_task_->stop();
        display_task_->wait();
        display_task_.reset();
    }
    if (decode_task_)
    {
        decode_task_->stop();
        decode_task_->wait();
        decode_task_.reset();
    }
    if (demux_task_)
    {
        demux_task_->stop();
        demux_task_->wait();
        demux_task_.reset();
    }
    if (record_task_)
    {
        record_task_->stop();
        record_task_->wait();
        record_task_.reset();
    }
}

void RtspClient::enableRecord()
{
    if (record_enabled_)
        return;

    LOGI("启用录制功能");
    record_task_ = XRecordTask::create();
    demux_task_->addObserver(record_task_);
    record_task_->start();
    record_enabled_ = true;
}

void RtspClient::startTasks()
{
    if (record_task_)
        record_task_->start();
    if (audio_ready_)
    {
        if (audio_play_task_)
            audio_play_task_->start();
        if (audio_decode_task_)
            audio_decode_task_->start();
    }
    XMediaClient::startTasks();
    if (display_task_)
        display_task_->start();
}

void RtspClient::stopTasks()
{
    XMediaClient::stopTasks();
    if (display_task_)
        display_task_->stop();
    if (audio_decode_task_)
        audio_decode_task_->stop();
    if (audio_play_task_)
        audio_play_task_->stop();
    if (record_task_)
        record_task_->stop();
}

void RtspClient::resetTasks()
{
    XMediaClient::resetTasks();
    if (display_task_)
        display_task_->reset();
    if (audio_decode_task_)
        audio_decode_task_->reset();
    if (audio_play_task_)
        audio_play_task_->reset();
    if (record_task_)
        record_task_->reset();
}

void RtspClient::reconnectImpl()
{
    LOGI("RtspClient 重连实现 - 重新创建任务");

    void* saved_win = external_win_;
    auto  saved_backend = render_backend_;
    auto* saved_widget  = opengl_widget_;

    destroyTasks();
    createTasks();

    render_backend_ = saved_backend;
    opengl_widget_  = saved_widget;
    external_win_   = saved_win;
    applyDisplayRender();
    applyOverlayStyle(overlay_style_, display_task_.get(), opengl_widget_);

    if (saved_win && render_backend_ == RenderBackend::SDL)
    {
        LOGI("重连后重新设置窗口: " << saved_win);
    }

    if (record_enabled_)
    {
        record_task_ = XRecordTask::create();
        demux_task_->addObserver(record_task_);
    }

    const int max_retries = 5;
    bool      opened      = false;

    for (int i = 0; i < max_retries; i++)
    {
        if (demux_task_->open(url_))
        {
            opened = true;
            break;
        }

        LOGE("重连打开URL失败 (尝试 " << i + 1 << "/" << max_retries << "): " << url_);

        if (i < max_retries - 1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    if (!opened)
    {
        LOGE("重连打开URL最终失败: " << url_);
        setState(MediaClientState::ERROR);
        return;
    }

    demux_task_->setRtspOptions(true, 5000);

    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("重连未找到视频流");
        setState(MediaClientState::ERROR);
        return;
    }

    if (!initDecoder())
    {
        LOGE("重连初始化解码器失败");
        setState(MediaClientState::ERROR);
        return;
    }

    if (audio_enabled_)
    {
        if (!initAudio())
        {
            LOGW("重连音频初始化失败，继续仅视频播放");
        }
    }

    startTasks();

    setState(MediaClientState::CONNECTED);
    LOGI("RtspClient 重连成功");
}

bool RtspClient::start()
{
    LOGI("RTSP客户端启动..." << (use_hardware_ ? "(硬件解码)" : "(软件解码)"));
    setState(MediaClientState::CONNECTING);

    if (!demux_task_->open(url_))
    {
        LOGE("打开RTSP失败: " << url_);
        setState(MediaClientState::ERROR);
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("未找到视频流");
        setState(MediaClientState::ERROR);
        return false;
    }

    if (!initDecoder())
    {
        LOGE("初始化解码器失败");

        if (use_hardware_)
        {
            LOGE("硬件解码失败，尝试软件解码");
            use_hardware_ = false;

            if (!initDecoder())
            {
                LOGE("软件解码也失败");
                setState(MediaClientState::ERROR);
                return false;
            }
        }
        else
        {
            setState(MediaClientState::ERROR);
            return false;
        }
    }

    startTasks();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const bool video_running =
        demux_task_->isRunning() && decode_task_->isRunning() && display_task_->isRunning();

    if (video_running)
    {
        setState(MediaClientState::CONNECTED);
        LOGI("RTSP客户端启动成功");
        return true;
    }

    LOGE("任务启动失败");
    setState(MediaClientState::ERROR);
    return false;
}

void RtspClient::stop()
{
    LOGI("RTSP客户端停止");
    abortReconnect();
    setState(MediaClientState::DISCONNECTED);
    stopTasks();
}

void RtspClient::wait()
{
    if (demux_task_)
        demux_task_->wait();
    if (decode_task_)
        decode_task_->wait();
    if (display_task_)
        display_task_->wait();
    if (audio_decode_task_)
        audio_decode_task_->wait();
    if (audio_play_task_)
        audio_play_task_->wait();
    if (record_task_)
        record_task_->wait();
    LOGI("RTSP客户端已停止");
}

void RtspClient::setRenderWindow(void* winId)
{
    external_win_ = winId;
    applyDisplayRender();
}

void RtspClient::setOpenGLWidget(XOpenGLVideoWidget* widget)
{
    opengl_widget_ = widget;
    if (opengl_widget_)
    {
        opengl_widget_->setOverlayStyle(overlay_style_);
    }
    applyDisplayRender();
}

void RtspClient::setRenderBackend(RenderBackend backend)
{
    render_backend_ = backend;
    applyDisplayRender();
}

void RtspClient::setOverlayStyle(const XOverlayStyle& style)
{
    overlay_style_ = style;
    applyOverlayStyle(style, display_task_.get(), opengl_widget_);
}

void RtspClient::setRenderCallback(XVideoDisplayTask::RenderCallback cb)
{
    custom_render_cb_ = std::move(cb);
    applyDisplayRender();
}

void RtspClient::setFirstFrameCallback(XVideoDisplayTask::FirstFrameCallback cb)
{
    if (display_task_)
    {
        display_task_->setFirstFrameCallback(std::move(cb));
    }
}

void RtspClient::setRecordingIndicator(bool show)
{
    const bool use_gl  = render_backend_ == RenderBackend::OpenGL;
    const bool use_sdl = !use_gl;
    applyRecordingIndicator(show, display_task_.get(), opengl_widget_, use_sdl, use_gl);
}

bool RtspClient::startRecording(const std::string& filename, int duration_sec)
{
    if (!record_enabled_)
    {
        enableRecord();
    }

    if (!record_task_)
    {
        LOGE("录制任务未初始化");
        return false;
    }

    if (!video_stream_)
    {
        LOGE("视频流未获取");
        return false;
    }

    return record_task_->beginRecord(filename, video_stream_, duration_sec);
}

void RtspClient::stopRecording()
{
    if (record_task_)
    {
        record_task_->endRecord();
    }
}

bool RtspClient::isRecording() const
{
    return record_task_ ? record_task_->isRecording() : false;
}

auto RtspClient::getRecordingStatus() const -> XRecordTask::Status
{
    if (record_task_)
    {
        return record_task_->getStatus();
    }
    return XRecordTask::Status{};
}

auto RtspClient::getDecoder() -> VideoDecoder*
{
    return decode_task_ ? decode_task_->getDecoder() : nullptr;
}

auto RtspClient::getDemuxTask() -> XDemuxTask::Ptr
{
    return demux_task_;
}

auto RtspClient::getDecodeTask() -> XVideoDecodeTask::Ptr
{
    return decode_task_;
}

auto RtspClient::getDisplayTask() -> XVideoDisplayTask::Ptr
{
    return display_task_;
}

auto RtspClient::hasAudio() const -> bool
{
    return demux_task_ && demux_task_->getAudioStream() != nullptr;
}

auto RtspClient::enableAudio() -> bool
{
    if (!demux_task_ || !demux_task_->getAudioStream())
    {
        return false;
    }

    if (audio_ready_ && !audio_suspended_)
    {
        audio_enabled_ = true;
        if (audio_play_task_)
        {
            audio_play_task_->setVolume(volume_);
        }
        return true;
    }

    if (!initAudio())
    {
        return false;
    }

    audio_enabled_ = true;

    if (isRunning())
    {
        if (audio_play_task_)
        {
            audio_play_task_->start();
        }
        if (audio_decode_task_)
        {
            audio_decode_task_->start();
        }
    }

    LOGI("RTSP 预览音频已开启");
    return true;
}

auto RtspClient::disableAudio() -> void
{
    audio_enabled_ = false;
    volume_        = 0.0;
    pauseAudio();
    LOGI("RTSP 预览音频已关闭");
}

auto RtspClient::setVolume(double volume) -> void
{
    if (volume < 0.0)
    {
        volume = 0.0;
    }
    else if (volume > 1.0)
    {
        volume = 1.0;
    }

    volume_ = volume;
    if (volume > 0.0 && !audio_ready_)
    {
        enableAudio();
    }
    if (audio_play_task_)
    {
        audio_play_task_->setVolume(volume);
    }
}

auto RtspClient::getVolume() const -> double
{
    return volume_;
}

auto RtspClient::pauseAudio() -> void
{
    if (!audio_ready_ || audio_suspended_)
    {
        return;
    }

    /// 断开 Demux 音频分叉，避免解码队列塞满阻塞视频读包
    if (demux_task_)
    {
        demux_task_->setAudioNext(nullptr);
    }

    if (audio_play_task_)
    {
        audio_play_task_->flushDownstream();
        audio_play_task_->setPaused(true);
    }
    if (audio_decode_task_)
    {
        audio_decode_task_->setPaused(true);
    }

    audio_suspended_ = true;
    LOGI("RTSP 音频已暂停（连接保持）");
}

auto RtspClient::resumeAudio() -> void
{
    if (!audio_enabled_ || !audio_ready_ || !audio_suspended_)
    {
        return;
    }

    if (demux_task_ && audio_decode_task_)
    {
        demux_task_->setAudioNext(audio_decode_task_);
    }

    if (audio_decode_task_)
    {
        audio_decode_task_->setPaused(false);
    }
    if (audio_play_task_)
    {
        audio_play_task_->setPaused(false);
        audio_play_task_->setVolume(volume_);
    }

    audio_suspended_ = false;
    LOGI("RTSP 音频已恢复");
}
