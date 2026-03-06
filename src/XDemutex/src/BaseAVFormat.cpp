#include "BaseAVFormat.h"
#include <iostream>

BaseAVFormat::BaseAVFormat(std::string url) : url_(std::move(url))
{
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
        return streams;

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
