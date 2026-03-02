#include "PacketWrapper.h"

#include "AVException.h"

extern "C" {
#include <libavcodec/packet.h>
}

class PacketWrapper::PImpl
{
public:
    PImpl(PacketWrapper *owenr);
    ~PImpl() = default;

public:
    PacketWrapper *owenr_ = nullptr;
    AVPacket      *pkt_   = NULL;
};

PacketWrapper::PImpl::PImpl(PacketWrapper *owenr) : owenr_(owenr)
{
}


PacketWrapper::PacketWrapper() : impl_(std::make_unique<PImpl>(this))
{
    impl_->pkt_ = av_packet_alloc();
    if (!impl_->pkt_)
    {
        throw AVException("无法分配AVPacket");
    }
}

PacketWrapper::~PacketWrapper()
{
    if (impl_->pkt_)
    {
        av_packet_free(&impl_->pkt_);
    }
}

auto PacketWrapper::get() const -> AVPacket *
{
    return impl_->pkt_;
}

auto PacketWrapper::operator->() const -> AVPacket *
{
    return impl_->pkt_;
}

PacketWrapper::operator AVPacket *() const
{
    return impl_->pkt_;
}

auto PacketWrapper::unref() const -> void
{
    av_packet_unref(impl_->pkt_);
}
