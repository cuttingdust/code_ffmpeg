#pragma once

#include "AVConst.h"

/// AVFrame的RAII包装
class FrameWrapper
{
public:
    FrameWrapper();

    ~FrameWrapper();

public:
    auto get() const -> AVFrame *;

    auto operator->() const -> AVFrame *;

    operator AVFrame *() const;

    /// 分配缓冲区
    auto allocate_buffer() const -> void;

    /// 取消引用帧（重置内容但不释放对象）
    auto unref() const -> void;

    /// 克隆帧（创建新引用）
    auto clone() const -> FrameWrapper;

private:
    class PImpl;
    std::shared_ptr<PImpl> impl_;
};
