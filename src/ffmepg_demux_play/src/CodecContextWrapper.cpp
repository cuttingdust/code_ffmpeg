#include "CodecContextWrapper.h"
#include "AVException.h"

extern "C" {
#include <libavcodec/codec.h>
}

class CodecContextWrapper::PImpl
{
public:
    PImpl(CodecContextWrapper *owenr);
    ~PImpl() = default;

public:
    CodecContextWrapper *owenr_ = nullptr;
    AVCodecContext      *ctx    = NULL;
};

CodecContextWrapper::PImpl::PImpl(CodecContextWrapper *owenr) : owenr_(owenr)
{
}

CodecContextWrapper::CodecContextWrapper(const AVCodec *codec) : impl_(std::make_shared<PImpl>(this))
{
    impl_->ctx = avcodec_alloc_context3(codec);
    if (!impl_->ctx)
    {
        throw AVException("无法分配编码器上下文");
    }
}

CodecContextWrapper::~CodecContextWrapper()
{
    if (impl_->ctx)
    {
        avcodec_free_context(&impl_->ctx);
    }
}

auto CodecContextWrapper::get() const -> AVCodecContext *
{
    return impl_->ctx;
}

auto CodecContextWrapper::operator->() const -> AVCodecContext *
{
    return impl_->ctx;
}

CodecContextWrapper::operator AVCodecContext *() const
{
    return impl_->ctx;
}
