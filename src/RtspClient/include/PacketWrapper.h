#pragma once

#include "AVConst.h"

/// AVPacket的RAII包装
class PacketWrapper
{
public:
    PacketWrapper();
    using Ptr = std::unique_ptr<PacketWrapper>;
    static auto create() -> PacketWrapper::Ptr
    {
        return std::make_unique<PacketWrapper>();
    }


    ~PacketWrapper();

    /// 移动构造函数
    PacketWrapper(PacketWrapper&& other) noexcept;

    /// 移动赋值操作符
    PacketWrapper& operator=(PacketWrapper&& other) noexcept;

    /// 禁止拷贝
    PacketWrapper(const PacketWrapper&)            = delete;
    PacketWrapper& operator=(const PacketWrapper&) = delete;

public:
    auto get() const -> AVPacket*;

    auto operator->() const -> AVPacket*;

    operator AVPacket*() const;

    auto unref() const -> void;

    /// 检查是否有效
    explicit operator bool() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
