#pragma once

#include "AVConst.h"

/// AVPacket的RAII包装
class PacketWrapper
{
public:
    PacketWrapper();

    ~PacketWrapper();

public:
    auto get() const -> AVPacket *;

    auto operator->() const -> AVPacket *;

    operator AVPacket *() const;

    auto unref() const -> void;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
