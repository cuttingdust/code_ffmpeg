#pragma once

#include "XTask.h"
#include "VideoDecoder.h"

class XDecodeTask : public XTask
{
    DECLARE_CREATE(XDecodeTask)
public:
    XDecodeTask();
    ~XDecodeTask() override;

public:
    auto setHardwareDecode(bool enable) -> void;

    /// 初始化解码器
    auto initDecoder(AVCodecID codec_id, AVStream *stream) -> bool;

    /// 重置解码器（不清空队列）
    void resetDecoder(); 

    /// 获取解码器
    auto getDecoder() const -> VideoDecoder *;

    /// 设置帧回调（用于直接渲染，如果不用下游任务）
    auto setFrameCallback(DecoderConfig::FrameCallback cb) -> void;

    /// 获取统计信息
    auto getStats() const -> VideoDecoder::Stats;

    auto reset() -> void override;

protected:
    /// 任务处理逻辑
    auto process() -> void override;

private:
    VideoDecoder::Ptr            decoder_      = nullptr;
    DecoderConfig::FrameCallback frame_cb_     = nullptr;
    bool                         use_hardware_ = true;
};
