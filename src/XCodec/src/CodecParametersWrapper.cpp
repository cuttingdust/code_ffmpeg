#include "CodecParametersWrapper.h"
#include "AVException.h"
#include <iostream>

extern "C" {
#include <libavutil/pixdesc.h>
}


class CodecParametersWrapper::PImpl
{
public:
    PImpl()
    {
        params_ = avcodec_parameters_alloc();
        if (!params_)
        {
            throw AVException("无法分配 AVCodecParameters");
        }
    }

    explicit PImpl(AVCodecParameters *par)
    {
        params_ = avcodec_parameters_alloc();
        if (!params_)
        {
            throw AVException("无法分配 AVCodecParameters");
        }
        if (par)
        {
            avcodec_parameters_copy(params_, par);
        }
    }

    ~PImpl()
    {
        if (params_)
        {
            avcodec_parameters_free(&params_);
        }
    }

    AVCodecParameters *params_ = nullptr;
};

CodecParametersWrapper::CodecParametersWrapper() : impl_(std::make_unique<PImpl>())
{
}

CodecParametersWrapper::CodecParametersWrapper(AVCodecParameters *par) : impl_(std::make_unique<PImpl>(par))
{
}

CodecParametersWrapper::~CodecParametersWrapper() = default;

auto CodecParametersWrapper::from_stream(AVStream *stream) -> bool
{
    if (!stream || !stream->codecpar)
    {
        return false;
    }

    int ret = avcodec_parameters_copy(impl_->params_, stream->codecpar);
    return ret >= 0;
}

auto CodecParametersWrapper::copy_from(const AVCodecParameters *par) -> bool
{
    if (!par)
    {
        return false;
    }

    int ret = avcodec_parameters_copy(impl_->params_, par);
    return ret >= 0;
}

auto CodecParametersWrapper::get() const -> AVCodecParameters *
{
    return impl_->params_;
}

auto CodecParametersWrapper::get_codec_id() const -> AVCodecID
{
    return impl_->params_->codec_id;
}

auto CodecParametersWrapper::get_width() const -> int
{
    return impl_->params_->width;
}

auto CodecParametersWrapper::get_height() const -> int
{
    return impl_->params_->height;
}

auto CodecParametersWrapper::get_pix_fmt() const -> AVPixelFormat
{
    return static_cast<AVPixelFormat>(impl_->params_->format);
}

auto CodecParametersWrapper::get_frame_rate(AVStream *stream) const -> AVRational
{
    if (stream)
    {
        return stream->avg_frame_rate;
    }
    return { 0, 0 };
}

auto CodecParametersWrapper::get_time_base(AVStream *stream) const -> AVRational
{
    if (stream)
    {
        return stream->time_base;
    }
    return { 0, 0 };
}

void CodecParametersWrapper::print_info() const
{
    std::cout << "\n========== 编码参数 ==========" << std::endl;
    std::cout << "编码器: " << avcodec_get_name(impl_->params_->codec_id) << std::endl;

    if (impl_->params_->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        std::cout << "类型: 视频" << std::endl;
        std::cout << "分辨率: " << impl_->params_->width << "x" << impl_->params_->height << std::endl;
        std::cout << "像素格式: " << av_get_pix_fmt_name((AVPixelFormat)impl_->params_->format) << std::endl;
        std::cout << "比特率: " << impl_->params_->bit_rate / 1000 << " kbps" << std::endl;
    }
    else if (impl_->params_->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        std::cout << "类型: 音频" << std::endl;
        std::cout << "采样率: " << impl_->params_->sample_rate << " Hz" << std::endl;
        std::cout << "声道数: " << impl_->params_->ch_layout.nb_channels << std::endl;
    }

    if (impl_->params_->extradata && impl_->params_->extradata_size > 0)
    {
        std::cout << "额外数据: " << impl_->params_->extradata_size << " 字节" << std::endl;
    }
    std::cout << "================================\n" << std::endl;
}
