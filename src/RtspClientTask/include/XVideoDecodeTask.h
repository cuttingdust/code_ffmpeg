#pragma once

#include "XTask.h"
#include "VideoDecoder.h"

/// \brief 视频解码 Task：从上游接收 AVPacket，输出 AVFrame
class XVideoDecodeTask : public XTask
{
    DECLARE_CREATE(XVideoDecodeTask)
public:
    XVideoDecodeTask();
    ~XVideoDecodeTask() override;

public:
    /// 初始化解码器
    auto initDecoder(AVCodecID codec_id, AVStream* stream) -> bool;

    /// 获取解码器
    auto getDecoder() const -> VideoDecoder*;

    /// 设置帧回调（用于直接渲染，如果不用下游任务）
    auto setFrameCallback(DecoderConfig::FrameCallback cb) -> void;

    /// 获取统计信息
    auto getStats() const -> VideoDecoder::Stats;

    auto reset() -> void override;

protected:
    /// 任务处理逻辑
    auto process() -> void override;

private:
    VideoDecoder::Ptr            decoder_;
    DecoderConfig::FrameCallback frame_cb_;
};
