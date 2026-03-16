#include "Demuxer.h"
#include "AVException.h"
#include <iostream>
#include <thread>

static int safe_av_read_frame(AVFormatContext* ctx, AVPacket* pkt)
{
    try
    {
        return av_read_frame(ctx, pkt);
    }
    catch (...)
    {
        LOGE("av_read_frame 崩溃");
        return -1;
    }
}

Demuxer::Demuxer(const std::string& url) : BaseAVFormat(url)
{
}

auto Demuxer::open() -> bool
{
    try
    {
        AVIOInterruptCB interrupt_cb = { BaseAVFormat::interruptCallback, this };

        fmt_ctx_ = FormatContextWrapper::createInput(url_, getOptionsPtr(), &interrupt_cb);

        resetTimer();

        if (fmt_ctx_->findStreamInfo() < 0)
        {
            return false;
        }

        findStreamIndices();
        return true;
    }
    catch (const std::exception& e)
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

auto Demuxer::readPacket(AVPacket* pkt) -> int
{
    if (!fmt_ctx_)
    {
        return -1;
    }

    resetTimer();

    // 使用安全函数
    int ret = safe_av_read_frame(fmt_ctx_->get(), pkt);

    if (ret >= 0)
    {
        resetTimer();
    }

    // 如果是致命错误，标记需要重建
    if (ret == AVERROR(EINVAL) || ret == AVERROR(ENOSYS) || ret < -1000)
    {
        LOGE("致命错误 " << ret << "，需要重建解封装器");
        return -2; // 特殊错误码，表示需要重建
    }

    return ret;
}

auto Demuxer::seek(double timestamp, int stream_index, int flags) -> bool
{
    if (!fmt_ctx_)
    {
        return false;
    }

    AVFormatContext* ctx     = fmt_ctx_->get();
    int64_t          seek_ts = timestamp * AV_TIME_BASE;

    if (stream_index >= 0 && std::cmp_less(stream_index, ctx->nb_streams))
    {
        AVStream* stream = ctx->streams[stream_index];
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

void Demuxer::dumpInfo() const
{
    if (!fmt_ctx_)
    {
        return;
    }
    fmt_ctx_->dumpInfo(0, url_.c_str(), 0);
}

bool Demuxer::rebuild()
{
    LOGI("重建解封装器");

    // 1. 完全销毁旧资源
    close();

    // 2. 等待一下，让网络恢复
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 3. 重新打开
    return open();
}

bool Demuxer::isValid() const
{
    return fmt_ctx_ != nullptr;
}
