#pragma once

#include "AVConst.h"

/// AVCodecContext解码器的RAII包装
class DecoderContextWrapper
{
public:
    explicit DecoderContextWrapper(const AVCodec* codec);
    ~DecoderContextWrapper();

public:
    auto get() const -> AVCodecContext*;
    auto operator->() const -> AVCodecContext*;
    operator AVCodecContext*() const;

private:
    class PImpl;
    std::shared_ptr<PImpl> impl_;
};
