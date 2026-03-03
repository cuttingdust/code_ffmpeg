#include "Demuxer.h"
#include "AVException.h"
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

Demuxer::Demuxer(const std::string &url) : url_(url)
{
}

Demuxer::~Demuxer() = default;

auto Demuxer::open() -> bool
{
    try
    {
        fmt_ctx_ = FormatContextWrapper::createInput(url_);

        if (fmt_ctx_->findStreamInfo() < 0)
        {
            return false;
        }

        // 查找视频流和音频流
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

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "打开文件失败: " << e.what() << std::endl;
        return false;
    }
}

void Demuxer::close()
{
    fmt_ctx_.reset();
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
}

auto Demuxer::getVideoStream() const -> AVStream *
{
    if (!fmt_ctx_ || video_stream_index_ < 0)
        return nullptr;
    return fmt_ctx_->get()->streams[video_stream_index_];
}

auto Demuxer::getAudioStream() const -> AVStream *
{
    if (!fmt_ctx_ || audio_stream_index_ < 0)
        return nullptr;
    return fmt_ctx_->get()->streams[audio_stream_index_];
}

auto Demuxer::getStream(int index) const -> AVStream *
{
    if (!fmt_ctx_)
        return nullptr;
    AVFormatContext *ctx = fmt_ctx_->get();
    if (index >= 0 && index < static_cast<int>(ctx->nb_streams))
    {
        return ctx->streams[index];
    }
    return nullptr;
}

auto Demuxer::getStreams() const -> std::vector<AVStream *>
{
    std::vector<AVStream *> streams;
    if (!fmt_ctx_)
        return streams;

    AVFormatContext *ctx = fmt_ctx_->get();
    for (unsigned int i = 0; i < ctx->nb_streams; i++)
    {
        streams.push_back(ctx->streams[i]);
    }
    return streams;
}

auto Demuxer::getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>
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

auto Demuxer::readPacket(AVPacket *pkt) -> int
{
    if (!fmt_ctx_)
        return -1;
    return av_read_frame(fmt_ctx_->get(), pkt);
}

auto Demuxer::seek(double timestamp, int stream_index, int flags) -> bool
{
    if (!fmt_ctx_)
        return false;

    AVFormatContext *ctx     = fmt_ctx_->get();
    int64_t          seek_ts = timestamp * AV_TIME_BASE;

    if (stream_index >= 0 && stream_index < static_cast<int>(ctx->nb_streams))
    {
        AVStream *stream = ctx->streams[stream_index];
        seek_ts          = av_rescale_q(timestamp * AV_TIME_BASE, AV_TIME_BASE_Q, stream->time_base);
    }

    return av_seek_frame(ctx, stream_index, seek_ts, flags) >= 0;
}

auto Demuxer::getDuration() const -> double
{
    if (!fmt_ctx_)
        return 0.0;
    return fmt_ctx_->getDuration();
}

auto Demuxer::getFilename() const -> std::string
{
    return url_;
}

void Demuxer::dumpInfo() const
{
    if (!fmt_ctx_)
        return;
    fmt_ctx_->dumpInfo(0, url_.c_str(), 0);
}

auto Demuxer::getRawContext() const -> AVFormatContext *
{
    return fmt_ctx_ ? fmt_ctx_->get() : nullptr;
}
