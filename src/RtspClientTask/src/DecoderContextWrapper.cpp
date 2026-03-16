#include "DecoderContextWrapper.h"
#include "AVException.h"

extern "C" {
#include <libavcodec/codec.h>
}

class DecoderContextWrapper::PImpl
{
public:
    PImpl(DecoderContextWrapper* owner);
    ~PImpl();

public:
    DecoderContextWrapper* owner_ = nullptr;
    AVCodecContext*        ctx_   = nullptr;
};

DecoderContextWrapper::PImpl::PImpl(DecoderContextWrapper* owner) : owner_(owner)
{
}

DecoderContextWrapper::PImpl::~PImpl()
{
    if (ctx_)
    {
        avcodec_free_context(&ctx_);
    }
}

DecoderContextWrapper::DecoderContextWrapper(const AVCodec* codec) : impl_(std::make_shared<PImpl>(this))
{
    impl_->ctx_ = avcodec_alloc_context3(codec);
    if (!impl_->ctx_)
    {
        throw AVException("无法分配解码器上下文");
    }
}

DecoderContextWrapper::~DecoderContextWrapper()
{
    // PImpl 的析构函数会自动清理
}

auto DecoderContextWrapper::get() const -> AVCodecContext*
{
    return impl_->ctx_;
}

auto DecoderContextWrapper::operator->() const -> AVCodecContext*
{
    return impl_->ctx_;
}

DecoderContextWrapper::operator AVCodecContext*() const
{
    return impl_->ctx_;
}

// ✅ 添加 close 方法
auto DecoderContextWrapper::close() -> void
{
    if (impl_->ctx_)
    {
        avcodec_free_context(&impl_->ctx_);
        impl_->ctx_ = nullptr;
    }
}
