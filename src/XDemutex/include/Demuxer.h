#pragma once

#include "AVConst.h"
#include "FormatContextWrapper.h"
#include "CodecParametersWrapper.h"
#include <string>
#include <vector>
#include <memory>

/// ==================== 解封装器类 ====================
class Demuxer
{
public:
    explicit Demuxer(const std::string& url);
    ~Demuxer();

public:
    /// 打开媒体文件
    auto open() -> bool;

    /// 关闭媒体文件
    void close();

    /// 获取视频流
    auto getVideoStream() const -> AVStream*;

    /// 获取音频流
    auto getAudioStream() const -> AVStream*;

    /// 获取指定索引的流
    auto getStream(int index) const -> AVStream*;

    /// 获取所有流
    auto getStreams() const -> std::vector<AVStream*>;

    /// 获取编码参数包装器（用于传递给解码器）
    auto getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>;

    /// 读取下一个数据包
    auto readPacket(AVPacket* pkt) -> int;

    /// 定位到指定时间（秒）
    auto seek(double timestamp, int stream_index = -1, int flags = AVSEEK_FLAG_BACKWARD) -> bool;

    /// 获取文件总时长（秒）
    auto getDuration() const -> double;

    /// 获取文件名
    auto getFilename() const -> std::string;

    /// 打印文件信息
    void dumpInfo() const;

    /// 获取原始 AVFormatContext（谨慎使用）
    auto getRawContext() const -> AVFormatContext*;

private:
    std::unique_ptr<FormatContextWrapper> fmt_ctx_;
    std::string                           url_;
    int                                   video_stream_index_ = -1;
    int                                   audio_stream_index_ = -1;
};
