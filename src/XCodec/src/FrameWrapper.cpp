#include "FrameWrapper.h"
#include "AVException.h"

class FrameWrapper::PImpl
{
public:
    PImpl() = default;
    ~PImpl()
    {
        if (frame_)
        {
            av_frame_free(&frame_);
        }
    }

    // 移动构造函数
    PImpl(PImpl&& other) noexcept : frame_(other.frame_)
    {
        other.frame_ = nullptr;
    }

    AVFrame* frame_ = nullptr;
};

FrameWrapper::FrameWrapper() : impl_(std::make_unique<PImpl>())
{
    impl_->frame_ = av_frame_alloc();
    if (!impl_->frame_)
    {
        throw AVException("无法分配AVFrame");
    }
}

FrameWrapper::FrameWrapper(AVFrame* frame) : impl_(std::make_unique<PImpl>())
{
    impl_->frame_ = frame;
}

FrameWrapper::~FrameWrapper() = default;

FrameWrapper::FrameWrapper(FrameWrapper&& other) noexcept : impl_(std::move(other.impl_))
{
}

FrameWrapper& FrameWrapper::operator=(FrameWrapper&& other) noexcept
{
    if (this != &other)
    {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

auto FrameWrapper::get() const -> AVFrame*
{
    return impl_ ? impl_->frame_ : nullptr;
}

auto FrameWrapper::operator->() const -> AVFrame*
{
    if (!impl_ || !impl_->frame_)
    {
        throw std::runtime_error("FrameWrapper: 访问空指针");
    }
    return impl_->frame_;
}

FrameWrapper::operator AVFrame*() const
{
    return impl_ ? impl_->frame_ : nullptr;
}

FrameWrapper::operator bool() const
{
    return impl_ && impl_->frame_;
}

auto FrameWrapper::allocate_buffer() const -> void
{
    if (!impl_ || !impl_->frame_)
    {
        throw AVException("无法分配缓冲区：帧为空");
    }
    int ret = av_frame_get_buffer(impl_->frame_, 0);
    if (ret < 0)
    {
        throw AVException("无法分配帧缓冲区", ret);
    }
}

auto FrameWrapper::unref() const -> void
{
    if (impl_ && impl_->frame_)
    {
        av_frame_unref(impl_->frame_);
    }
}

auto FrameWrapper::clone() const -> FrameWrapper
{
    if (!impl_ || !impl_->frame_)
    {
        throw AVException("无法克隆空帧");
    }

    FrameWrapper new_frame;
    int          ret = av_frame_ref(new_frame, impl_->frame_);
    if (ret < 0)
    {
        throw AVException("无法克隆帧", ret);
    }
    return new_frame;
}

auto FrameWrapper::release() -> AVFrame*
{
    if (!impl_)
        return nullptr;
    AVFrame* frame = impl_->frame_;
    impl_->frame_  = nullptr;
    return frame;
}

void FrameWrapper::reset(AVFrame* frame)
{
    impl_         = std::make_unique<PImpl>();
    impl_->frame_ = frame;
}
