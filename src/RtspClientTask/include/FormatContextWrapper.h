#pragma once

#include "AVConst.h"
#include "DictWrapper.h"

/// AVFormatContext 的 RAII 包装（支持输入和输出）
class FormatContextWrapper
{
public:
    using Ptr = std::unique_ptr<FormatContextWrapper>;

    /// 输入模式构造函数
    static auto createInput(const std::string& url, AVDictionary** options = nullptr,
                            AVIOInterruptCB* interrupt_cb = nullptr) -> FormatContextWrapper::Ptr;

    /// 输出模式构造函数
    static auto createOutput(const std::string& url) -> FormatContextWrapper::Ptr;


    ~FormatContextWrapper();


public:
    /// 获取原始指针
    auto get() const -> AVFormatContext*;
    operator AVFormatContext*() const;

    /// 查找流信息（输入模式）
    auto findStreamInfo() -> int;

    /// 创建新流（输出模式）
    auto addStream() -> AVStream*;

    /// 写入文件头（输出模式）
    auto writeHeader(AVDictionary** options = nullptr) -> int;

    /// 写入文件尾（输出模式）
    auto writeTrailer() -> int;

    /// 打印格式信息
    void dumpInfo(int index, const char* url, int is_output) const;

    /// 获取时长（秒）
    auto getDuration() const -> double;

    /// 获取比特率
    auto getBitRate() const -> int64_t;

private:
    FormatContextWrapper();

    auto openInput(const std::string& url, AVDictionary** options = nullptr,
                   const AVIOInterruptCB* interrupt_cb = nullptr) -> bool;

    auto openOutput(const std::string& url) -> bool;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
