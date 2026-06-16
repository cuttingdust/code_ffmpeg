#pragma once

#include "AVConst.h"
#include "CodecParametersWrapper.h"
#include "DecoderContextWrapper.h"
#include "FrameWrapper.h"
#include "PacketWrapper.h"

/// \brief 音频解码器输出配置（对齐 XAudioPlay：S16 交错）
struct AudioDecoderConfig
{
    int            thread_count       = 8;
    int            output_sample_rate = 0; ///< 0 表示与源一致
    int            output_channels    = 2; ///< 输出声道数，默认立体声
    AVSampleFormat output_sample_fmt  = AV_SAMPLE_FMT_S16;
    AVRational     time_base          = { 1, 44100 };
};

/// \brief 音频解码器：FFmpeg 解码 + SwrContext 重采样为 S16 packed PCM
class AudioDecoder
{
public:
    explicit AudioDecoder(const AudioDecoderConfig& cfg = AudioDecoderConfig{});
    ~AudioDecoder();

    using Ptr = std::unique_ptr<AudioDecoder>;
    static auto create(const AudioDecoderConfig& cfg = AudioDecoderConfig{}) -> AudioDecoder::Ptr
    {
        return std::make_unique<AudioDecoder>(cfg);
    }

    /// 从流设置 codecpar（须在 open 前调用）
    auto set_parameters_from_stream(AVStream* stream) -> bool;

    auto set_parameters(const AVCodecParameters* par) -> bool;

    auto get_parameters() const -> std::shared_ptr<CodecParametersWrapper>;

    auto open() -> void;

    auto close() -> void;

    auto get_ctx() const -> AVCodecContext*;

    /// 解码单个 packet，out_frames 中为 S16 交错 AVFrame（调用方 av_frame_free）
    auto decode_packet(const AVPacket* pkt, std::vector<AVFrame*>& out_frames) -> int;

    /// 冲洗解码器并输出剩余 PCM 帧
    auto flush(std::vector<AVFrame*>& out_frames) -> int;

    auto flushBuffers() -> void;

    auto reset() -> void;

    auto output_sample_rate() const -> int;

    auto output_channels() const -> int;

    auto output_sample_format() const -> AVSampleFormat;

    struct Stats
    {
        int frames_decoded    = 0;
        int packets_processed = 0;
        int errors            = 0;
    };

    auto get_stats() const -> Stats;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
