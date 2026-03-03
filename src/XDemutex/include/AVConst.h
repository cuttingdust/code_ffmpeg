#pragma once

#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixfmt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
}

///前向声明
struct AVFrame;
struct AVPacket;
struct AVDictionary;
enum AVCodecID;

/// ==================== 错误处理 ====================

/**
 * @brief 打印FFmpeg错误信息
 * @param err FFmpeg错误码（负数）
 */
inline void PrintErr(int err)
{
    char buf[1024] = { 0 };
    av_strerror(err, buf, sizeof(buf) - 1);
    std::cerr << buf << std::endl;
}

/**
 * @brief 错误检查宏，如果错误则打印并退出
 */
#define CHECK_ERR(err) \
    if (err < 0)       \
    {                  \
        PrintErr(err); \
        getchar();     \
        return -1;     \
    }

/**
 * @brief 错误检查宏（用于函数内部，抛出异常）
 */
#define THROW_ERR(err)                            \
    if (err < 0)                                  \
    {                                             \
        PrintErr(err);                            \
        throw std::runtime_error("FFmpeg error"); \
    }

/// ==================== 时间工具 ====================

/**
 * @brief 将秒转换为时间戳
 * @param seconds 秒数
 * @param time_base 时间基
 * @return 时间戳
 */
inline int64_t seconds_to_pts(double seconds, AVRational time_base)
{
    if (time_base.num == 0 || time_base.den == 0)
        return 0;
    return seconds * time_base.den / time_base.num;
}

/**
 * @brief 将时间戳转换为秒
 * @param pts 时间戳
 * @param time_base 时间基
 * @return 秒数
 */
inline double pts_to_seconds(int64_t pts, AVRational time_base)
{
    if (time_base.num == 0 || time_base.den == 0)
        return 0;
    return pts * time_base.num / (double)time_base.den;
}

/**
 * @brief 获取当前时间（毫秒）
 */
inline long long NowMs()
{
    return av_gettime() / 1000;
}

/// ==================== 格式工具 ====================

/**
 * @brief 获取像素格式名称
 */
inline const char* GetPixFmtName(AVPixelFormat fmt)
{
    return av_get_pix_fmt_name(fmt);
}

/**
 * @brief 获取编码器名称
 */
inline const char* GetCodecName(AVCodecID id)
{
    return avcodec_get_name(id);
}

/**
 * @brief 打印AVRational
 */
inline std::ostream& operator<<(std::ostream& os, const AVRational& r)
{
    os << r.num << "/" << r.den;
    return os;
}

/// ==================== 智能指针删除器 ====================

/**
 * @brief AVFrame 智能指针删除器
 */
struct AVFrameDeleter
{
    void operator()(AVFrame* f) const
    {
        av_frame_free(&f);
    }
};
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

/**
 * @brief AVPacket 智能指针删除器
 */
struct AVPacketDeleter
{
    void operator()(AVPacket* p) const
    {
        av_packet_free(&p);
    }
};
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

/**
 * @brief AVCodecContext 智能指针删除器
 */
struct AVCodecContextDeleter
{
    void operator()(AVCodecContext* c) const
    {
        avcodec_free_context(&c);
    }
};
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

/**
 * @brief AVFormatContext 智能指针删除器（输入）
 */
struct AVFormatInputDeleter
{
    void operator()(AVFormatContext* f) const
    {
        avformat_close_input(&f);
    }
};
using AVFormatInputPtr = std::unique_ptr<AVFormatContext, AVFormatInputDeleter>;

/**
 * @brief AVFormatContext 智能指针删除器（输出）
 */
struct AVFormatOutputDeleter
{
    void operator()(AVFormatContext* f) const
    {
        if (f && f->pb)
        {
            avio_closep(&f->pb);
        }
        avformat_free_context(f);
    }
};
using AVFormatOutputPtr = std::unique_ptr<AVFormatContext, AVFormatOutputDeleter>;
