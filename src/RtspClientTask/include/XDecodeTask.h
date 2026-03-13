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
    /// 初始化解码器
    bool initDecoder(AVCodecID codec_id, AVStream* stream);

    /// 获取解码器
    VideoDecoder* getDecoder() const
    {
        return decoder_.get();
    }

    /// 设置帧回调（用于直接渲染，如果不用下游任务）
    void setFrameCallback(DecoderConfig::FrameCallback cb)
    {
        frame_cb_ = cb;
    }

    /// 获取统计信息
    VideoDecoder::Stats getStats() const
    {
        return decoder_ ? decoder_->get_stats() : VideoDecoder::Stats();
    }

protected:
    /// 任务处理逻辑
    void process() override;

private:
    VideoDecoder::Ptr            decoder_;
    DecoderConfig::FrameCallback frame_cb_;
};
