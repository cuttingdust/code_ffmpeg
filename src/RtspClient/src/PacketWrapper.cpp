#include "PacketWrapper.h"
#include "AVException.h"

extern "C" {
#include <libavcodec/packet.h>
}

class PacketWrapper::PImpl
{
public:
    PImpl()
    {
        pkt_ = av_packet_alloc();
        if (!pkt_)
        {
            throw AVException("无法分配AVPacket");
        }
    }

    ~PImpl()
    {
        if (pkt_)
        {
            av_packet_free(&pkt_);
        }
    }

    // 移动构造函数
    PImpl(PImpl&& other) noexcept : pkt_(other.pkt_)
    {
        other.pkt_ = nullptr;
    }

    // 禁止拷贝
    PImpl(const PImpl&)            = delete;
    PImpl& operator=(const PImpl&) = delete;

    AVPacket* pkt_ = nullptr;
};

PacketWrapper::PacketWrapper() : impl_(std::make_unique<PImpl>())
{
}

// 移动构造函数
PacketWrapper::PacketWrapper(PacketWrapper&& other) noexcept : impl_(std::move(other.impl_))
{
}

// 移动赋值操作符
PacketWrapper& PacketWrapper::operator=(PacketWrapper&& other) noexcept
{
    if (this != &other)
    {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

PacketWrapper::~PacketWrapper() = default;

auto PacketWrapper::get() const -> AVPacket*
{
    return impl_ ? impl_->pkt_ : nullptr;
}

auto PacketWrapper::operator->() const -> AVPacket*
{
    if (!impl_ || !impl_->pkt_)
    {
        throw std::runtime_error("PacketWrapper: 访问空指针");
    }
    return impl_->pkt_;
}

PacketWrapper::operator AVPacket*() const
{
    return impl_ ? impl_->pkt_ : nullptr;
}

auto PacketWrapper::unref() const -> void
{
    if (impl_ && impl_->pkt_)
    {
        av_packet_unref(impl_->pkt_);
    }
}

PacketWrapper::operator bool() const
{
    return impl_ && impl_->pkt_;
}
