#pragma once

#include "AVConst.h"
#include <string>
#include <memory>

/// AVFormatContext 的 RAII 包装（支持输入和输出）
class FormatContextWrapper
{
public:
    /// 输入模式构造函数
    static std::unique_ptr<FormatContextWrapper> createInput(const std::string& url);

    /// 输出模式构造函数
    static std::unique_ptr<FormatContextWrapper> createOutput(const std::string& url);

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

    bool openInput(const std::string& url);
    bool openOutput(const std::string& url);

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_;
};
