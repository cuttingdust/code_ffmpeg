#pragma once

#include "AVConst.h"

#include <vector>
#include <string>

/// 硬件加速上下文管理
class HardwareContext
{
public:
    enum class Type
    {
        None,
        DXVA2,
        D3D11VA,
        CUDA,
        QSV,
        VAAPI,
        VDPAU,
        VIDEOTOOLBOX,
        MEDIACODEC,
        OPENCL,
        VULKAN
    };

    HardwareContext();
    ~HardwareContext();

public:
    /// 初始化指定类型的硬件上下文
    auto init(Type type) -> bool;

    /// 自动选择可用的硬件加速类型
    auto init_auto() -> bool;

    /// 获取可用硬件加速类型列表
    auto get_available_types() const -> std::vector<Type>;

    /// 获取类型名称
    static auto type_name(Type type) -> const char*;

    /// 转换字符串到类型
    static auto type_from_string(const std::string& name) -> Type;

    /// 获取硬件上下文
    auto get() const -> AVBufferRef*;

    /// 检查是否初始化
    auto is_initialized() const -> bool
    {
        return hw_ctx_ != nullptr;
    }

    /// 获取当前类型
    auto current_type() const -> Type
    {
        return current_type_;
    }

private:
    AVBufferRef* hw_ctx_       = nullptr;
    Type         current_type_ = Type::None;
};

/// 硬件帧转换器
class HardwareFrameTransfer
{
public:
    HardwareFrameTransfer();
    ~HardwareFrameTransfer();

public:
    /// 从硬件帧传输数据到软件帧
    static auto transfer_to_software(AVFrame* hw_frame, AVFrame* sw_frame) -> bool;

    /// 检查帧是否为硬件帧
    static auto is_hardware_frame(AVFrame* frame) -> bool;

    /// 获取帧的像素格式（如果是硬件帧，返回对应的软件格式）
    static auto get_sw_format(AVFrame* frame) -> AVPixelFormat;
};
