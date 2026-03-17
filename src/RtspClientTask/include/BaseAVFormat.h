#pragma once

#include "AVConst.h"
#include "FormatContextWrapper.h"
#include "CodecParametersWrapper.h"
#include <vector>

/// ==================== 基础格式类 ====================
class BaseAVFormat
{
public:
    virtual ~BaseAVFormat() = default;

public:
    /// 设置网络超时（毫秒）
    auto setTimeout(int timeout_ms) -> void;

    /// 获取超时设置
    auto getTimeout() const -> int;

    /// 设置传输协议（TCP/UDP）
    auto setTransport(const std::string &transport) -> void;

    /// 设置缓冲区大小
    auto setBufferSize(int size) -> void;

    /// 配置 RTSP 选项（便捷方法）
    auto setRtspOptions(bool use_tcp = true, int timeout_ms = 3000) -> void;

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
    auto getFilename() const -> std::string;

    /// 获取原始 AVFormatContext（谨慎使用）
    auto getRawContext() const -> AVFormatContext *;

    /// 打印文件信息
    virtual auto dumpInfo() const -> void = 0;

    /// 检查是否超时（用于中断回调）
    auto isTimeout() const -> bool;


    /// 重置计时器
    auto resetTimer() -> void;

protected:
    BaseAVFormat(std::string url);

    /// 查找视频流和音频流索引
    void findStreamIndices();

    /// 构建选项字典（用于打开输入/输出）
    auto getOptionsPtr() -> AVDictionary **;

    /// 检查是否是网络协议
    auto isNetworkProtocol() const -> bool;

    /// 静态中断回调函数
    static auto interruptCallback(void *ctx) -> int;

protected:
    std::unique_ptr<FormatContextWrapper> fmt_ctx_;
    std::string                           url_;
    int                                   video_stream_index_ = -1;
    int                                   audio_stream_index_ = -1;

    /// RTSP/网络配置
    DictWrapper options_;
    int         timeout_ms_    = 3000;    /// 默认3秒
    std::string transport_     = "tcp";   /// 默认TCP
    int         buffer_size_   = 2048000; /// 2MB
    int         max_delay_ms_  = 500;     /// 500ms
    bool        options_dirty_ = true;    /// 选项是否需要重新构建

    /// 超时计时
    mutable std::chrono::steady_clock::time_point last_activity_time_;
    mutable std::chrono::steady_clock::time_point start_time_;
};
