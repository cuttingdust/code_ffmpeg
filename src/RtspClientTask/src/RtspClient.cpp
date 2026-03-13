#include "RtspClient.h"
#include "AVLog.h"

RtspClient::RtspClient()
{
    LOGI("RTSP客户端创建");

    /// 创建任务链
    demux_task_   = XDemuxTask::create();
    decode_task_  = XDecodeTask::create();
    display_task_ = XDisplayTask::create();

    /// 搭建责任链
    demux_task_->setNext(decode_task_);
    decode_task_->setNext(display_task_);

    /// 设置错误回调
    auto error_cb = [this](const std::string& msg)
    {
        LOGE("任务错误，触发重连: " << msg);
        reconnect();
    };

    demux_task_->setErrorCallback(error_cb);
    decode_task_->setErrorCallback(error_cb);
    display_task_->setErrorCallback(error_cb);
}

RtspClient::~RtspClient()
{
    LOGI("RTSP客户端销毁");
    stop();
    wait();
}

auto RtspClient::start() -> void
{
    if (!demux_task_->open(url_))
    {
        LOGE("打开RTSP失败: " << url_);
        state_ = RtspState::ERROR;
        return;
    }

    /// 设置RTSP选项
    demux_task_->setRtspOptions(true, 5000); /// TCP, 5秒超时

    /// 从解封装任务获取流信息，初始化解码器
    auto video_stream = demux_task_->getVideoStream();
    if (!video_stream)
    {
        LOGE("未找到视频流");
        state_ = RtspState::ERROR;
        return;
    }

    if (!decode_task_->initDecoder(video_stream->codecpar->codec_id, video_stream))
    {
        LOGE("初始化解码器失败");
        state_ = RtspState::ERROR;
        return;
    }

    /// 启动所有任务
    demux_task_->start();
    decode_task_->start();
    display_task_->start();

    state_ = RtspState::CONNECTED;
    LOGI("RTSP客户端启动成功");
}

auto RtspClient::stop() -> void
{
    demux_task_->stop();
    decode_task_->stop();
    display_task_->stop();
}

auto RtspClient::wait() -> void
{
    demux_task_->wait();
    decode_task_->wait();
    display_task_->wait();

    state_ = RtspState::DISCONNECTED;
    LOGI("RTSP客户端已停止");
}

void RtspClient::setRenderCallback(XDisplayTask::RenderCallback cb)
{
    if (display_task_)
    {
        display_task_->setRenderCallback(cb);
    }
}

VideoDecoder* RtspClient::getDecoder()
{
    return decode_task_ ? decode_task_->getDecoder() : nullptr;
}

void RtspClient::reconnect()
{
    if (max_reconnects_ > 0 && reconnect_count_ >= max_reconnects_)
    {
        LOGE("达到最大重连次数，停止重连");
        state_ = RtspState::ERROR;
        return;
    }

    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_time_).count();

    // 退避算法
    int wait_time = reconnect_interval_ * (reconnect_count_ + 1);
    if (elapsed < wait_time)
    {
        int wait_ms = (wait_time - elapsed) * 1000;
        LOGI("等待 " << wait_ms << "ms 后重连...");
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }

    LOGI("尝试重连，第 " << (reconnect_count_ + 1) << " 次");
    reconnect_count_++;
    last_reconnect_time_ = std::chrono::steady_clock::now();

    // 停止当前任务
    stop();

    // 重新启动
    start();
}
