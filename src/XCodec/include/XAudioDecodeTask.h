#pragma once

#include "XTask.h"
#include "AudioDecoder.h"

/// \brief 音频解码 Task：从上游接收 AVPacket，经 AudioDecoder 输出 S16 交错 AVFrame
class XCODEC_EXPORT XAudioDecodeTask : public XTask
{
    DECLARE_CREATE(XAudioDecodeTask)
public:
    XAudioDecodeTask();
    ~XAudioDecodeTask() override;

    /// 按音频流初始化解码器（须在 start 前调用）
    auto initDecoder(AVStream* stream) -> bool;

    auto getDecoder() const -> AudioDecoder*;

    void flushDownstream() override;

    auto getStats() const -> AudioDecoder::Stats;

    auto reset() -> void override;

protected:
    auto process() -> void override;

private:
    auto pushPcmFrames(std::vector<AVFrame*>& frames) -> void;

    AudioDecoder::Ptr         decoder_ = nullptr;
    std::atomic<bool>         need_flush_decoder_{ false };
};
