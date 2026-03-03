#pragma once

#include "AVConst.h"
/// ==================== 编码器配置类 ====================
class EncoderConfig
{
public:
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

    void print() const;
};
