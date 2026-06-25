#pragma once

#include "XCodec_Global.h"

#include <memory>
#include <vector>

struct AVFrame;

/// \brief FFmpeg atempo 滤镜：保音调变速（单实例 0.5~2.0，更高倍速链式串联）
class XCODEC_EXPORT AudioAtempoFilter
{
public:
    AudioAtempoFilter();
    ~AudioAtempoFilter();

    AudioAtempoFilter(const AudioAtempoFilter&)            = delete;
    AudioAtempoFilter& operator=(const AudioAtempoFilter&) = delete;

    auto open(int sample_rate, int channels, int sample_fmt) -> bool;

    auto close() -> void;

    /// \brief 设置倍速；1.0 时 bypass，不建图
    auto setSpeed(double speed) -> void;

    auto speed() const -> double;

    auto isOpen() const -> bool;

    auto isBypass() const -> bool;

    /// \brief 重建滤镜图，丢弃中间缓冲（seek / 切速后调用，不发 EOF）
    auto resetPipeline() -> void;

    /// \brief 冲洗并输出剩余 PCM 帧（EOF 时调用）
    auto flushOutput(std::vector<AVFrame*>& out_frames) -> int;

    /// \brief 处理一帧 PCM；成功时可能输出 0~N 帧，调用方 av_frame_free
    auto process(AVFrame* in, std::vector<AVFrame*>& out_frames) -> int;

private:
    auto buildTempoChain(double speed) const -> std::vector<double>;

    auto rebuildGraph() -> bool;

    auto drainSink(std::vector<AVFrame*>& out_frames) -> int;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
