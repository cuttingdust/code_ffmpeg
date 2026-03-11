#pragma once

#include "Demuxer.h"
#include "Muxer.h"
#include "VideoDecoder.h"
#include "VideoEncoder.h"
#include "EncoderConfig.h"
#include "PacketWrapper.h"
#include "FrameWrapper.h"
#include <string>
#include <functional>
#include <memory>

/// 转码进度回调类型
using TranscodeProgressCallback = std::function<void(int frame_number, int64_t pts, int fps)>;

/// ==================== 视频转码器类 ====================
class VideoTranscoder
{
public:
    /// 转码参数
    struct TranscodeParams
    {
        std::string input_file;              /// 输入文件路径
        std::string output_file;             /// 输出文件路径
        double      start_time      = 0.0;   /// 开始时间（秒）
        double      end_time        = 0.0;   /// 结束时间（秒）
        bool        transcode_video = true;  /// 是否转码视频
        bool        transcode_audio = false; /// 是否转码音频（暂不支持）

        EncoderConfig video_config; /// 视频编码配置

        /// 硬件加速配置
        bool                  enable_hardware_decode = false; /// 是否启用硬件解码
        bool                  enable_hardware_encode = false; /// 是否启用硬件编码
        HardwareContext::Type hw_type                = HardwareContext::Type::None;
    };

    /// 转码统计
    struct TranscodeStats
    {
        int     input_packets = 0;   /// 输入包数
        int     output_frames = 0;   /// 输出帧数
        int     video_frames  = 0;   /// 视频帧数
        int     audio_packets = 0;   /// 音频包数
        double  duration      = 0.0; /// 处理时长
        int64_t start_pts     = 0;   /// 开始PTS
        int64_t end_pts       = 0;   /// 结束PTS
    };

public:
    VideoTranscoder();
    ~VideoTranscoder();

public:
    /// 设置转码参数
    void setParams(const TranscodeParams& params);

    /// 设置进度回调
    void setProgressCallback(TranscodeProgressCallback callback);

    /// 执行转码
    bool transcode();

    /// 获取统计信息
    const TranscodeStats& getStats() const
    {
        return stats_;
    }

private:
    /// 初始化解码器
    bool initDecoder();

    /// 初始化编码器
    bool initEncoder();

    /// 计算PTS
    bool calculatePTS();

    /// 定位到开始时间
    bool seekToStart();

    /// 处理视频帧
    bool processVideoFrame(AVFrame* frame);

    /// 刷新编码器
    bool flushEncoder();

private:
    TranscodeParams           params_;
    TranscodeStats            stats_;
    TranscodeProgressCallback progress_callback_;

    /// 解封装和封装
    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<Muxer>   muxer_;

    /// 解码器和编码器
    VideoDecoder::Ptr             decoder_;
    std::unique_ptr<VideoEncoder> encoder_;

    /// 流信息
    AVStream* video_stream_  = nullptr;
    AVStream* audio_stream_  = nullptr;
    int       out_video_idx_ = -1;
    int       out_audio_idx_ = -1;

    /// PTS计算
    int64_t video_start_pts_ = 0;
    int64_t video_end_pts_   = 0;
    int64_t audio_start_pts_ = 0;

    /// 时间基
    AVRational input_time_base_{ 0, 0 };
    AVRational encoder_time_base_{ 0, 0 };
    AVRational mux_time_base_{ 0, 0 };

    /// 帧率控制
    double input_fps_  = 0.0;
    double output_fps_ = 0.0;
};
