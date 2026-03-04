#include "FormatContextWrapper.h"
#include "AVException.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

class FormatContextWrapper::PImpl
{
public:
    PImpl() = default;
    ~PImpl()
    {
        if (ctx_)
        {
            if (is_input_)
            {
                avformat_close_input(&ctx_);
            }
            else
            {
                if (ctx_->pb)
                {
                    avio_closep(&ctx_->pb);
                }
                avformat_free_context(ctx_);
            }
        }
    }

    AVFormatContext* ctx_      = nullptr;
    bool             is_input_ = false;
};

FormatContextWrapper::FormatContextWrapper() : impl_(std::make_unique<PImpl>())
{
}

FormatContextWrapper::~FormatContextWrapper() = default;

auto FormatContextWrapper::createInput(const std::string& url) -> std::unique_ptr<FormatContextWrapper>
{
    auto wrapper = std::unique_ptr<FormatContextWrapper>(new FormatContextWrapper());
    if (!wrapper->openInput(url))
    {
        throw AVException("打开输入文件失败: " + url);
    }
    return wrapper;
}

auto FormatContextWrapper::createOutput(const std::string& url) -> std::unique_ptr<FormatContextWrapper>
{
    auto wrapper = std::unique_ptr<FormatContextWrapper>(new FormatContextWrapper());
    if (!wrapper->openOutput(url))
    {
        throw AVException("创建输出文件失败: " + url);
    }
    return wrapper;
}

auto FormatContextWrapper::openInput(const std::string& url) -> bool
{
    impl_->is_input_ = true;
    int ret          = avformat_open_input(&impl_->ctx_, url.c_str(), nullptr, nullptr);
    return ret >= 0;
}

auto FormatContextWrapper::openOutput(const std::string& url) -> bool
{
    impl_->is_input_ = false;

    int ret = avformat_alloc_output_context2(&impl_->ctx_, nullptr, nullptr, url.c_str()); /// 后缀名分析
    if (ret < 0 || !impl_->ctx_)
    {
        return false;
    }

    ret = avio_open(&impl_->ctx_->pb, url.c_str(), AVIO_FLAG_WRITE);
    return ret >= 0;
}

auto FormatContextWrapper::get() const -> AVFormatContext*
{
    return impl_->ctx_;
}

FormatContextWrapper::operator AVFormatContext*() const
{
    return impl_->ctx_;
}

auto FormatContextWrapper::findStreamInfo() -> int
{
    if (!impl_->ctx_ || !impl_->is_input_)
    {
        return -1;
    }

    return avformat_find_stream_info(impl_->ctx_, nullptr);
}

auto FormatContextWrapper::addStream() -> AVStream*
{
    if (!impl_->ctx_ || impl_->is_input_)
    {
        return nullptr;
    }

    return avformat_new_stream(impl_->ctx_, nullptr);
}

auto FormatContextWrapper::writeHeader(AVDictionary** options) -> int
{
    if (!impl_->ctx_ || impl_->is_input_)
        return -1;
    return avformat_write_header(impl_->ctx_, options);
}

auto FormatContextWrapper::writeTrailer() -> int
{
    if (!impl_->ctx_ || impl_->is_input_)
        return -1;
    return av_write_trailer(impl_->ctx_);
}

void FormatContextWrapper::dumpInfo(int index, const char* url, int is_output) const
{
    if (!impl_->ctx_)
    {
        return;
    }
    /// 0 输入 1 输出
    av_dump_format(impl_->ctx_, index, url, is_output);
}

auto FormatContextWrapper::getDuration() const -> double
{
    if (!impl_->ctx_ || impl_->ctx_->duration == AV_NOPTS_VALUE)
        return 0.0;
    return impl_->ctx_->duration / (double)AV_TIME_BASE;
}

auto FormatContextWrapper::getBitRate() const -> int64_t
{
    if (!impl_->ctx_)
        return 0;
    return impl_->ctx_->bit_rate;
}
