#pragma once

#include "AVConst.h"
#include <memory>

/// AVFrame的RAII包装
class FrameWrapper
{
public:
    FrameWrapper();
    explicit FrameWrapper(AVFrame* frame);
    ~FrameWrapper();

    // 移动语义
    FrameWrapper(FrameWrapper&& other) noexcept;
    FrameWrapper& operator=(FrameWrapper&& other) noexcept;

    // 禁止拷贝
    FrameWrapper(const FrameWrapper&)            = delete;
    FrameWrapper& operator=(const FrameWrapper&) = delete;

    using Ptr = std::unique_ptr<FrameWrapper>;
    static auto create() -> FrameWrapper::Ptr
    {
        return std::make_unique<FrameWrapper>();
    }

public:
    auto get() const -> AVFrame*;
    auto operator->() const -> AVFrame*;
    operator AVFrame*() const;
    explicit operator bool() const;

    /// 分配缓冲区
    auto allocate_buffer() const -> void;

    /// 取消引用帧（重置内容但不释放对象）
    auto unref() const -> void;

    /// 克隆帧（创建新引用）
    auto clone() const -> FrameWrapper;

    /// 释放所有权，返回原始指针
    auto release() -> AVFrame*;

    /// 重置（释放当前，指向新帧）
    void reset(AVFrame* frame = nullptr);

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
