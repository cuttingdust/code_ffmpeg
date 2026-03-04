#pragma once

#include "AVConst.h"
#include "FormatContextWrapper.h"
#include "CodecParametersWrapper.h"
#include <string>
#include <vector>
#include <memory>

/// ==================== 基础格式类 ====================
class BaseAVFormat
{
public:
    virtual ~BaseAVFormat() = default;

public:
    /// 获取视频流
    auto getVideoStream() const -> AVStream *;

    /// 获取音频流
    auto getAudioStream() const -> AVStream *;

    /// 获取指定索引的流
    auto getStream(int index) const -> AVStream *;

    /// 获取所有流
    auto getStreams() const -> std::vector<AVStream *>;

    /// 获取编码参数包装器
    auto getCodecParameters(int stream_index) const -> std::shared_ptr<CodecParametersWrapper>;

    /// 获取文件名
    auto getFilename() const -> std::string
    {
        return url_;
    }

    /// 获取原始 AVFormatContext（谨慎使用）
    auto getRawContext() const -> AVFormatContext *
    {
        return fmt_ctx_ ? fmt_ctx_->get() : nullptr;
    }

    /// 打印文件信息
    virtual void dumpInfo() const = 0;

protected:
    BaseAVFormat(std::string url);

    /// 查找视频流和音频流索引
    void findStreamIndices();

protected:
    std::unique_ptr<FormatContextWrapper> fmt_ctx_;
    std::string                           url_;
    int                                   video_stream_index_ = -1;
    int                                   audio_stream_index_ = -1;
};
