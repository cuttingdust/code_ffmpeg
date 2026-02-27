#pragma once

#include "AVConst.h"

/// AVCodecParserContext的RAII包装
class ParserWrapper
{
public:
    explicit ParserWrapper(AVCodecID codec_id);
    ~ParserWrapper();

public:
    auto get() const -> AVCodecParserContext*;
    auto operator->() const -> AVCodecParserContext*;
    operator AVCodecParserContext*() const;

    /// 解析数据，返回消耗的字节数
    auto parse(AVCodecContext* ctx, AVPacket* pkt, const uint8_t* data, int data_size) -> int;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
