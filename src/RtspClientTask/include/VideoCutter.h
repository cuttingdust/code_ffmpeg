#pragma once

#include "Demuxer.h"
#include "Muxer.h"
#include "PacketWrapper.h"
#include <string>
#include <functional>

/// 截取进度回调类型
using CutProgressCallback = std::function<void(int video_packets, int audio_packets, int64_t current_pts)>;

/// ==================== 视频截取器类 ====================
class VideoCutter
{
public:
    /// 截取参数
    struct CutParams
    {
        std::string input_file;        /// 输入文件路径
        std::string output_file;       /// 输出文件路径
        double      start_time = 0.0;  /// 开始时间（秒）
        double      end_time   = 0.0;  /// 结束时间（秒）
        bool        copy_video = true; /// 是否复制视频流
        bool        copy_audio = true; /// 是否复制音频流
    };

public:
    VideoCutter();
    ~VideoCutter();

public:
    /// 设置截取参数
    void setParams(const CutParams& params);

    /// 获取截取参数
    const CutParams& getParams() const
    {
        return params_;
    }

    /// 设置进度回调
    void setProgressCallback(CutProgressCallback callback);

    /// 执行截取
    bool cut();

    /// 获取截取统计
    struct CutStats
    {
        int     video_packets = 0;
        int     audio_packets = 0;
        int64_t start_pts     = 0;
        int64_t end_pts       = 0;
        double  duration      = 0.0;
    };

    const CutStats& getStats() const
    {
        return stats_;
    }

private:
    /// 计算PTS
    bool calculatePTS();

    /// 定位到开始时间
    bool seekToStart();

    /// 处理视频包
    bool processVideoPacket(AVPacket* pkt, AVStream* in_stream, int out_stream_index, int64_t offset_pts);

    /// 处理音频包
    bool processAudioPacket(AVPacket* pkt, AVStream* in_stream, int out_stream_index, int64_t offset_pts);

private:
    CutParams           params_;
    CutStats            stats_;
    CutProgressCallback progress_callback_;

    Demuxer::Ptr demuxer_;
    Muxer::Ptr   muxer_;

    int64_t video_start_pts_ = 0;
    int64_t video_end_pts_   = 0;
    int64_t audio_start_pts_ = 0;

    AVStream* video_stream_  = nullptr;
    AVStream* audio_stream_  = nullptr;
    int       out_video_idx_ = -1;
    int       out_audio_idx_ = -1;
};
