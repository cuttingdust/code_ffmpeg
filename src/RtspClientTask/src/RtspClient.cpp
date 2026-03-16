#include "RtspClient.h"
#include "AVLog.h"

RtspClient::RtspClient()
{
    LOGI("RTSP客户端创建");

    demux_task_   = XDemuxTask::create();
    decode_task_  = XDecodeTask::create();
    display_task_ = XDisplayTask::create();

    demux_task_->setIdleTimeoutMs(5000);
    decode_task_->setIdleTimeoutMs(3000);
    display_task_->setIdleTimeoutMs(3000);

    demux_task_->setNext(decode_task_);
    decode_task_->setNext(display_task_);

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
    LOGI("RTSP客户端销毁 - 开始");

    // 1. 先停止所有任务
    LOGI("停止所有任务...");
    if (demux_task_)
        demux_task_->stop();
    if (decode_task_)
        decode_task_->stop();
    if (display_task_)
        display_task_->stop();

    // 2. 等待一小段时间让任务停止
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 3. 等待任务结束
    LOGI("等待任务结束...");
    if (demux_task_)
        demux_task_->wait();
    if (decode_task_)
        decode_task_->wait();
    if (display_task_)
        display_task_->wait();

    LOGI("所有任务已停止");

    // 4. 最后重置智能指针（会触发析构）
    demux_task_.reset();
    decode_task_.reset();
    display_task_.reset();

    LOGI("RTSP客户端销毁 - 结束");
}

auto RtspClient::start() -> bool
{
    LOGI("RTSP客户端启动..." << (use_hardware_ ? "(硬件解码)" : "(软件解码)"));

    if (!demux_task_->open(url_))
    {
        LOGE("打开RTSP失败: " << url_);
        state_ = RtspState::ERROR;
        return false;
    }

    demux_task_->setRtspOptions(true, 5000);

    auto video_stream = demux_task_->getVideoStream();
    if (!video_stream)
    {
        LOGE("未找到视频流");
        state_ = RtspState::ERROR;
        return false;
    }

    if (!decode_task_->initDecoder(video_stream->codecpar->codec_id, video_stream))
    {
        LOGE("初始化解码器失败");

        if (use_hardware_)
        {
            LOGE("硬件解码失败，尝试软件解码");
            use_hardware_ = false;

            if (!decode_task_->initDecoder(video_stream->codecpar->codec_id, video_stream))
            {
                LOGE("软件解码也失败");
                state_ = RtspState::ERROR;
                return false;
            }
        }
        else
        {
            state_ = RtspState::ERROR;
            return false;
        }
    }

    demux_task_->start();
    decode_task_->start();
    display_task_->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (demux_task_->isRunning() && decode_task_->isRunning() && display_task_->isRunning())
    {
        state_ = RtspState::CONNECTED;
        LOGI("RTSP客户端启动成功");
        return true;
    }
    else
    {
        LOGE("任务启动失败");
        state_ = RtspState::ERROR;
        return false;
    }
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
    static std::atomic<bool> reconnecting{ false };

    if (reconnecting.exchange(true))
    {
        LOGW("重连已在进行中，忽略本次请求");
        return;
    }

    // 检查是否达到最大重连次数
    if (max_reconnects_ > 0 && reconnect_count_ >= max_reconnects_)
    {
        LOGE("达到最大重连次数(" << max_reconnects_ << ")，停止重连");
        state_       = RtspState::ERROR;
        reconnecting = false;
        return;
    }

    reconnect_count_++;
    LOGI("===== 开始第 " << reconnect_count_ << " 次重连 =====");
    last_reconnect_time_ = std::chrono::steady_clock::now();

    std::thread(
            [this]()
            {
                // 停止所有任务
                LOGI("停止所有任务...");
                demux_task_->stop();
                decode_task_->stop();
                display_task_->stop();

                demux_task_->wait();
                decode_task_->wait();
                display_task_->wait();

                LOGI("所有任务已停止，开始重置...");
                demux_task_->reset();
                decode_task_->reset();
                display_task_->reset();

                LOGI("重置完成，开始重新启动...");

                // ✅ 在这里循环重试，不增加 reconnect_count_
                const int max_start_retries = 5;
                bool      started           = false;

                for (int i = 0; i < max_start_retries; i++)
                {
                    LOGI("启动尝试 " << i + 1 << "/" << max_start_retries);

                    if (start())
                    {
                        started = true;
                        LOGI("===== 第 " << reconnect_count_ << " 次重连成功！ =====");
                        break;
                    }

                    if (i < max_start_retries - 1)
                    {
                        int retry_wait = (i + 1) * 1000;
                        LOGI("启动失败，等待 " << retry_wait << "ms 后重试...");
                        std::this_thread::sleep_for(std::chrono::milliseconds(retry_wait));
                    }
                }

                if (!started)
                {
                    LOGE("第 " << reconnect_count_ << " 次重连失败，将在 " << reconnect_interval_ << " 秒后再次尝试");

                    // 等待重连间隔
                    std::this_thread::sleep_for(std::chrono::seconds(reconnect_interval_));

                    // 再次触发重连（会增加 reconnect_count_）
                    reconnecting = false;

                    if (max_reconnects_ == 0 || reconnect_count_ < max_reconnects_)
                    {
                        reconnect(); // 再次尝试，会增加计数
                    }
                }
                else
                {
                    reconnecting = false;
                }
            })
            .detach();
}
