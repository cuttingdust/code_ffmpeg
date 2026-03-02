#include <iostream>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

void PrintErr(int err)
{
    char buf[1024] = { 0 };
    av_strerror(err, buf, sizeof(buf) - 1);
    std::cerr << buf << std::endl;
}

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

    /// 打开媒体文件
    const char *url = R"(.\assert\v1080.mp4)";

    /// 解封装输入上下文
    AVFormatContext *ic = nullptr;
    int              re = avformat_open_input(&ic, url, NULL, NULL);
    CHECK_ERR(re)

    /// 获取媒体信息
    re = avformat_find_stream_info(ic, NULL);
    CHECK_ERR(re);

    /// 打印封装信息
    av_dump_format(ic, 0, url, 0);

    /// 查找视频流和音频流
    int video_stream_idx = -1;
    int audio_stream_idx = -1;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVStream *stream = ic->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1)
        {
            video_stream_idx = i;
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
            std::cout << "\n=========音频流信息=========" << std::endl;
            std::cout << "索引: " << i << std::endl;
            std::cout << "编码器: " << avcodec_get_name(stream->codecpar->codec_id) << std::endl;
            std::cout << "采样率: " << stream->codecpar->sample_rate << " Hz" << std::endl;
            std::cout << "声道数: " << stream->codecpar->ch_layout.nb_channels << std::endl;
        }
    }

    if (video_stream_idx == -1)
    {
        std::cerr << "未找到视频流" << std::endl;
        return -1;
    }

    /// 为视频流创建解码器
    const AVCodec *video_codec = avcodec_find_decoder(ic->streams[video_stream_idx]->codecpar->codec_id);
    if (!video_codec)
    {
        std::cerr << "找不到视频解码器" << std::endl;
        return -1;
    }

    AVCodecContext *video_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_ctx, ic->streams[video_stream_idx]->codecpar);

    re = avcodec_open2(video_ctx, video_codec, NULL);
    CHECK_ERR(re);

    AVPacket *pkt   = av_packet_alloc();
    AVFrame  *frame = av_frame_alloc();

    int video_frame_count = 0;
    int audio_frame_count = 0;

    std::cout << "\n开始解封装..." << std::endl;

    /// 读取数据包循环
    while (true)
    {
        re = av_read_frame(ic, pkt);

        if (re == AVERROR_EOF)
        {
            std::cout << "\n文件读取完成" << std::endl;
            break;
        }
        else if (re < 0)
        {
            PrintErr(re);
            break;
        }

        /// 处理视频包
        if (pkt->stream_index == video_stream_idx)
        {
            std::cout << "\r";
            std::cout << "视频包 #" << ++video_frame_count << " PTS:" << pkt->pts << " DTS:" << pkt->dts
                      << " 大小:" << pkt->size << " 关键帧:" << ((pkt->flags & AV_PKT_FLAG_KEY) ? "是" : "否");

            // std::cout << std::endl;
            std::cout << std::flush;

            /// 发送到解码器
            re = avcodec_send_packet(video_ctx, pkt);
            if (re < 0)
            {
                PrintErr(re);
                continue;
            }

            /// 接收解码后的帧
            while (re >= 0)
            {
                re = avcodec_receive_frame(video_ctx, frame);
                if (re == AVERROR(EAGAIN) || re == AVERROR_EOF)
                {
                    break;
                }
                else if (re < 0)
                {
                    PrintErr(re);
                    break;
                }

                /// 这里可以处理解码后的视频帧
                /// 比如显示或保存
                /// frame->data[0], frame->linesize[0] 等
            }
        }
        /// 处理音频包
        else if (pkt->stream_index == audio_stream_idx)
        {
            audio_frame_count++;
        }

        av_packet_unref(pkt);

        /// 稍微暂停一下，避免输出太快
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /// 刷新解码器
    std::cout << "\n\n刷新解码器..." << std::endl;

    avcodec_send_packet(video_ctx, NULL);
    while (avcodec_receive_frame(video_ctx, frame) >= 0)
    {
        video_frame_count++;
        /// 处理最后一帧
    }

    std::cout << "总计: 视频包 " << video_frame_count << " 个, 音频包 " << audio_frame_count << " 个" << std::endl;

    /// 清理
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&video_ctx);
    avformat_close_input(&ic);

    std::cout << "\n按回车键退出..." << std::endl;
    getchar();
    return 0;
}
