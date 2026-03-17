#pragma once

#include "AVConst.h"
#include "CodecParametersWrapper.h"
#include "DecoderContextWrapper.h"
#include "ParserWrapper.h"
#include "HardwareContext.h"
#include "FrameWrapper.h"
#include "PacketWrapper.h"

#include <functional>

/// ==================== 视频解码器配置 ====================
struct DecoderConfig
{
    AVCodecID codec_id     = AV_CODEC_ID_H264;
    int       thread_count = 16;

    /// 时间基和帧率（从流中获取）
    AVRational time_base  = { 1, 25 }; /// 时间基，用于 PTS/DTS 转换
    AVRational frame_rate = { 25, 1 }; /// 帧率

    /// 硬件加速配置
    struct HardwareConfig
    {
        bool                  enable               = true;                        /// 是否启用硬件加速
        HardwareContext::Type preferred_type       = HardwareContext::Type::None; /// 首选类型
        bool                  auto_select          = true;                        /// 自动选择可用类型
        bool                  transfer_to_software = true;                        /// 是否自动转换到软件帧（用于显示）
    } hardware;

    /// 回调函数类型
    using FrameCallback = std::function<void(AVFrame* frame, bool is_hw)>;

    void print() const;
};

/// ==================== 视频解码器类 ====================
class VideoDecoder
{
public:
    explicit VideoDecoder(const DecoderConfig& cfg = DecoderConfig());
    ~VideoDecoder();
    using Ptr = std::unique_ptr<VideoDecoder>;
    static auto create(const DecoderConfig& cfg) -> VideoDecoder::Ptr
    {
        return std::make_unique<VideoDecoder>(cfg);
    }


public:
    /// 设置编码参数（必须在 open 之前调用）
    auto set_parameters(const AVCodecParameters* par) -> bool;

    /// 从流设置编码参数（必须在 open 之前调用）
    auto set_parameters_from_stream(AVStream* stream) -> bool;

    /// 获取编码参数
    auto get_parameters() const -> CodecParametersWrapper::Ptr;

    /// 打开解码器
    auto open() -> void;

    /// 关闭解码器
    auto close() -> void;

    /// 获取解码器上下文
    auto get_ctx() const -> AVCodecContext*;

    /// 解析并解码数据
    auto decode(const uint8_t* data, int size, std::vector<AVFrame*>& out_frames) -> int;

    /// 解码单个packet
    auto decode_packet(AVPacket* pkt, std::vector<AVFrame*>& out_frames) -> int;

    /// 刷新解码器（获取剩余帧）
    auto flush(std::vector<AVFrame*>& out_frames) -> int;

    auto reset() -> void;

    /// 设置帧回调
    auto set_frame_callback(DecoderConfig::FrameCallback callback) -> void;

    /// 获取解码统计
    struct Stats
    {
        int    frames_decoded     = 0;
        int    packets_processed  = 0;
        int    hardware_frames    = 0;
        int    software_frames    = 0;
        int    errors             = 0;
        double avg_decode_time_ms = 0.0;
    };

    auto get_stats() const -> Stats;

    /// 打印统计信息
    auto print_stats() const -> void;

    /// 检查是否为硬件解码
    auto is_hardware_decoding() const -> bool;

    /// 获取解码器支持的像素格式列表
    auto get_supported_pixel_formats() const -> std::vector<AVPixelFormat>;

    /// 等待第一帧解码后获取实际像素格式
    auto wait_for_pixel_format(int timeout_ms = 5000) -> AVPixelFormat;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
