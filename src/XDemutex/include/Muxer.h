#pragma once

#include "AVConst.h"
#include "FormatContextWrapper.h"
#include "CodecParametersWrapper.h"
#include <string>
#include <vector>
#include <memory>

/// ==================== 封装器类 ====================
class Muxer
{
public:
    explicit Muxer(const std::string& url);
    ~Muxer();

public:
    /// 打开输出文件
    auto open() -> bool;

    /// 关闭输出文件
    void close();

    /// 添加视频流（从输入流复制参数）
    auto addVideoStream(AVStream* in_stream) -> int;

    /// 添加音频流（从输入流复制参数）
    auto addAudioStream(AVStream* in_stream) -> int;

    /// 添加流（从编码参数复制）
    auto addStream(const AVCodecParameters* codecpar, AVRational time_base) -> int;

    /// 获取视频流
    auto getVideoStream() const -> AVStream*;

    /// 获取音频流
    auto getAudioStream() const -> AVStream*;

    /// 获取指定索引的流
    auto getStream(int index) const -> AVStream*;

    /// 获取所有流
    auto getStreams() const -> std::vector<AVStream*>;

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

    /// 获取文件名
    auto getFilename() const -> std::string;

    /// 打印文件信息
    void dumpInfo() const;

private:
    std::unique_ptr<FormatContextWrapper> fmt_ctx_;
    std::string                           url_;
    int                                   video_stream_index_ = -1;
    int                                   audio_stream_index_ = -1;
    bool                                  header_written_     = false;
};
