#include <iostream>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

/**
 * @brief 打印FFmpeg错误信息
 * @param err FFmpeg错误码（负数）
 */
void PrintErr(int err)
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

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "==========================================" << std::endl;

    // ==================== 第一部分：打开输入文件（解封装） ====================

    /// 输入文件路径
    const char *in_url = R"(.\assert\v1080.mp4)";

    /// 解封装输入上下文
    AVFormatContext *ic = nullptr; // 输入格式上下文

    /// 打开输入文件，自动探测格式
    int re = avformat_open_input(&ic, in_url, NULL, NULL);
    CHECK_ERR(re)

    /// 获取媒体流信息（如时长、比特率等）
    re = avformat_find_stream_info(ic, NULL);
    CHECK_ERR(re);

    /// 打印输入文件信息（用于调试）
    av_dump_format(ic, 0, in_url, 0);

    /// 查找视频流和音频流的索引
    int       video_stream_idx = -1;
    int       audio_stream_idx = -1;
    AVStream *video_stream     = nullptr; // 视频流指针
    AVStream *audio_stream     = nullptr; // 音频流指针

    /// 遍历所有流，找到视频流和音频流
    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVStream *stream = ic->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1)
        {
            video_stream_idx = i;
            video_stream     = stream;

            /// 打印视频流详细信息
            std::cout << "\n=========视频流信息=========" << std::endl;
            std::cout << "索引: " << i << std::endl;
            std::cout << "编码器: " << avcodec_get_name(stream->codecpar->codec_id) << std::endl;
            std::cout << "分辨率: " << stream->codecpar->width << "x" << stream->codecpar->height << std::endl;
            std::cout << "像素格式: " << av_get_pix_fmt_name((AVPixelFormat)stream->codecpar->format) << std::endl;
            std::cout << "帧率: " << av_q2d(stream->avg_frame_rate) << " fps" << std::endl;
            std::cout << "时间基: " << stream->time_base.num << "/" << stream->time_base.den << std::endl;
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1)
        {
            audio_stream_idx = i;
            audio_stream     = stream;

            /// 打印音频流详细信息
            std::cout << "\n=========音频流信息=========" << std::endl;
            std::cout << "索引: " << i << std::endl;
            std::cout << "编码器: " << avcodec_get_name(stream->codecpar->codec_id) << std::endl;
            std::cout << "采样率: " << stream->codecpar->sample_rate << " Hz" << std::endl;
            std::cout << "声道数: " << stream->codecpar->ch_layout.nb_channels << std::endl;
        }
    }

    /// 检查是否找到视频流
    if (video_stream_idx == -1)
    {
        std::cerr << "未找到视频流" << std::endl;
        return -1;
    }

    // ==================== 第二部分：创建输出文件（封装） ====================

    /// 输出文件路径
    const char *out_url = R"(.\assert\test_mux.mp4)";

    /// 输出格式上下文
    AVFormatContext *ec = nullptr;

    /// 分配输出上下文，根据输出文件后缀名自动猜测封装格式（MP4）
    re = avformat_alloc_output_context2(&ec, NULL, NULL, out_url);
    CHECK_ERR(re);

    /// 在输出文件中添加视频流和音频流
    /// avformat_new_stream 会创建新的流并添加到输出上下文中
    auto mvs = avformat_new_stream(ec, NULL); /// 视频流
    auto mas = avformat_new_stream(ec, NULL); /// 音频流

    /// 打开输出文件的IO
    /// ec->pb 是AVIOContext，用于文件读写
    re = avio_open(&ec->pb, out_url, AVIO_FLAG_WRITE);
    CHECK_ERR(re);

    /// 设置输出流的编码参数
    /// 这里直接从输入流复制参数，实现"转封装"（remuxing）
    /// 注意：转封装不改变编码格式，只是改变封装格式

    /// 设置视频流参数
    if (video_stream)
    {
        /// 时间基与原视频一致，保持PTS/DTS的正确性
        mvs->time_base = video_stream->time_base;

        /// 从解封装的视频流复制编码参数到输出视频流
        /// 这包括编码器ID、分辨率、像素格式等信息
        avcodec_parameters_copy(mvs->codecpar, video_stream->codecpar);
    }

    /// 设置音频流参数
    if (audio_stream)
    {
        mas->time_base = audio_stream->time_base;

        /// 从解封装的音频流复制编码参数到输出音频流
        /// 这包括编码器ID、采样率、声道数等信息
        avcodec_parameters_copy(mas->codecpar, audio_stream->codecpar);
    }

    /// 写入文件头
    /// avformat_write_header 会写入封装格式的头部信息
    /// 对于MP4，这包括ftyp、moov等box
    re = avformat_write_header(ec, NULL);
    CHECK_ERR(re);

    /// 打印输出文件信息（用于调试）
    av_dump_format(ec, 0, out_url, 1);

    ////////////////////////////////////////////////////////////////////////////////////
    /// 截取10 ~ 20 秒之间的音频视频 取多不取少
    /// 假定 9 11秒有关键帧 我们取第9秒
    double begin_sec = 10.0; /// 截取开始时间
    double end_sec   = 20.0; /// 截取结束时间

    long long begin_pts       = 0;
    long long end_pts         = 0;
    long long begin_audio_pts = 0; /// 音频的开始时间

    /// 换算成pts 换算成输入ic的pts，以视频流为准
    if (video_stream && video_stream->time_base.num > 0)
    {
        /// sec /timebase = pts
        /// pts =  sec/(num/den) = sec* (den/num)
        double t  = (double)video_stream->time_base.den / (double)video_stream->time_base.num; /// den分母/num分子
        begin_pts = begin_sec * t;
        end_pts   = end_sec * t;
    }

    if (audio_stream && audio_stream->time_base.num > 0)
    {
        begin_audio_pts = begin_sec * ((double)audio_stream->time_base.den / (double)audio_stream->time_base.num);
    }

    /// seek输入媒体 移动到第十秒的关键帧位置
    if (video_stream)
    {
        re = av_seek_frame(ic, video_stream->index, begin_pts,
                           AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD); /// 向后关键帧
    }
    CHECK_ERR(re);


    /// 分配数据包和帧结构
    AVPacket *pkt   = av_packet_alloc(); /// 用于存储读取的数据包
    AVFrame  *frame = av_frame_alloc();  /// 这里实际上没用到，但保留以备后续可能

    /// 统计变量
    int video_frame_count = 0;
    int audio_frame_count = 0;

    std::cout << "\n开始封装..." << std::endl;

    // ==================== 第三部分：读取并写入数据包 ====================

    /// 循环读取输入文件的数据包
    while (true)
    {
        /// 从输入文件读取一个数据包
        /// pkt 包含编码后的数据（H.264/AAC等）
        re = av_read_frame(ic, pkt);

        /// 处理文件结束
        if (re == AVERROR_EOF)
        {
            std::cout << "\n文件读取完成" << std::endl;
            break;
        }
        /// 处理读取错误
        else if (re < 0)
        {
            PrintErr(re);
            break;
        }

        AVStream *in_stream  = ic->streams[pkt->stream_index];
        AVStream *out_stream = nullptr;
        long long offset_pts = 0; /// 偏移pts，用于截断的开头pts运算

        /// 处理视频包
        if (pkt->stream_index == video_stream_idx && video_stream)
        {
            /// 显示进度信息（使用\r在同一行刷新）
            std::cout << "\r";
            std::cout << "视频包 #" << ++video_frame_count << " PTS:" << pkt->pts      /// 显示时间戳
                      << " DTS:" << pkt->dts                                           /// 解码时间戳
                      << " 大小:" << pkt->size                                         /// 数据包大小
                      << " 关键帧:" << ((pkt->flags & AV_PKT_FLAG_KEY) ? "是" : "否"); /// 是否为关键帧

            std::cout << std::flush;

            /// 超过第20秒退出，只存10~20秒
            if (pkt->pts > end_pts)
            {
                break;
            }
            out_stream = ec->streams[0];
            offset_pts = begin_pts;
        }
        /// 处理音频包
        else if (pkt->stream_index == audio_stream_idx && audio_stream)
        {
            audio_frame_count++;
            /// 超过第20秒退出，只存10~20秒
            out_stream = ec->streams[1];
            offset_pts = begin_audio_pts;
        }


        /// 重新计算pts dts duration
        /// `a * bq（输入basetime） / cq（输出basetime）`
        if (out_stream)
        {
            pkt->pts = av_rescale_q_rnd(pkt->pts - offset_pts, in_stream->time_base, out_stream->time_base,
                                        (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

            pkt->dts = av_rescale_q_rnd(pkt->dts - offset_pts, in_stream->time_base, out_stream->time_base,
                                        (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

            pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
        }
        pkt->pos = -1;

        /// 写入音视频帧 会清理pkt
        re = av_interleaved_write_frame(ec, pkt);
        if (re != 0)
        {
            PrintErr(re);
        }

        /// 释放数据包，避免内存泄漏
        /// 注意：av_interleaved_write_frame 内部会处理数据包内容，所以我们仍需要unref
        av_packet_unref(pkt);

        /// 可选：稍微暂停一下，避免输出太快
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // ==================== 第四部分：收尾和清理 ====================

    /// 写入文件尾部（moov等索引信息）
    /// 对于MP4文件，这非常重要，因为索引信息在文件尾部
    re = av_write_trailer(ec);
    CHECK_ERR(re)

    /// 关闭输入文件
    avformat_close_input(&ic);

    /// 关闭输出文件的IO
    avio_closep(&ec->pb);

    /// 释放输出上下文
    avformat_close_input(&ec);
    ec = nullptr;

    /// 打印统计信息
    std::cout << "\n总计: 视频包 " << video_frame_count << " 个, 音频包 " << audio_frame_count << " 个" << std::endl;

    std::cout << "\n按回车键退出..." << std::endl;
    getchar();
    return 0;
}
