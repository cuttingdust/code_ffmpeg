#include "RtspClient.h"
#include "AVLog.h"
#include <chrono>

RtspClient::RtspClient()
{
    setName("RtspClient");
    LOGI("RTSP客户端创建");
}

RtspClient::~RtspClient()
{
    LOGI("RTSP客户端销毁");
    stop();
    wait();
    disconnect(DisconnectReason::MANUAL);
}

auto RtspClient::connect() -> bool
{
    LOGI("正在连接 RTSP: " << url_);
    state_             = RtspState::CONNECTING;
    disconnect_reason_ = DisconnectReason::NONE;

    try
    {
        LOGI("创建 demuxer...");
        demuxer_ = Demuxer::create(url_);

        LOGI("设置 RTSP 选项...");
        demuxer_->setRtspOptions(true, 5000);

        LOGI("打开 demuxer...");
        if (!demuxer_->open())
        {
            LOGE("打开 RTSP 失败: " << url_);
            return false;
        }

        video_stream_ = demuxer_->getVideoStream();
        if (!video_stream_)
        {
            LOGE("未找到视频流");
            return false;
        }

        LOGI("创建解码器...");
        DecoderConfig config;
        config.codec_id                      = video_stream_->codecpar->codec_id;
        config.thread_count                  = 16;
        config.hardware.enable               = true;
        config.hardware.auto_select          = true;
        config.hardware.preferred_type       = HardwareContext::Type::D3D11VA;
        config.hardware.transfer_to_software = true;

        decoder_ = VideoDecoder::create(config);

        /// 设置解码器参数
        if (!decoder_->set_parameters_from_stream(video_stream_))
        {
            LOGE("设置解码器参数失败");
            return false;
        }

        /// 打开解码器
        decoder_->open();

        LOGI("连接成功！视频: " << video_stream_->codecpar->width << "x" << video_stream_->codecpar->height);

        state_           = RtspState::CONNECTED;
        reconnect_count_ = 0;
        eof_reached_     = false;
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("连接异常: " << e.what());
        return false;
    }
}

void RtspClient::disconnect(DisconnectReason reason)
{
    LOGI("断开 RTSP 连接，原因: " << (int)reason);

    {
        std::scoped_lock lock(queue_mutex_);
        while (!packet_queue_.empty())
        {
            packet_queue_.pop();
        }
    }

    if (decoder_)
    {
        decoder_->close();
        decoder_.reset();
    }

    if (demuxer_)
    {
        demuxer_->close();
        demuxer_.reset();
    }

    video_stream_ = nullptr;
    eof_reached_  = false;

    disconnect_reason_ = reason;

    if (state_ != RtspState::RECONNECTING)
    {
        state_ = RtspState::DISCONNECTED;
    }
}

size_t RtspClient::getQueueSize() const
{
    std::scoped_lock lock(queue_mutex_);
    return packet_queue_.size();
}

std::unique_ptr<PacketWrapper> RtspClient::getPacketBlocking(int timeout_ms)
{
    std::unique_lock<std::mutex> lock(queue_mutex_);

    auto predicate = [this]() { return !packet_queue_.empty() || eof_reached_ || state_ != RtspState::CONNECTED; };

    if (timeout_ms > 0)
    {
        if (!queue_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), predicate))
        {
            return nullptr;
        }
    }
    else
    {
        queue_cv_.wait(lock, predicate);
    }

    if (packet_queue_.empty())
    {
        return nullptr;
    }

    auto pkt = std::move(packet_queue_.front());
    packet_queue_.pop();
    return pkt;
}

void RtspClient::forceReconnect()
{
    LOGI("手动触发重连");
    if (state_ == RtspState::CONNECTED)
    {
        state_ = RtspState::RECONNECTING;
        disconnect(DisconnectReason::MANUAL);
    }
}

void RtspClient::handleConnectionFailure()
{
    /// 如果已经停止，不再重连
    if (shouldStop())
    {
        return;
    }

    /// 如果是正常结束，不重连
    if (disconnect_reason_ == DisconnectReason::NORMAL_EOF)
    {
        LOGI("正常播放结束，不重连");
        return;
    }

    if (max_reconnects_ > 0 && reconnect_count_ >= max_reconnects_)
    {
        LOGE("达到最大重连次数，停止重连");
        state_ = RtspState::ERROR;
        return;
    }

    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_reconnect_time_).count();

    /// 逐渐增加重连间隔（退避算法）
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

    state_ = RtspState::RECONNECTING;
    disconnect(DisconnectReason::NETWORK_ERROR);
}

void RtspClient::readLoop()
{
    auto      last_data_time   = std::chrono::steady_clock::now();
    int       empty_read_count = 0;
    const int max_empty_reads  = 10;

    while (state_ == RtspState::CONNECTED && !shouldStop())
    {
        /// 检查队列大小
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (packet_queue_.size() > 100)
            {
                lock.unlock();
                sleep(10);
                continue;
            }
        }

        /// 检查是否长时间没有数据
        auto now       = std::chrono::steady_clock::now();
        auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_data_time).count();
        if (idle_time > 10) /// 10秒没有数据
        {
            LOGE("10秒没有收到数据，可能网络异常");
            handleConnectionFailure();
            break;
        }

        PacketWrapper pkt;
        int           ret = demuxer_->readPacket(pkt);

        if (ret == AVERROR_EOF)
        {
            LOGI("RTSP 流正常结束，视频播放完成");
            eof_reached_       = true;
            disconnect_reason_ = DisconnectReason::NORMAL_EOF;
            queue_cv_.notify_all();
            state_ = RtspState::DISCONNECTED;
            break;
        }
        else if (ret < 0)
        {
            // 网络错误，需要重连
            LOGE("读取 RTSP 包错误: " << ret << "，触发重连");
            handleConnectionFailure();
            break;
        }

        // 成功读取到数据包
        last_data_time   = std::chrono::steady_clock::now();
        empty_read_count = 0;
        ++total_packets_;

        if (pkt->stream_index == video_stream_->index)
        {
            ++video_packets_;

            {
                std::scoped_lock lock(queue_mutex_);
                packet_queue_.push(std::make_unique<PacketWrapper>(std::move(pkt)));
            }
            queue_cv_.notify_one();
        }
    }
}

void RtspClient::run()
{
    LOGI("RTSP 客户端线程启动");

    while (!shouldStop())
    {
        switch (state_)
        {
            case RtspState::DISCONNECTED:
                // 如果是正常结束，不再重连
                if (disconnect_reason_ == DisconnectReason::NORMAL_EOF)
                {
                    LOGI("视频正常播放结束，退出线程");
                    return;
                }
                // 否则尝试重连
                // fall through

            case RtspState::RECONNECTING:
                if (connect())
                {
                    readLoop();
                }
                else
                {
                    handleConnectionFailure();
                }
                break;

            case RtspState::CONNECTED:
                readLoop();
                break;

            case RtspState::CONNECTING:
            case RtspState::ERROR:
            default:
                sleep(100);
                break;
        }

        sleep(10);
    }

    LOGI("RTSP 客户端线程结束");
}
