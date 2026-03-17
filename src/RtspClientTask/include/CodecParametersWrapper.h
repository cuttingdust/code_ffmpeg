#pragma once

#include "AVConst.h"

/// AVCodecParameters 的 RAII 包装
class CodecParametersWrapper
{
public:
    explicit CodecParametersWrapper();
    explicit CodecParametersWrapper(AVCodecParameters *par);

    using Ptr = std::shared_ptr<CodecParametersWrapper>;
    static auto create() -> CodecParametersWrapper::Ptr
    {
        return std::make_shared<CodecParametersWrapper>();
    }


    ~CodecParametersWrapper();

public:
    /// 从 AVStream 复制参数
    auto from_stream(AVStream *stream) -> bool;

    /// 从已有的参数复制
    auto copy_from(const AVCodecParameters *par) -> bool;

    /// 获取原始指针
    auto get() const -> AVCodecParameters *;

    /// 获取编码器ID
    auto get_codec_id() const -> AVCodecID;

    /// 获取视频宽度
    auto get_width() const -> int;

    /// 获取视频高度
    auto get_height() const -> int;

    /// 获取像素格式
    auto get_pix_fmt() const -> AVPixelFormat;

    /// 获取帧率
    auto get_frame_rate(AVStream *stream) const -> AVRational;

    /// 获取时间基
    auto get_time_base(AVStream *stream) const -> AVRational;

    /// 打印参数信息
    void print_info() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
