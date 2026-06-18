#include "SwrContextWrapper.h"

#include "AVException.h"

SwrContextWrapper::~SwrContextWrapper()
{
    reset();
}

SwrContextWrapper::SwrContextWrapper(SwrContextWrapper&& other) noexcept : ctx_(other.ctx_)
{
    other.ctx_ = nullptr;
}

SwrContextWrapper& SwrContextWrapper::operator=(SwrContextWrapper&& other) noexcept
{
    if (this != &other)
    {
        reset();
        ctx_       = other.ctx_;
        other.ctx_ = nullptr;
    }
    return *this;
}

auto SwrContextWrapper::configure(const ChannelLayoutWrapper& out_layout,
                                  AVSampleFormat              out_fmt,
                                  int                         out_rate,
                                  const ChannelLayoutWrapper& in_layout,
                                  AVSampleFormat              in_fmt,
                                  int                         in_rate) -> void
{
    reset();

    /// swr_alloc_set_opts2：参数顺序 out 在前、in 在后；此时尚未做实际转换
    int ret = swr_alloc_set_opts2(&ctx_,
                                  out_layout.get(),
                                  out_fmt,
                                  out_rate,
                                  in_layout.get(),
                                  in_fmt,
                                  in_rate,
                                  0,
                                  nullptr);
    if (ret < 0 || !ctx_)
    {
        reset();
        throw AVException("swr_alloc_set_opts2 失败", ret);
    }

    /// swr_init：预计算滤波器；失败则释放 ctx_，避免半初始化泄漏
    ret = swr_init(ctx_);
    if (ret < 0)
    {
        reset();
        throw AVException("swr_init 失败", ret);
    }
}

auto SwrContextWrapper::get_out_samples(int in_samples) const -> int
{
    if (!ctx_ || in_samples <= 0)
    {
        return 0;
    }
    return swr_get_out_samples(ctx_, in_samples);
}

auto SwrContextWrapper::convert(uint8_t** dst, int dst_count, const uint8_t* const* src, int src_samples) -> int
{
    if (!ctx_)
    {
        return 0;
    }

    const int out_samples = swr_convert(ctx_, dst, dst_count, src, src_samples);
    if (out_samples < 0)
    {
        throw AVException("swr_convert 失败", out_samples);
    }
    return out_samples;
}

auto SwrContextWrapper::is_ready() const -> bool
{
    return ctx_ != nullptr;
}

auto SwrContextWrapper::get() const -> SwrContext*
{
    return ctx_;
}

auto SwrContextWrapper::reset() -> void
{
    if (ctx_)
    {
        swr_free(&ctx_);
        ctx_ = nullptr;
    }
}
