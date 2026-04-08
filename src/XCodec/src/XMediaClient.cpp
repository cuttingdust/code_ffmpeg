#include "XMediaClient.h"
#include "AVLog.h"

XMediaClient::XMediaClient() = default;

void XMediaClient::setUrl(const std::string& url)
{
    url_ = url;
}

void XMediaClient::startTasks()
{
    if (demux_task_)
        demux_task_->start();
    if (decode_task_)
        decode_task_->start();
}

void XMediaClient::stopTasks()
{
    if (demux_task_)
        demux_task_->stop();
    if (decode_task_)
        decode_task_->stop();
}

void XMediaClient::resetTasks()
{
    if (demux_task_)
        demux_task_->reset();
    if (decode_task_)
        decode_task_->reset();
}

AVStream* XMediaClient::getVideoStream() const
{
    return demux_task_ ? demux_task_->getVideoStream() : nullptr;
}

bool XMediaClient::initDecoder()
{
    if (!decode_task_ || !video_stream_)
    {
        LOGE("initDecoder: 解码器或视频流为空");
        return false;
    }

    decode_task_->setHardwareDecode(use_hardware_);
    return decode_task_->initDecoder(video_stream_->codecpar->codec_id, video_stream_);
}

void XMediaClient::handleError(const std::string& msg)
{
    LOGE("客户端错误: " << msg);
    if (error_cb_)
    {
        error_cb_(msg);
    }
    reconnect();
}

void XMediaClient::reconnect()
{
    if (reconnecting_.exchange(true))
    {
        LOGW("重连已在进行中，忽略本次请求");
        return;
    }

    if (max_reconnects_ > 0 && reconnect_count_ >= max_reconnects_)
    {
        LOGE("达到最大重连次数(" << max_reconnects_ << ")，停止重连");
        state_        = MediaClientState::ERROR;
        reconnecting_ = false;
        return;
    }

    reconnect_count_++;
    LOGI("===== 开始第 " << reconnect_count_ << " 次重连 =====");
    last_reconnect_time_ = std::chrono::steady_clock::now();

    std::thread(
            [this]()
            {
                /// 停止所有任务
                stopTasks();

                if (demux_task_)
                    demux_task_->wait();
                if (decode_task_)
                    decode_task_->wait();

                /// 重置任务状态
                resetTasks();

                /// 调用子类的重连实现
                reconnectImpl();

                /// 如果重连失败，且还有重连次数，继续重试
                if (state_ == MediaClientState::ERROR && (max_reconnects_ == 0 || reconnect_count_ < max_reconnects_))
                {
                    LOGI("重连失败，等待 " << reconnect_interval_ << " 秒后再次尝试");
                    std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval_));
                    reconnecting_ = false;
                    reconnect(); /// 继续重试
                }
                else
                {
                    reconnecting_ = false;
                }
            })
            .detach();
}
