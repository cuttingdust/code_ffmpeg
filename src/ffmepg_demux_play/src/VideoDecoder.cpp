#include "VideoDecoder.h"
#include "AVException.h"
#include "FrameWrapper.h"

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

#include <iostream>

void DecoderConfig::print() const
{
    std::cout << "\n========== 解码器配置 ==========" << std::endl;
    std::cout << "编码器ID: " << codec_id << std::endl;
    std::cout << "线程数: " << thread_count << std::endl;

    /// 打印时间基和帧率
    if (time_base.num > 0 && time_base.den > 0)
    {
        std::cout << "时间基: " << time_base.num << "/" << time_base.den << std::endl;
    }
    if (frame_rate.num > 0 && frame_rate.den > 0)
    {
        std::cout << "帧率: " << av_q2d(frame_rate) << " fps" << std::endl;
    }

    std::cout << "硬件加速: " << (hardware.enable ? "开启" : "关闭") << std::endl;
    if (hardware.enable)
    {
        std::cout << "  首选类型: " << HardwareContext::type_name(hardware.preferred_type) << std::endl;
        std::cout << "  自动选择: " << (hardware.auto_select ? "是" : "否") << std::endl;
        std::cout << "  转换到软件帧: " << (hardware.transfer_to_software ? "是" : "否") << std::endl;
    }
    std::cout << "================================\n" << std::endl;
}

class VideoDecoder::PImpl
{
public:
    PImpl(VideoDecoder* owner, DecoderConfig cfg);
    ~PImpl();

public:
    auto find_decoder() -> void;
    auto setup_hardware() -> void;
    auto setup_context() -> void;
    auto init_parser() -> void;
    auto reset_stats() -> void;
    auto update_stats(double decode_time) -> void;

public:
    VideoDecoder* owner_ = nullptr;
    DecoderConfig config_;

    const AVCodec*                          codec_ = nullptr;
    std::unique_ptr<DecoderContextWrapper>  ctx_;
    std::unique_ptr<ParserWrapper>          parser_;
    std::unique_ptr<HardwareContext>        hw_ctx_;
    std::shared_ptr<CodecParametersWrapper> codec_params_;

    PacketWrapper pkt_;
    FrameWrapper  frame_;
    FrameWrapper  sw_frame_; /// 用于硬件帧转换

    Stats   stats_;
    int64_t last_stats_time_  = 0;
    int     frames_in_second_ = 0;

    DecoderConfig::FrameCallback callback_;

    std::atomic<bool> is_hw_decoding_{ false };
};

VideoDecoder::PImpl::PImpl(VideoDecoder* owner, DecoderConfig cfg) :
    owner_(owner), config_(std::move(cfg)), pkt_(), frame_(), sw_frame_()
{
}

VideoDecoder::PImpl::~PImpl() = default;

auto VideoDecoder::PImpl::find_decoder() -> void
{
    codec_ = avcodec_find_decoder(config_.codec_id);
    if (!codec_)
    {
        throw AVException("找不到解码器: " + std::to_string(config_.codec_id));
    }

    std::cout << "解码器名称: " << codec_->name << std::endl;
    std::cout << "解码器描述: " << codec_->long_name << std::endl;
}

auto VideoDecoder::PImpl::setup_hardware() -> void
{
    if (!config_.hardware.enable)
    {
        return;
    }

    hw_ctx_ = std::make_unique<HardwareContext>();

    bool hw_ok = false;

    /// 如果指定了首选类型
    if (config_.hardware.preferred_type != HardwareContext::Type::None)
    {
        hw_ok = hw_ctx_->init(config_.hardware.preferred_type);
        if (hw_ok)
        {
            std::cout << "使用指定硬件加速: " << HardwareContext::type_name(config_.hardware.preferred_type)
                      << std::endl;
        }
    }

    /// 自动选择
    if (!hw_ok && config_.hardware.auto_select)
    {
        hw_ok = hw_ctx_->init_auto();
    }

    if (hw_ok)
    {
        /// 打印解码器支持的硬件格式
        std::cout << "\n解码器支持的硬件格式:" << std::endl;
        for (int i = 0;; i++)
        {
            auto config = avcodec_get_hw_config(codec_, i);
            if (!config)
                break;

            if (config->device_type)
            {
                std::cout << "  " << av_hwdevice_get_type_name(config->device_type);
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                    std::cout << " (支持设备上下文)";
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX)
                    std::cout << " (支持帧上下文)";
                std::cout << std::endl;
            }
        }

        is_hw_decoding_ = true;
    }
    else
    {
        std::cout << "硬件加速初始化失败，使用软件解码" << std::endl;
        hw_ctx_.reset();
    }
}

auto VideoDecoder::PImpl::setup_context() -> void
{
    ctx_ = std::make_unique<DecoderContextWrapper>(codec_);

    auto* avctx = ctx_->get();

    /// 如果有编码参数，复制到上下文
    if (codec_params_)
    {
        AVCodecParameters* par = codec_params_->get();
        avcodec_parameters_to_context(avctx, par);

        /// 设置时间基
        avctx->time_base = config_.time_base;
        avctx->framerate = config_.frame_rate;

        std::cout << "使用预设编码参数: " << avctx->width << "x" << avctx->height << std::endl;
    }

    /// 设置线程数
    avctx->thread_count = config_.thread_count;

    /// 设置硬件上下文
    if (hw_ctx_ && hw_ctx_->is_initialized())
    {
        avctx->hw_device_ctx = av_buffer_ref(hw_ctx_->get());
    }

    /// 打开解码器
    int ret = avcodec_open2(avctx, codec_, nullptr);
    if (ret < 0)
    {
        throw AVException("打开解码器失败", ret);
    }

    std::cout << "\n========== 解码器初始化完成 ==========" << std::endl;
    std::cout << "解码器名称: " << codec_->name << std::endl;
    std::cout << "线程数: " << avctx->thread_count << std::endl;
    std::cout << "硬件加速: " << (is_hw_decoding_ ? "是" : "否") << std::endl;
    if (is_hw_decoding_ && hw_ctx_)
    {
        std::cout << "硬件类型: " << HardwareContext::type_name(hw_ctx_->current_type()) << std::endl;
    }
    std::cout << "视频参数将在解码第一帧后确定" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

auto VideoDecoder::PImpl::init_parser() -> void
{
    parser_ = std::make_unique<ParserWrapper>(config_.codec_id);
}

auto VideoDecoder::PImpl::reset_stats() -> void
{
    stats_            = Stats{};
    last_stats_time_  = av_gettime_relative();
    frames_in_second_ = 0;
}

auto VideoDecoder::PImpl::update_stats(double decode_time) -> void
{
    stats_.frames_decoded++;
    frames_in_second_++;

    /// 更新平均解码时间
    stats_.avg_decode_time_ms =
            (stats_.avg_decode_time_ms * (stats_.frames_decoded - 1) + decode_time) / stats_.frames_decoded;

    /// 每秒打印一次fps
    int64_t now = av_gettime_relative();
    if (now - last_stats_time_ >= 1000000) /// 1秒
    {
        std::cout << "\r解码FPS: " << frames_in_second_ << "    " << std::flush;
        frames_in_second_ = 0;
        last_stats_time_  = now;
    }
}

/// VideoDecoder 公有接口实现
VideoDecoder::VideoDecoder(const DecoderConfig& cfg) : impl_(std::make_unique<PImpl>(this, cfg))
{
}

VideoDecoder::~VideoDecoder() = default;

// 添加设置参数的方法
auto VideoDecoder::set_parameters(const AVCodecParameters* par) -> bool
{
    if (!par)
        return false;

    impl_->codec_params_ = std::make_shared<CodecParametersWrapper>();
    return impl_->codec_params_->copy_from(par);
}

auto VideoDecoder::set_parameters_from_stream(AVStream* stream) -> bool
{
    if (!stream || !stream->codecpar)
        return false;

    impl_->codec_params_ = std::make_shared<CodecParametersWrapper>();
    bool ret             = impl_->codec_params_->from_stream(stream);

    if (ret)
    {
        /// 保存时间基信息
        impl_->config_.time_base  = stream->time_base;
        impl_->config_.frame_rate = stream->avg_frame_rate;
    }

    return ret;
}

auto VideoDecoder::get_parameters() const -> std::shared_ptr<CodecParametersWrapper>
{
    return impl_->codec_params_;
}

auto VideoDecoder::open() -> void
{
    impl_->find_decoder();
    impl_->setup_hardware();
    impl_->setup_context();
    impl_->init_parser();
    impl_->reset_stats();
}

auto VideoDecoder::close() -> void
{
    impl_.reset();
}

auto VideoDecoder::get_ctx() const -> AVCodecContext*
{
    return impl_->ctx_ ? impl_->ctx_->get() : nullptr;
}

auto VideoDecoder::decode(const uint8_t* data, int size, std::vector<AVFrame*>& out_frames) -> int
{
    if (!impl_->ctx_ || !impl_->parser_)
    {
        throw AVException("解码器未打开");
    }

    int            total_consumed = 0;
    const uint8_t* input_data     = data;
    int            remaining      = size;

    while (remaining > 0)
    {
        // 解析数据
        int consumed = impl_->parser_->parse(impl_->ctx_->get(), impl_->pkt_, input_data, remaining);
        if (consumed < 0)
        {
            break;
        }

        input_data += consumed;
        remaining -= consumed;
        total_consumed += consumed;

        /// 如果有完整的packet，进行解码
        if (impl_->pkt_->size > 0)
        {
            decode_packet(impl_->pkt_, out_frames);
            impl_->pkt_.unref();
        }
    }

    return total_consumed;
}

auto VideoDecoder::decode_packet(const AVPacket* pkt, std::vector<AVFrame*>& out_frames) -> int
{
    if (!impl_->ctx_)
    {
        throw AVException("解码器未打开");
    }

    impl_->stats_.packets_processed++;

    int64_t start_time = av_gettime_relative();

    /// ===== 处理 MP4 格式的 H.264/H.265 数据 =====
    AVPacket* input_pkt      = pkt;
    AVPacket  temp_pkt       = { 0 };
    bool      need_free_temp = false;

    /// 发送packet
    int ret = avcodec_send_packet(impl_->ctx_->get(), input_pkt);
    if (ret < 0)
    {
        if (need_free_temp)
        {
            av_free(temp_pkt.data);
        }
        impl_->stats_.errors++;
        throw AVException("发送packet失败", ret);
    }

    if (need_free_temp)
    {
        av_free(temp_pkt.data);
    }

    int         frame_count    = 0;
    static bool first_frame    = true;
    static bool params_printed = false;

    /// 接收帧
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(impl_->ctx_->get(), impl_->frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        if (ret < 0)
        {
            impl_->stats_.errors++;
            throw AVException("接收帧失败", ret);
        }

        frame_count++;

        /// 只在第一帧时打印参数
        if (first_frame && frame_count > 0)
        {
            first_frame = false;

            std::cout << "\n========== 第一帧解码成功 ==========" << std::endl;
            std::cout << "宽度: " << impl_->frame_->width << std::endl;
            std::cout << "高度: " << impl_->frame_->height << std::endl;
            auto pix_fmt = static_cast<AVPixelFormat>(impl_->frame_->format);
            std::cout << "像素格式: " << av_get_pix_fmt_name(pix_fmt) << " (" << pix_fmt << ")" << std::endl;
            std::cout << "PTS: " << impl_->frame_->pts << std::endl;
            std::cout << "====================================\n" << std::endl;
        }


        /// 确定输出帧
        AVFrame* output_frame = impl_->frame_;
        bool     is_hw        = false;

        /// 如果是硬件帧且需要转换
        if (HardwareFrameTransfer::is_hardware_frame(impl_->frame_))
        {
            impl_->stats_.hardware_frames++;
            is_hw = true;

            if (impl_->config_.hardware.transfer_to_software)
            {
                /// 设置软件帧格式
                impl_->sw_frame_->format = HardwareFrameTransfer::get_sw_format(impl_->frame_);
                impl_->sw_frame_->width  = impl_->frame_->width;
                impl_->sw_frame_->height = impl_->frame_->height;

                /// 分配缓冲区
                int buffer_ret = av_frame_get_buffer(impl_->sw_frame_, 0);
                if (buffer_ret < 0)
                {
                    std::cerr << "无法分配软件帧缓冲区" << std::endl;
                    continue;
                }

                /// 传输数据
                if (HardwareFrameTransfer::transfer_to_software(impl_->frame_, impl_->sw_frame_))
                {
                    output_frame = impl_->sw_frame_;
                }
            }
        }
        else
        {
            impl_->stats_.software_frames++;
        }

        /// 复制帧（因为我们要存储）
        AVFrame* new_frame = av_frame_alloc();
        if (new_frame)
        {
            int ref_ret = av_frame_ref(new_frame, output_frame);
            if (ref_ret >= 0)
            {
                out_frames.push_back(new_frame);
            }
            else
            {
                av_frame_free(&new_frame);
            }
        }

        /// 调用回调
        if (impl_->callback_)
        {
            impl_->callback_(output_frame, is_hw);
        }

        /// 更新统计
        int64_t end_time    = av_gettime_relative();
        double  decode_time = (end_time - start_time) / 1000.0; /// 转换为毫秒
        impl_->update_stats(decode_time);
    }

    impl_->frame_.unref();
    impl_->sw_frame_.unref();

    return frame_count;
}

auto VideoDecoder::flush(std::vector<AVFrame*>& out_frames) -> int
{
    if (!impl_->ctx_)
    {
        return 0;
    }

    std::cout << "\n刷新解码器缓冲区..." << std::endl;

    int ret = avcodec_send_packet(impl_->ctx_->get(), nullptr);
    if (ret < 0)
    {
        return 0;
    }

    int frame_count = 0;

    while (true)
    {
        ret = avcodec_receive_frame(impl_->ctx_->get(), impl_->frame_);
        if (ret == AVERROR_EOF)
            break;
        if (ret < 0)
            break;

        frame_count++;

        AVFrame* output_frame = impl_->frame_;

        if (HardwareFrameTransfer::is_hardware_frame(impl_->frame_) && impl_->config_.hardware.transfer_to_software)
        {
            impl_->sw_frame_->format = HardwareFrameTransfer::get_sw_format(impl_->frame_);
            impl_->sw_frame_->width  = impl_->frame_->width;
            impl_->sw_frame_->height = impl_->frame_->height;
            av_frame_get_buffer(impl_->sw_frame_, 0);

            if (HardwareFrameTransfer::transfer_to_software(impl_->frame_, impl_->sw_frame_))
            {
                output_frame = impl_->sw_frame_;
            }
        }

        AVFrame* new_frame = av_frame_alloc();
        av_frame_ref(new_frame, output_frame);
        out_frames.push_back(new_frame);

        if (impl_->callback_)
        {
            impl_->callback_(output_frame, false);
        }

        impl_->frame_.unref();
        impl_->sw_frame_.unref();
    }

    return frame_count;
}

void VideoDecoder::set_frame_callback(DecoderConfig::FrameCallback callback)
{
    impl_->callback_ = std::move(callback);
}

auto VideoDecoder::get_stats() const -> Stats
{
    return impl_->stats_;
}

auto VideoDecoder::print_stats() const -> void
{
    std::cout << "\n========== 解码器统计 ==========" << std::endl;
    std::cout << "解码帧数: " << impl_->stats_.frames_decoded << std::endl;
    std::cout << "处理packet数: " << impl_->stats_.packets_processed << std::endl;
    std::cout << "硬件帧数: " << impl_->stats_.hardware_frames << std::endl;
    std::cout << "软件帧数: " << impl_->stats_.software_frames << std::endl;
    std::cout << "错误数: " << impl_->stats_.errors << std::endl;
    std::cout << "平均解码时间: " << std::fixed << std::setprecision(2) << impl_->stats_.avg_decode_time_ms << " ms"
              << std::endl;
    std::cout << "================================" << std::endl;
}

auto VideoDecoder::is_hardware_decoding() const -> bool
{
    return impl_->is_hw_decoding_;
}

auto VideoDecoder::get_supported_pixel_formats() const -> std::vector<AVPixelFormat>
{
    std::vector<AVPixelFormat> formats;

    if (!impl_->codec_)
    {
        return formats;
    }


    const AVPixelFormat* p = impl_->codec_->pix_fmts;
    if (p)
    {
        while (*p != AV_PIX_FMT_NONE)
        {
            formats.push_back(*p);
            p++;
        }
    }

    return formats;
}

auto VideoDecoder::wait_for_pixel_format(int timeout_ms) -> AVPixelFormat
{
    // 这个功能需要在解码循环中实现
    // 这里只是提供一个接口，实际使用时需要在收到第一帧后调用
    return AV_PIX_FMT_NONE;
}
