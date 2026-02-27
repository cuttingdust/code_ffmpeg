#pragma once

#include "AVConst.h"

/// AVCodecContext的RAII包装
class CodecContextWrapper
{
public:
    explicit CodecContextWrapper(const AVCodec* codec);
    ~CodecContextWrapper();

public:
    auto get() const -> AVCodecContext*;

    auto operator->() const -> AVCodecContext*;

    operator AVCodecContext*() const;

private:
    class PImpl;
    std::shared_ptr<PImpl> impl_;
};
