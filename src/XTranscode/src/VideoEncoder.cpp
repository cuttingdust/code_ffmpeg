#include "VideoEncoder.h"

/// ==================== RAII包装类 ====================
#include "AVException.h"
#include "CodecContextWrapper.h"
#include "PacketWrapper.h"
#include "DictWrapper.h"
/// ====================================================

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
}


class VideoEncoder::PImpl
{
public:
    PImpl(VideoEncoder *owner, AVCodecID id, EncoderConfig cfg);
    ~PImpl() = default;

public:
    auto setup_basic_params() -> void;
    auto setup_codec_specific_params() -> void;
    auto open() -> void;
    auto print_codec_params(AVCodecContext *c) const -> void;

private:
    auto setup_h264_params() -> void;
    auto setup_h265_params() -> void;

public:
    VideoEncoder       *owner_ = nullptr;
    const AVCodec      *codec_ = nullptr; ///< 编码器
    CodecContextWrapper ctx_;             ///< 编码器上下文（现在可以直接存储对象）

    AVCodecID     codec_id_; ///< 编码器ID
    std::string   codec_name_;
    DictWrapper   opts_;
    EncoderConfig config_;
};

VideoEncoder::PImpl::PImpl(VideoEncoder *owner, AVCodecID id, EncoderConfig cfg) :
    owner_(owner), codec_id_(id), config_(std::move(cfg)), ctx_(codec_)
{
}

auto VideoEncoder::PImpl::setup_basic_params() -> void
{
    // 先检查 ctx_ 是否有效
    if (!ctx_)
    {
        throw std::runtime_error("编码器上下文无效");
    }

    ctx_->width        = config_.width;
    ctx_->height       = config_.height;
    ctx_->time_base    = { .num = 1, .den = config_.framerate };
    ctx_->framerate    = { .num = config_.framerate, .den = 1 };
    ctx_->pix_fmt      = config_.pix_fmt;
    ctx_->thread_count = config_.thread_count;
    ctx_->max_b_frames = config_.max_b_frames;
    ctx_->bit_rate     = config_.bitrate;
    ctx_->gop_size     = config_.gop_size;

    /// VBV设置
    ctx_->rc_max_rate    = config_.bitrate;
    ctx_->rc_buffer_size = config_.bitrate * 2;
}

auto VideoEncoder::PImpl::setup_codec_specific_params() -> void
{
    if (codec_id_ == AV_CODEC_ID_H264)
    {
        setup_h264_params();
    }
    else if (codec_id_ == AV_CODEC_ID_HEVC)
    {
        setup_h265_params();
    }
}

auto VideoEncoder::PImpl::open() -> void
{
    opts_.print("即将设置的参数");

    std::cout << "\n正在打开编码器..." << std::endl;
    int ret = avcodec_open2(ctx_, codec_, opts_.get_ptr());

    opts_.check_unused();

    if (ret < 0)
    {
        throw AVException("打开编码器失败", ret);
    }

    std::cout << "编码器打开成功!" << std::endl;
    print_codec_params(ctx_);
}

auto VideoEncoder::PImpl::print_codec_params(AVCodecContext *c) const -> void
{
    std::cout << "\n========== 编码器参数 ==========" << std::endl;
    std::cout << "宽度: " << c->width << std::endl;
    std::cout << "高度: " << c->height << std::endl;
    std::cout << "像素格式: " << av_get_pix_fmt_name(c->pix_fmt) << std::endl;
    std::cout << "时间基: " << c->time_base.num << "/" << c->time_base.den << std::endl;
    std::cout << "帧率: " << av_q2d(c->framerate) << " fps" << std::endl;
    std::cout << "目标比特率: " << c->bit_rate / 1000 << " kbps" << std::endl;
    std::cout << "最大比特率: " << c->rc_max_rate / 1000 << " kbps" << std::endl;
    std::cout << "缓冲区大小: " << c->rc_buffer_size / 1000 << " kbits" << std::endl;
    std::cout << "GOP大小: " << c->gop_size << " 帧" << std::endl;
    std::cout << "最大B帧: " << c->max_b_frames << std::endl;
    std::cout << "线程数: " << c->thread_count << std::endl;
    std::cout << "编码器名称: " << c->codec->name << std::endl;
    std::cout << "================================\n" << std::endl;
}

auto VideoEncoder::PImpl::setup_h264_params() -> void
{
    std::cout << "\n--- 设置H264特定参数 ---" << std::endl;

    opts_.set("crf", std::to_string(config_.h264.crf));
    opts_.set("preset", config_.h264.preset);
    opts_.set("profile", config_.h264.profile);

    if (!config_.h264.tune.empty())
    {
        opts_.set("tune", config_.h264.tune);
    }

    if (config_.h264.force_idr)
    {
        /// 使用x264-params传递IDR相关参数
        std::string x264_params = "keyint=" + std::to_string(config_.gop_size) +
                ":"
                "min-keyint=" +
                std::to_string(config_.gop_size) +
                ":"
                "open-gop=" +
                std::string(config_.h264.open_gop ? "1" : "0") +
                ":"
                "scenecut=0";

        opts_.set("x264-params", x264_params);
    }
}

auto VideoEncoder::PImpl::setup_h265_params() -> void
{
    std::cout << "\n--- 设置H265特定参数 ---" << std::endl;

    std::string x265_params = "preset=" + config_.h265.preset +
            ":"
            "tu-intra-depth=" +
            std::to_string(config_.h265.tu_intra_depth) +
            ":"
            "crf=" +
            std::to_string(config_.h265.crf) +
            ":"
            "bframes=" +
            std::to_string(config_.max_b_frames);

    opts_.set("x265-params", x265_params);
}

VideoEncoder::VideoEncoder(AVCodecID id, const EncoderConfig &cfg) : impl_(std::make_unique<PImpl>(this, id, cfg))
{
    try
    {
        /// 1. 查找编码器
        impl_->codec_ = avcodec_find_encoder(impl_->codec_id_);
        if (!impl_->codec_)
        {
            throw AVException("找不到编码器: " + std::to_string(impl_->codec_id_));
        }

        impl_->codec_name_ = impl_->codec_->name;
        std::cout << "编码器名称: " << impl_->codec_->name << std::endl;
        std::cout << "编码器描述: " << impl_->codec_->long_name << std::endl;

        /// 2. 创建上下文（使用移动赋值）
        impl_->ctx_ = CodecContextWrapper(impl_->codec_);

        /// 3. 验证上下文是否有效
        if (!impl_->ctx_)
        {
            throw AVException("无法创建有效的编码器上下文");
        }

        /// 4. 设置基础参数
        impl_->setup_basic_params();

        /// 5. 设置编码器特定参数
        impl_->setup_codec_specific_params();

        /// 6. 打开编码器
        impl_->open();
    }
    catch (const std::exception &e)
    {
        std::cerr << "编码器初始化失败: " << e.what() << std::endl;
        throw;
    }
}

VideoEncoder::~VideoEncoder() = default;

auto VideoEncoder::get_ctx() const -> AVCodecContext *
{
    return impl_->ctx_;
}

auto VideoEncoder::encode_frame(AVFrame *frame, std::vector<AVPacket *> &out_packets) -> void
{
    if (!impl_->ctx_)
    {
        throw AVException("编码器上下文无效");
    }

    int ret = avcodec_send_frame(impl_->ctx_, frame);
    if (ret < 0)
    {
        throw AVException("发送帧失败", ret);
    }

    PacketWrapper pkt;
    while (true)
    {
        ret = avcodec_receive_packet(impl_->ctx_, pkt);
        if (ret == AVERROR(EAGAIN))
            break;
        if (ret == AVERROR_EOF)
            break;
        if (ret < 0)
        {
            throw AVException("接收包失败", ret);
        }

        /// 复制包（因为pkt是局部的）
        AVPacket *new_pkt = av_packet_alloc();
        av_packet_ref(new_pkt, pkt);
        out_packets.push_back(new_pkt);

        pkt.unref();
    }
}

auto VideoEncoder::flush() -> std::vector<AVPacket *>
{
    std::vector<AVPacket *> packets;

    if (!impl_->ctx_)
    {
        return packets;
    }

    int ret = avcodec_send_frame(impl_->ctx_, nullptr);
    if (ret < 0)
    {
        return packets;
    }

    PacketWrapper pkt;
    while (true)
    {
        ret = avcodec_receive_packet(impl_->ctx_, pkt);
        if (ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;

        AVPacket *new_pkt = av_packet_alloc();
        av_packet_ref(new_pkt, pkt);
        packets.push_back(new_pkt);

        pkt.unref();
    }
    return packets;
}

auto VideoEncoder::print_stats() const -> void
{
    if (!impl_->ctx_)
        return;

    std::cout << "\n========== 编码器统计 ==========" << std::endl;
    if (impl_->codec_id_ == AV_CODEC_ID_H264 && strstr(impl_->codec_->name, "libx264"))
    {
        // libx264会在关闭时自动打印统计信息
    }
}
