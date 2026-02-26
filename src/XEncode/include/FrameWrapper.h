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

    auto allocate_buffer() const -> void;

private:
    class PImpl;
    std::shared_ptr<PImpl> impl_;
};
