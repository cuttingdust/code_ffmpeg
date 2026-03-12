#include "CodecContextWrapper.h"
#include "AVException.h"

class CodecContextWrapper::PImpl
{
public:
    PImpl() = default;
    ~PImpl()
    {
        if (ctx)
        {
            avcodec_free_context(&ctx);
        }
    }

    AVCodecContext* ctx = nullptr;
};

// 构造函数
CodecContextWrapper::CodecContextWrapper(const AVCodec* codec) : impl_(std::make_unique<PImpl>())
{
    impl_->ctx = avcodec_alloc_context3(codec);
    if (!impl_->ctx)
    {
        throw AVException("无法分配编码器上下文");
    }
}

// 移动构造函数
CodecContextWrapper::CodecContextWrapper(CodecContextWrapper&& other) noexcept :
    impl_(std::move(other.impl_)) // 直接转移所有权
{
    // other.impl_ 现在为 nullptr
}

// 移动赋值操作符
CodecContextWrapper& CodecContextWrapper::operator=(CodecContextWrapper&& other) noexcept
{
    if (this != &other)
    {
        // 先释放当前资源
        impl_.reset();

        // 转移资源
        impl_ = std::move(other.impl_);
    }
    return *this;
}

CodecContextWrapper::~CodecContextWrapper() = default; // unique_ptr 会自动清理

auto CodecContextWrapper::get() const -> AVCodecContext*
{
    return impl_ ? impl_->ctx : nullptr;
}

auto CodecContextWrapper::operator->() const -> AVCodecContext*
{
    // 安全检查
    if (!impl_ || !impl_->ctx)
    {
        throw std::runtime_error("CodecContextWrapper: 访问空指针");
    }
    return impl_->ctx;
}

CodecContextWrapper::operator AVCodecContext*() const
{
    return impl_ ? impl_->ctx : nullptr;
}

CodecContextWrapper::operator bool() const
{
    return impl_ && impl_->ctx;
}
