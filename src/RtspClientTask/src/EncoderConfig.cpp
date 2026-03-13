#include "EncoderConfig.h"
#include <iostream>

void EncoderConfig::print() const
{
    std::cout << "\n========== 编码器配置 ==========" << std::endl;
    std::cout << "编码器: " << getCodecName() << " (" << codec_id << ")" << std::endl;
    std::cout << "分辨率: " << width << "x" << height << std::endl;
    std::cout << "帧率: " << framerate << " fps" << std::endl;
    std::cout << "比特率: " << bitrate / 1000 << " kbps" << std::endl;
    std::cout << "GOP大小: " << gop_size << " 帧" << std::endl;
    std::cout << "最大B帧: " << max_b_frames << std::endl;
    std::cout << "线程数: " << thread_count << std::endl;
    std::cout << "像素格式: " << av_get_pix_fmt_name(pix_fmt) << std::endl;

    // 打印编码器特定参数
    if (codec_id == AV_CODEC_ID_H264)
    {
        std::cout << "\n--- H.264 特定参数 ---" << std::endl;
        std::cout << "preset: " << h264.preset << std::endl;
        std::cout << "profile: " << h264.profile << std::endl;
        if (!h264.tune.empty())
        {
            std::cout << "tune: " << h264.tune << std::endl;
        }
        std::cout << "crf: " << h264.crf << std::endl;
        std::cout << "force_idr: " << (h264.force_idr ? "是" : "否") << std::endl;
        std::cout << "open_gop: " << (h264.open_gop ? "是" : "否") << std::endl;
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        std::cout << "\n--- H.265/HEVC 特定参数 ---" << std::endl;
        std::cout << "preset: " << h265.preset << std::endl;
        std::cout << "crf: " << h265.crf << std::endl;
        std::cout << "tu_intra_depth: " << h265.tu_intra_depth << std::endl;
    }

    std::cout << "================================\n" << std::endl;
}

std::string EncoderConfig::getCodecName() const
{
    switch (codec_id)
    {
        case AV_CODEC_ID_H264:
            return "H.264";
        case AV_CODEC_ID_HEVC:
            return "H.265/HEVC";
        case AV_CODEC_ID_MPEG4:
            return "MPEG-4";
        case AV_CODEC_ID_VP9:
            return "VP9";
        case AV_CODEC_ID_AV1:
            return "AV1";
        default:
            return "未知";
    }
}

bool EncoderConfig::isValid() const
{
    if (width <= 0 || height <= 0)
    {
        std::cerr << "无效的分辨率: " << width << "x" << height << std::endl;
        return false;
    }

    if (framerate <= 0)
    {
        std::cerr << "无效的帧率: " << framerate << std::endl;
        return false;
    }

    if (bitrate <= 0)
    {
        std::cerr << "无效的比特率: " << bitrate << std::endl;
        return false;
    }

    if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_HEVC)
    {
        std::cerr << "不支持的编码器ID: " << codec_id << std::endl;
        return false;
    }

    return true;
}
