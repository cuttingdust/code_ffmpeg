#pragma once

#include "BaseAVFormat.h"

/// ==================== 封装器类 ====================
class Muxer : public BaseAVFormat
{
public:
    explicit Muxer(const std::string& url);
    ~Muxer() override;

    using Ptr = std::unique_ptr<Muxer>;
    static auto create(const std::string& url) -> Muxer::Ptr
    {
        return std::make_unique<Muxer>(url);
    }

public:
    /// 打开输出文件
    auto open() -> bool;

    /// 关闭输出文件
    void close();

    /// 添加视频流（从输入流复制参数）
    auto addVideoStream(const AVStream* in_stream) -> int;

    /// 添加音频流（从输入流复制参数）
    auto addAudioStream(AVStream* in_stream) -> int;

    /// 添加流（从编码参数复制）
    auto addStream(const AVCodecParameters* codecpar, AVRational time_base) -> int;

    auto addStream(const AVCodecContext* enc_ctx, AVRational time_base) -> int;

    /// 写入文件头
    auto writeHeader() -> int;

    /// 写入文件尾
    auto writeTrailer() -> int;

    /// 写入数据包（自动处理时基转换）
    auto writePacket(AVPacket* pkt, AVStream* in_stream, int64_t pts_offset = 0) -> int;

    /// 写入数据包（指定输入输出流索引）
    auto writePacket(AVPacket* pkt, int in_stream_index, int out_stream_index, AVRational in_time_base,
                     int64_t pts_offset = 0) -> int;

    /// 获取输出上下文
    auto getContext() const -> AVFormatContext*;

    /// 打印文件信息
    void dumpInfo() const override;

private:
    bool header_written_ = false;
};
