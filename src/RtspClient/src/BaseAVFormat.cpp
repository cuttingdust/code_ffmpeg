#include "BaseAVFormat.h"

auto BaseAVFormat::isTimeout() const -> bool
{
    if (timeout_ms_ <= 0)
    {
        return false;
    }


    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_time_).count();

    return elapsed > timeout_ms_;
}

auto BaseAVFormat::resetTimer() -> void
{
    last_activity_time_ = std::chrono::steady_clock::now();
    start_time_         = last_activity_time_;
}

BaseAVFormat::BaseAVFormat(std::string url) : url_(std::move(url))
{
    resetTimer();
}

auto BaseAVFormat::setTimeout(int timeout_ms) -> void
{
    timeout_ms_ = timeout_ms;
}

auto BaseAVFormat::getTimeout() const -> int
{
    return timeout_ms_;
}

auto BaseAVFormat::setTransport(const std::string &transport) -> void
{
    transport_ = transport;
}

auto BaseAVFormat::setBufferSize(int size) -> void
{
    buffer_size_ = size;
}

auto BaseAVFormat::setRtspOptions(bool use_tcp, int timeout_ms) -> void
{
    timeout_ms_    = timeout_ms;
    transport_     = use_tcp ? "tcp" : "udp";
    options_dirty_ = true;

    LOGI("设置 RTSP 选项: transport=" << transport_ << ", timeout=" << timeout_ms_ << "ms");
}

auto BaseAVFormat::getVideoStream() const -> AVStream *
{
    if (!fmt_ctx_ || video_stream_index_ < 0)
    {
        return nullptr;
    }
    return fmt_ctx_->get()->streams[video_stream_index_];
}

auto BaseAVFormat::getAudioStream() const -> AVStream *
{
    if (!fmt_ctx_ || audio_stream_index_ < 0)
    {
        return nullptr;
    }
    return fmt_ctx_->get()->streams[audio_stream_index_];
}

auto BaseAVFormat::getStream(int index) const -> AVStream *
{
    if (!fmt_ctx_)
    {
        return nullptr;
    }

    AVFormatContext *ctx = fmt_ctx_->get();
    if (index >= 0 && std::cmp_less(index, ctx->nb_streams))
    {
        return ctx->streams[index];
    }
    return nullptr;
}

auto BaseAVFormat::getStreams() const -> std::vector<AVStream *>
{
    std::vector<AVStream *> streams;
    if (!fmt_ctx_)
    {
        return streams;
    }


    AVFormatContext *ctx = fmt_ctx_->get();
    for (unsigned int i = 0; i < ctx->nb_streams; i++)
    {
        streams.push_back(ctx->streams[i]);
    }
    return streams;
}

auto BaseAVFormat::getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>
{
    if (!fmt_ctx_)
        return nullptr;

    AVFormatContext *ctx = fmt_ctx_->get();
    if (stream_index < 0 || stream_index >= static_cast<int>(ctx->nb_streams))
    {
        return nullptr;
    }

    auto params = std::make_shared<CodecParametersWrapper>();
    params->copy_from(ctx->streams[stream_index]->codecpar);
    return params;
}

auto BaseAVFormat::getFilename() const -> std::string
{
    return url_;
}

auto BaseAVFormat::getRawContext() const -> AVFormatContext *
{
    return fmt_ctx_ ? fmt_ctx_->get() : nullptr;
}

void BaseAVFormat::findStreamIndices()
{
    if (!fmt_ctx_)
    {
        return;
    }

    AVFormatContext *ctx = fmt_ctx_->get();
    for (unsigned int i = 0; i < ctx->nb_streams; i++)
    {
        AVStream *stream = ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index_ == -1)
        {
            video_stream_index_ = i;
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index_ == -1)
        {
            audio_stream_index_ = i;
        }
    }
}

auto BaseAVFormat::getOptionsPtr() -> AVDictionary **
{
    if (!options_dirty_)
    {
        return options_.get_ptr();
    }

    options_.clear();

    if (isNetworkProtocol())
    {
        /// 通用选项：传输协议和超时
        options_.set("rtsp_transport", transport_);
        options_.set("timeout", std::to_string(timeout_ms_ * 1000));

        if (transport_ == "udp")
        {
            /// UDP 专用配置
            options_.set("stimeout", std::to_string(timeout_ms_ * 1000)); /// UDP读取超时
            options_.set("buffer_size", "2048000");                       /// 2MB 缓冲区
            options_.set("udp_buffer_size", "2048000");                   /// UDP 缓冲区
            options_.set("max_delay", "500000");                          /// 500ms 重排序延迟
            options_.set("reorder_queue_size", "1000");                   /// 重排序队列

            LOGI("UDP 模式配置完成");
        }
        else if (transport_ == "tcp")
        {
            /// TCP 专用配置
            options_.set("rtsp_flags", "prefer_tcp"); /// 偏好 TCP
            options_.set("tcp_nodelay", "1");         /// 禁用 Nagle 算法，降低延迟
            options_.set("buffer_size", "102400");    /// 100KB 缓冲区就够了

            LOGI("TCP 模式配置完成");
        }

        options_.print("网络协议选项");
    }

    options_dirty_ = false;
    return options_.get_ptr();
}

auto BaseAVFormat::isNetworkProtocol() const -> bool
{
    return url_.starts_with("rtsp://") || url_.starts_with("rtmp://") || url_.starts_with("http://");
}

auto BaseAVFormat::interruptCallback(void *ctx) -> int
{
    auto *format = static_cast<BaseAVFormat *>(ctx);
    return format->isTimeout() ? 1 : 0;
}
