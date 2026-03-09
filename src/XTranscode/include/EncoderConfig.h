#pragma once

#include "AVConst.h"
#include <iostream>

/// ==================== 编码器配置类 ====================
class EncoderConfig
{
public:
    /// 编码器ID（默认H265）
    AVCodecID codec_id = AV_CODEC_ID_H265;

    /// 视频参数
    int           width        = 400;
    int           height       = 300;
    int           framerate    = 25;
    int           bitrate      = 400000; /// 400 kbps
    int           gop_size     = 12;
    int           max_b_frames = 0;
    int           thread_count = 16;
    AVPixelFormat pix_fmt      = AV_PIX_FMT_YUV420P;

    /// H264特定参数
    struct H264Params
    {
        std::string preset    = "ultrafast";
        std::string profile   = "baseline";
        std::string tune      = ""; /// 可选: zerolatency, film, animation
        int         crf       = 23;
        bool        force_idr = true;
        bool        open_gop  = false;
    } h264;

    /// H265特定参数
    struct H265Params
    {
        std::string preset         = "ultrafast";
        int         crf            = 23;
        int         tu_intra_depth = 4;
    } h265;

    /// 打印配置信息
    void print() const;

    /// 根据codec_id获取对应的编码器名称
    std::string getCodecName() const;

    /// 检查配置是否有效
    bool isValid() const;
};
