#pragma once

#include "AVConst.h"

/// \brief AVChannelLayout 的 RAII 包装（FFmpeg 5+ channel API）
///
/// 对 av_channel_layout_copy / default 等会分配内部资源的布局，析构时自动 av_channel_layout_uninit。
/// 宏常量布局（如 AV_CHANNEL_LAYOUT_STEREO）按值写入后同样可安全 uninit（空 map 时为 no-op）。
class ChannelLayoutWrapper
{
public:
    ChannelLayoutWrapper() = default;
    ~ChannelLayoutWrapper();

    ChannelLayoutWrapper(ChannelLayoutWrapper&& other) noexcept;
    ChannelLayoutWrapper& operator=(ChannelLayoutWrapper&& other) noexcept;

    ChannelLayoutWrapper(const ChannelLayoutWrapper&)            = delete;
    ChannelLayoutWrapper& operator=(const ChannelLayoutWrapper&) = delete;

    /// \brief 立体声布局（2ch）
    static auto stereo() -> ChannelLayoutWrapper;

    /// \brief 单声道布局（1ch）
    static auto mono() -> ChannelLayoutWrapper;

    /// \brief 从已有布局深拷贝（会接管 dst 侧资源，先释放当前布局）
    auto copy_from(const AVChannelLayout* src) -> void;

    /// \brief 按声道数生成默认布局（如 2 → stereo）
    auto set_default(int nb_channels) -> void;

    auto get() const -> const AVChannelLayout*;

    auto get() -> AVChannelLayout*;

    auto nb_channels() const -> int;

    explicit operator bool() const;

private:
    auto uninit() -> void;

    AVChannelLayout layout_{};
};
