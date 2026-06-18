#pragma once

#include "ChannelLayoutWrapper.h"

/// \brief SwrContext 的 RAII 包装：格式/采样率/声道布局转换（libswresample）
///
/// 典型流程：configure() 一次完成 alloc + init；循环 convert()；析构自动 swr_free。
class SwrContextWrapper
{
public:
    SwrContextWrapper() = default;
    ~SwrContextWrapper();

    SwrContextWrapper(SwrContextWrapper&& other) noexcept;
    SwrContextWrapper& operator=(SwrContextWrapper&& other) noexcept;

    SwrContextWrapper(const SwrContextWrapper&)            = delete;
    SwrContextWrapper& operator=(const SwrContextWrapper&) = delete;

    /// \brief 按 out/in 规格创建并初始化 SwrContext（swr_alloc_set_opts2 + swr_init）
    ///
    /// 会先 reset 释放旧 context。init 失败时不留半初始化 context。
    auto configure(const ChannelLayoutWrapper& out_layout, AVSampleFormat out_fmt, int out_rate,
                   const ChannelLayoutWrapper& in_layout, AVSampleFormat in_fmt, int in_rate) -> void;

    /// \brief 估算输出样本数上限（包一层 swr_get_out_samples）
    auto get_out_samples(int in_samples) const -> int;

    /// \brief 转换一帧 PCM（swr_convert）；返回值即输出样本数，0 表示暂无输出，<0 由内部抛 AVException
    auto convert(uint8_t** dst, int dst_count, const uint8_t* const* src, int src_samples) -> int;

    auto is_ready() const -> bool;

    auto get() const -> SwrContext*;

    auto reset() -> void;

private:
    SwrContext* ctx_ = nullptr;
};
