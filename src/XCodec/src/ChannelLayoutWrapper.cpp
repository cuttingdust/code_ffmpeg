#include "ChannelLayoutWrapper.h"

#include "AVException.h"

ChannelLayoutWrapper::~ChannelLayoutWrapper()
{
    uninit();
}

ChannelLayoutWrapper::ChannelLayoutWrapper(ChannelLayoutWrapper&& other) noexcept : layout_(other.layout_)
{
    other.layout_ = {};
}

ChannelLayoutWrapper& ChannelLayoutWrapper::operator=(ChannelLayoutWrapper&& other) noexcept
{
    if (this != &other)
    {
        uninit();
        layout_       = other.layout_;
        other.layout_ = {};
    }
    return *this;
}

auto ChannelLayoutWrapper::stereo() -> ChannelLayoutWrapper
{
    ChannelLayoutWrapper layout;
    layout.layout_ = AV_CHANNEL_LAYOUT_STEREO;
    return layout;
}

auto ChannelLayoutWrapper::mono() -> ChannelLayoutWrapper
{
    ChannelLayoutWrapper layout;
    layout.layout_ = AV_CHANNEL_LAYOUT_MONO;
    return layout;
}

auto ChannelLayoutWrapper::copy_from(const AVChannelLayout* src) -> void
{
    if (!src)
    {
        throw AVException("copy_from: src 为空");
    }

    uninit();
    if (av_channel_layout_copy(&layout_, src) < 0)
    {
        layout_ = {};
        throw AVException("av_channel_layout_copy 失败");
    }
}

auto ChannelLayoutWrapper::set_default(int nb_channels) -> void
{
    if (nb_channels <= 0)
    {
        throw AVException("set_default: 无效声道数 " + std::to_string(nb_channels));
    }

    uninit();
    av_channel_layout_default(&layout_, nb_channels);
    if (layout_.nb_channels <= 0)
    {
        layout_ = {};
        throw AVException("av_channel_layout_default 失败");
    }
}

auto ChannelLayoutWrapper::get() const -> const AVChannelLayout*
{
    return &layout_;
}

auto ChannelLayoutWrapper::get() -> AVChannelLayout*
{
    return &layout_;
}

auto ChannelLayoutWrapper::nb_channels() const -> int
{
    return layout_.nb_channels;
}

ChannelLayoutWrapper::operator bool() const
{
    return layout_.nb_channels > 0;
}

auto ChannelLayoutWrapper::uninit() -> void
{
    av_channel_layout_uninit(&layout_);
    layout_ = {};
}
