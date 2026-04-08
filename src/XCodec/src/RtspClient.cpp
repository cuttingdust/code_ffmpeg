#include "RtspClient.h"
#include "AVLog.h"

RtspClient::RtspClient()
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
    // 基类纯虚函数实现，直接调用 createTasks
    createTasks();
}

void RtspClient::createTasks()
{
    // 创建基础任务
    demux_task_   = XDemuxTask::create();
    decode_task_  = XDecodeTask::create();
    display_task_ = XDisplayTask::create();

    // 设置超时
    demux_task_->setIdleTimeoutMs(5000);
    decode_task_->setIdleTimeoutMs(3000);
    display_task_->setIdleTimeoutMs(3000);

    // 设置任务链
    demux_task_->setNext(decode_task_);
    decode_task_->setNext(display_task_);

    // 设置错误回调
    auto error_cb = [this](const std::string& msg) { handleError(msg); };

    demux_task_->setErrorCallback(error_cb);
    decode_task_->setErrorCallback(error_cb);
    display_task_->setErrorCallback(error_cb);

    // 设置窗口
    if (external_win_)
    {
        display_task_->setWindow(external_win_);
    }
}

void RtspClient::destroyTasks()
{
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
    record_enabled_ = true;
}

void RtspClient::startTasks()
{
    XMediaClient::startTasks();
    if (display_task_)
        display_task_->start();
    if (record_task_)
        record_task_->start();
}

void RtspClient::stopTasks()
{
    XMediaClient::stopTasks();
    if (display_task_)
        display_task_->stop();
    if (record_task_)
        record_task_->stop();
}

void RtspClient::resetTasks()
{
    XMediaClient::resetTasks();
    if (display_task_)
        display_task_->reset();
    if (record_task_)
        record_task_->reset();
}

void RtspClient::reconnectImpl()
{
    LOGI("RtspClient 重连实现 - 重新创建任务");

    /// 保存当前窗口句柄（因为 destroyTasks 后会被清空）
    void* saved_win = external_win_;

    /// 1. 彻底释放旧任务
    destroyTasks();

    /// 2. 重新创建任务
    createTasks();

    /// 3. 重新设置窗口句柄（关键！）
    if (saved_win && display_task_)
    {
        display_task_->setWindow(saved_win);
        LOGI("重连后重新设置窗口: " << saved_win);
    }

    /// 4. 如果之前启用了录制，重新创建录制任务
    if (record_enabled_)
    {
        record_task_ = XRecordTask::create();
        demux_task_->addObserver(record_task_);
    }

    /// 5. 重新打开 URL（带重试）
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

    // 6. 重新获取视频流
    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("重连未找到视频流");
        setState(MediaClientState::ERROR);
        return;
    }

    /// 7. 重新初始化解码器
    if (!initDecoder())
    {
        LOGE("重连初始化解码器失败");
        setState(MediaClientState::ERROR);
        return;
    }

    /// 8. 重新启动任务
    startTasks();

    setState(MediaClientState::CONNECTED);
    LOGI("RtspClient 重连成功");
}

bool RtspClient::start()
{
    LOGI("RTSP客户端启动..." << (use_hardware_ ? "(硬件解码)" : "(软件解码)"));
    setState(MediaClientState::CONNECTING);

    /// 打开解封装
    if (!demux_task_->open(url_))
    {
        LOGE("打开RTSP失败: " << url_);
        setState(MediaClientState::ERROR);
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    // 获取视频流
    video_stream_ = demux_task_->getVideoStream();
    if (!video_stream_)
    {
        LOGE("未找到视频流");
        setState(MediaClientState::ERROR);
        return false;
    }

    // 初始化解码器
    if (!initDecoder())
    {
        LOGE("初始化解码器失败");

        // 如果硬件解码失败，尝试软件解码
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

    // 启动所有任务
    startTasks();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (demux_task_->isRunning() && decode_task_->isRunning() && display_task_->isRunning())
    {
        setState(MediaClientState::CONNECTED);
        LOGI("RTSP客户端启动成功");
        return true;
    }
    else
    {
        LOGE("任务启动失败");
        setState(MediaClientState::ERROR);
        return false;
    }
}

void RtspClient::stop()
{
    LOGI("RTSP客户端停止");
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
    if (record_task_)
        record_task_->wait();
    LOGI("RTSP客户端已停止");
}

void RtspClient::setRenderWindow(void* winId)
{
    external_win_ = winId;
    if (display_task_)
    {
        display_task_->setWindow(winId);
    }
}

void RtspClient::setRenderCallback(XDisplayTask::RenderCallback cb)
{
    if (display_task_)
    {
        display_task_->setRenderCallback(std::move(cb));
    }
}

void RtspClient::setFirstFrameCallback(XDisplayTask::FirstFrameCallback cb)
{
    if (display_task_)
    {
        display_task_->setFirstFrameCallback(std::move(cb));
    }
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

auto RtspClient::getDecodeTask() -> XDecodeTask::Ptr
{
    return decode_task_;
}

auto RtspClient::getDisplayTask() -> XDisplayTask::Ptr
{
    return display_task_;
}
