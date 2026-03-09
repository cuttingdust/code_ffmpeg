#include "Muxer.h"
#include "AVException.h"
#include <iostream>
#include <utility>

Muxer::Muxer(const std::string& url) : BaseAVFormat(url)
{
}

Muxer::~Muxer()
{
    close();
}

auto Muxer::open() -> bool
{
    try
    {
        fmt_ctx_ = FormatContextWrapper::createOutput(url_);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "创建输出文件失败: " << e.what() << std::endl;
        return false;
    }
}

void Muxer::close()
{
    if (fmt_ctx_ && header_written_)
    {
        writeTrailer();
    }
    fmt_ctx_.reset();
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
    header_written_     = false;
}

auto Muxer::addVideoStream(const AVStream* in_stream) -> int
{
    if (!fmt_ctx_ || !in_stream)
    {
        return -1;
    }

    AVStream* out_stream = fmt_ctx_->addStream();
    if (!out_stream)
    {
        return -1;
    }

    avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    out_stream->time_base = in_stream->time_base;

    video_stream_index_ = out_stream->index;
    return video_stream_index_;
}

auto Muxer::addAudioStream(AVStream* in_stream) -> int
{
    if (!fmt_ctx_ || !in_stream)
    {
        return -1;
    }

    AVStream* out_stream = fmt_ctx_->addStream();
    if (!out_stream)
    {
        return -1;
    }

    avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
    out_stream->time_base = in_stream->time_base;

    audio_stream_index_ = out_stream->index;
    return audio_stream_index_;
}

auto Muxer::addStream(const AVCodecParameters* codecpar, AVRational time_base) -> int
{
    if (!fmt_ctx_ || !codecpar)
        return -1;

    AVStream* out_stream = fmt_ctx_->addStream();
    if (!out_stream)
        return -1;

    avcodec_parameters_copy(out_stream->codecpar, codecpar);
    out_stream->time_base = time_base;

    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        video_stream_index_ = out_stream->index;
    }
    else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        audio_stream_index_ = out_stream->index;
    }

    return out_stream->index;
}

auto Muxer::addStream(const AVCodecContext* enc_ctx, AVRational time_base) -> int
{
    if (!fmt_ctx_ || !enc_ctx)
    {
        return -1;
    }

    AVStream* out_stream = fmt_ctx_->addStream();
    if (!out_stream)
    {
        return -1;
    }

    /// 设置时间基
    out_stream->time_base = time_base;

    /// 使用 FFmpeg 提供的函数复制参数
    int ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0)
    {
        std::cerr << "复制编码参数失败" << std::endl;
        return -1;
    }

    /// 设置流索引
    if (enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        video_stream_index_ = out_stream->index;
    }

    return out_stream->index;
}

auto Muxer::writeHeader() -> int
{
    if (!fmt_ctx_)
    {
        return -1;
    }

    int ret = avformat_write_header(fmt_ctx_->get(), nullptr);
    if (ret >= 0)
    {
        header_written_ = true;
    }
    return ret;
}

auto Muxer::writeTrailer() -> int
{
    if (!fmt_ctx_ || !header_written_)
        return -1;

    int ret         = av_write_trailer(fmt_ctx_->get());
    header_written_ = false;
    return ret;
}

auto Muxer::writePacket(AVPacket* pkt, AVStream* in_stream, int64_t pts_offset) -> int
{
    if (!fmt_ctx_ || !pkt || !in_stream || !header_written_)
        return -1;

    AVFormatContext* ctx        = fmt_ctx_->get();
    AVStream*        out_stream = nullptr;

    for (unsigned int i = 0; i < ctx->nb_streams; i++)
    {
        if (ctx->streams[i]->codecpar->codec_id == in_stream->codecpar->codec_id)
        {
            out_stream = ctx->streams[i];
            break;
        }
    }

    if (!out_stream)
        return -1;

    return writePacket(pkt, in_stream->index, out_stream->index, in_stream->time_base, pts_offset);
}

auto Muxer::writePacket(AVPacket* pkt, int in_stream_index, int out_stream_index, AVRational in_time_base,
                        int64_t pts_offset) -> int
{
    if (!fmt_ctx_ || !pkt || !header_written_)
    {
        return -1;
    }

    AVFormatContext* ctx = fmt_ctx_->get();

    if (out_stream_index < 0 || std::cmp_greater_equal(out_stream_index, ctx->nb_streams))
    {
        return -1;
    }

    AVRational out_time_base = ctx->streams[out_stream_index]->time_base;

    if (pkt->pts != AV_NOPTS_VALUE)
    {
        pkt->pts = av_rescale_q_rnd(pkt->pts - pts_offset, in_time_base, out_time_base,
                                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    }

    if (pkt->dts != AV_NOPTS_VALUE)
    {
        pkt->dts = av_rescale_q_rnd(pkt->dts - pts_offset, in_time_base, out_time_base,
                                    static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
    }

    pkt->duration     = av_rescale_q(pkt->duration, in_time_base, out_time_base);
    pkt->stream_index = out_stream_index;
    pkt->pos          = -1;

    return av_interleaved_write_frame(ctx, pkt);
}

auto Muxer::getContext() const -> AVFormatContext*
{
    return fmt_ctx_ ? fmt_ctx_->get() : nullptr;
}

void Muxer::dumpInfo() const
{
    if (!fmt_ctx_)
    {
        return;
    }
    fmt_ctx_->dumpInfo(0, url_.c_str(), 1);
}
