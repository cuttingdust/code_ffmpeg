#include "HardwareContext.h"
#include "AVException.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

HardwareContext::HardwareContext() = default;

HardwareContext::~HardwareContext()
{
    if (hw_ctx_)
    {
        av_buffer_unref(&hw_ctx_);
    }
}

auto HardwareContext::init(Type type) -> bool
{
    AVHWDeviceType hw_type = AV_HWDEVICE_TYPE_NONE;

    switch (type)
    {
        case Type::DXVA2:
            hw_type = AV_HWDEVICE_TYPE_DXVA2;
            break;
        case Type::D3D11VA:
            hw_type = AV_HWDEVICE_TYPE_D3D11VA;
            break;
        case Type::CUDA:
            hw_type = AV_HWDEVICE_TYPE_CUDA;
            break;
        case Type::QSV:
            hw_type = AV_HWDEVICE_TYPE_QSV;
            break;
        case Type::VAAPI:
            hw_type = AV_HWDEVICE_TYPE_VAAPI;
            break;
        case Type::VDPAU:
            hw_type = AV_HWDEVICE_TYPE_VDPAU;
            break;
        case Type::VIDEOTOOLBOX:
            hw_type = AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
            break;
        case Type::MEDIACODEC:
            hw_type = AV_HWDEVICE_TYPE_MEDIACODEC;
            break;
        case Type::OPENCL:
            hw_type = AV_HWDEVICE_TYPE_OPENCL;
            break;
        case Type::VULKAN:
            hw_type = AV_HWDEVICE_TYPE_VULKAN;
            break;
        default:
            return false;
    }

    if (hw_ctx_)
    {
        av_buffer_unref(&hw_ctx_);
    }

    int ret = av_hwdevice_ctx_create(&hw_ctx_, hw_type, nullptr, nullptr, 0);
    if (ret < 0)
    {
        return false;
    }

    current_type_ = type;
    return true;
}

auto HardwareContext::init_auto() -> bool
{
    auto types = get_available_types();
    for (auto type : types)
    {
        if (init(type))
        {
            std::cout << "自动选择硬件加速: " << type_name(type) << std::endl;
            return true;
        }
    }
    return false;
}

auto HardwareContext::get_available_types() const -> std::vector<Type>
{
    std::vector<Type> types;

    for (int i = 0;; i++)
    {
        auto hw_type = static_cast<AVHWDeviceType>(i);
        hw_type      = av_hwdevice_iterate_types(hw_type);
        if (hw_type == AV_HWDEVICE_TYPE_NONE)
        {
            break;
        }

        switch (hw_type)
        {
            case AV_HWDEVICE_TYPE_DXVA2:
                types.push_back(Type::DXVA2);
                break;
            case AV_HWDEVICE_TYPE_D3D11VA:
                types.push_back(Type::D3D11VA);
                break;
            case AV_HWDEVICE_TYPE_CUDA:
                types.push_back(Type::CUDA);
                break;
            case AV_HWDEVICE_TYPE_QSV:
                types.push_back(Type::QSV);
                break;
            case AV_HWDEVICE_TYPE_VAAPI:
                types.push_back(Type::VAAPI);
                break;
            case AV_HWDEVICE_TYPE_VDPAU:
                types.push_back(Type::VDPAU);
                break;
            case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
                types.push_back(Type::VIDEOTOOLBOX);
                break;
            case AV_HWDEVICE_TYPE_MEDIACODEC:
                types.push_back(Type::MEDIACODEC);
                break;
            case AV_HWDEVICE_TYPE_OPENCL:
                types.push_back(Type::OPENCL);
                break;
            case AV_HWDEVICE_TYPE_VULKAN:
                types.push_back(Type::VULKAN);
                break;
            default:
                break;
        }
    }

    return types;
}

auto HardwareContext::type_name(Type type) -> const char*
{
    switch (type)
    {
        case Type::DXVA2:
            return "DXVA2";
        case Type::D3D11VA:
            return "D3D11VA";
        case Type::CUDA:
            return "CUDA";
        case Type::QSV:
            return "QSV";
        case Type::VAAPI:
            return "VAAPI";
        case Type::VDPAU:
            return "VDPAU";
        case Type::VIDEOTOOLBOX:
            return "VideoToolbox";
        case Type::MEDIACODEC:
            return "MediaCodec";
        case Type::OPENCL:
            return "OpenCL";
        case Type::VULKAN:
            return "Vulkan";
        default:
            return "None";
    }
}

auto HardwareContext::type_from_string(const std::string& name) -> Type
{
    std::string lower = name;
    for (auto& c : lower)
        c = tolower(c);

    if (lower == "dxva2")
        return Type::DXVA2;
    if (lower == "d3d11va" || lower == "d3d11")
        return Type::D3D11VA;
    if (lower == "cuda")
        return Type::CUDA;
    if (lower == "qsv")
        return Type::QSV;
    if (lower == "vaapi")
        return Type::VAAPI;
    if (lower == "vdpau")
        return Type::VDPAU;
    if (lower == "videotoolbox")
        return Type::VIDEOTOOLBOX;
    if (lower == "mediacodec")
        return Type::MEDIACODEC;
    if (lower == "opencl")
        return Type::OPENCL;
    if (lower == "vulkan")
        return Type::VULKAN;

    return Type::None;
}

auto HardwareContext::get() const -> AVBufferRef*
{
    return hw_ctx_;
}

/// HardwareFrameTransfer实现
HardwareFrameTransfer::HardwareFrameTransfer()  = default;
HardwareFrameTransfer::~HardwareFrameTransfer() = default;

auto HardwareFrameTransfer::transfer_to_software(AVFrame* hw_frame, AVFrame* sw_frame) -> bool
{
    if (!hw_frame || !sw_frame)
    {
        LOGE("transfer_to_software: 空指针");
        return false;
    }

    /// ✅ 检查硬件帧是否有效
    if (!is_hardware_frame(hw_frame))
    {
        LOGE("transfer_to_software: 不是硬件帧");
        return false;
    }

    /// ✅ 检查帧数据指针
    if (!hw_frame->data[0] && !hw_frame->data[1] && !hw_frame->data[2] && !hw_frame->data[3])
    {
        LOGE("transfer_to_software: 硬件帧数据为空");
        return false;
    }

    try
    {
        int ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
        if (ret < 0)
        {
            char err_buf[256];
            av_strerror(ret, err_buf, sizeof(err_buf));
            LOGE("av_hwframe_transfer_data 失败: " << err_buf);
            return false;
        }
        return true;
    }
    catch (const std::exception& e)
    {
        LOGE("硬件帧转换异常: " << e.what());
        return false;
    }
    catch (...)
    {
        LOGE("硬件帧转换未知异常");
        return false;
    }
}

auto HardwareFrameTransfer::is_hardware_frame(AVFrame* frame) -> bool
{
    if (!frame)
    {
        return false;
    }


    auto desc = av_pix_fmt_desc_get(static_cast<AVPixelFormat>(frame->format));
    if (!desc)
    {
        return false;
    }

    return !!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL);
}

auto HardwareFrameTransfer::get_sw_format(AVFrame* frame) -> AVPixelFormat
{
    if (!frame)
        return AV_PIX_FMT_NONE;

    /// 如果是硬件帧，尝试找到对应的软件格式
    if (is_hardware_frame(frame))
    {
        /// NV12 是最常见的硬件解码输出格式
        return AV_PIX_FMT_NV12;
    }

    return static_cast<AVPixelFormat>(frame->format);
}
