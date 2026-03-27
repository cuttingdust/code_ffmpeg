#pragma once

#include "AVConst.h"
#include "EncoderConfig.h"

/// ==================== 视频编码器类 ====================
class VideoEncoder
{
public:
    VideoEncoder(AVCodecID id, const EncoderConfig &cfg = EncoderConfig());
    ~VideoEncoder();
    using Ptr = std::unique_ptr<VideoEncoder>;

    static auto create(AVCodecID id, const EncoderConfig &cfg) -> VideoEncoder::Ptr
    {
        return std::make_unique<VideoEncoder>(id, cfg);
    }

public:
    auto get_ctx() const -> AVCodecContext *;

    auto encode_frame(AVFrame *frame, std::vector<AVPacket *> &out_packets) -> void;

    auto flush() -> std::vector<AVPacket *>;

    auto print_stats() const -> void;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
