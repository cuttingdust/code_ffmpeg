#include "FrameWrapper.h"
#include "AVException.h"

extern "C" {
#include <libavutil/frame.h>
}


class FrameWrapper::PImpl
{
public:
    PImpl(FrameWrapper *owenr);
    ~PImpl() = default;

public:
    FrameWrapper *owenr_ = nullptr;
    AVFrame      *frame_ = NULL;
};

FrameWrapper::PImpl::PImpl(FrameWrapper *owenr) : owenr_(owenr)
{
}

FrameWrapper::FrameWrapper() : impl_(std::make_shared<PImpl>(this))
{
    impl_->frame_ = av_frame_alloc();
    if (!impl_->frame_)
    {
        throw AVException("无法分配AVFrame");
    }
}

FrameWrapper::~FrameWrapper()
{
    if (impl_->frame_)
    {
        av_frame_free(&impl_->frame_);
    }
}

auto FrameWrapper::get() const -> AVFrame *
{
    return impl_->frame_;
}

auto FrameWrapper::operator->() const -> AVFrame *
{
    return impl_->frame_;
}

FrameWrapper::operator AVFrame *() const
{
    return impl_->frame_;
}

auto FrameWrapper::allocate_buffer() const -> void
{
    int ret = av_frame_get_buffer(impl_->frame_, 0);
    if (ret < 0)
    {
        throw AVException("无法分配帧缓冲区", ret);
    }
}
