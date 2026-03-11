#pragma once

#include "AVConst.h"

/// AVCodecContext的RAII包装（支持移动语义，禁止拷贝）
class CodecContextWrapper
{
public:
    explicit CodecContextWrapper(const AVCodec* codec);

    /// 移动构造函数
    CodecContextWrapper(CodecContextWrapper&& other) noexcept;

    /// 移动赋值操作符
    CodecContextWrapper& operator=(CodecContextWrapper&& other) noexcept;

    /// 禁止拷贝
    CodecContextWrapper(const CodecContextWrapper&)            = delete;
    CodecContextWrapper& operator=(const CodecContextWrapper&) = delete;

    ~CodecContextWrapper();

public:
    auto get() const -> AVCodecContext*;
    auto operator->() const -> AVCodecContext*;
    operator AVCodecContext*() const;

    /// 检查是否有效
    explicit operator bool() const;

private:
    class PImpl;
    std::unique_ptr<PImpl> impl_; // 改用 unique_ptr 而不是 shared_ptr
};
