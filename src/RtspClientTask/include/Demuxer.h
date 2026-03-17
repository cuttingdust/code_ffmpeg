#pragma once

#include "BaseAVFormat.h"

/// ==================== 解封装器类 ====================
class Demuxer : public BaseAVFormat
{
public:
    explicit Demuxer(const std::string& url);
    ~Demuxer() override = default;

    using Ptr = std::unique_ptr<Demuxer>;
    static auto create(const std::string& url) -> Demuxer::Ptr
    {
        return std::make_unique<Demuxer>(url);
    }

public:
    /// 打开媒体文件
    auto open() -> bool;

    /// 关闭媒体文件
    auto close() -> void;

    /// 读取下一个数据包
    auto readPacket(AVPacket* pkt) -> int;

    /// 定位到指定时间（秒）
    auto seek(double timestamp, int stream_index = -1, int flags = AVSEEK_FLAG_BACKWARD) -> bool;

    /// 获取文件总时长（秒）
    auto getDuration() const -> double;

    /// 打印文件信息
    auto dumpInfo() const -> void override;

    /// 完全重建解封装器
    auto rebuild() -> bool;

    /// 检查是否有效
    auto isValid() const -> bool;
};
