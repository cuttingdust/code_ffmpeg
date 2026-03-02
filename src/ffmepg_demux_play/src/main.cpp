#include "VideoDecoder.h"
#include "XVideoView.h" // 需要添加这个头文件

#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
}

extern long long NowMs();

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
    int       video_stream_idx = -1;
    int       audio_stream_idx = -1;
    AVStream *video_stream     = nullptr;

    for (int i = 0; i < ic->nb_streams; i++)
    {
        AVStream *stream = ic->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1)
        {
            video_stream_idx = i;
            video_stream     = stream;

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

    /// 创建显示窗口
    auto view = XVideoView::create();
    if (!view)
    {
        std::cerr << "无法创建显示窗口" << std::endl;
        return -1;
    }

    /// 配置解码器
    DecoderConfig config;
    config.codec_id     = video_stream->codecpar->codec_id;
    config.thread_count = 16;

    /// 硬件加速配置
    config.hardware.enable               = true;
    config.hardware.auto_select          = true;
    config.hardware.preferred_type       = HardwareContext::Type::D3D11VA;
    config.hardware.transfer_to_software = true;

    config.print();

    /// 创建解码器
    VideoDecoder decoder(config);

    /// 从流设置编码参数
    if (!decoder.set_parameters_from_stream(video_stream))
    {
        std::cerr << "设置编码参数失败" << std::endl;
        return -1;
    }

    /// 窗口初始化标志
    bool   is_init_win = false;
    double last_pts    = 0;

    /// 设置帧回调
    decoder.set_frame_callback(
            [&](AVFrame *frame, bool is_hw)
            {
                if (!is_init_win && frame->width > 0 && frame->height > 0)
                {
                    is_init_win = true;
                    view->init(frame->width, frame->height, (XVideoView::Format)frame->format);
                    std::cout << "\n窗口初始化: " << frame->width << "x" << frame->height << " 格式: " << frame->format
                              << " 硬件帧: " << (is_hw ? "是" : "否") << std::endl;
                }

                if (is_init_win)
                {
                    // 根据 PTS 控制显示时间
                    if (frame->pts != AV_NOPTS_VALUE)
                    {
                        double pts_seconds = frame->pts * av_q2d(video_stream->time_base);

                        // 计算需要等待的时间
                        if (last_pts > 0)
                        {
                            double wait_seconds = pts_seconds - last_pts;
                            if (wait_seconds > 0 && wait_seconds < 1.0)
                            {
                                int wait_ms = (int)(wait_seconds * 1000);
                                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
                            }
                        }
                        last_pts = pts_seconds;
                    }

                    // 显示帧
                    view->drawFrame(frame);
                }
            });

    /// 打开解码器
    std::cout << "\n打开解码器..." << std::endl;
    decoder.open();

    /// 读取数据包循环
    AVPacket              *pkt = av_packet_alloc();
    std::vector<AVFrame *> frames;

    int  video_frame_count = 0;
    int  keyframe_count    = 0;
    auto begin             = NowMs();

    std::cout << "\n开始解码播放..." << std::endl;

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
            video_frame_count++;

            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                keyframe_count++;
            }

            /// 实时显示进度
            auto now = NowMs();
            if (now - begin >= 100)
            {
                std::cout << "\r视频帧: " << video_frame_count << " 关键帧: " << keyframe_count << " PTS:" << pkt->pts
                          << " DTS:" << pkt->dts << " 大小:" << pkt->size << " "
                          << ((pkt->flags & AV_PKT_FLAG_KEY) ? "[I]" : "[P/B]") << std::flush;
                begin = now;
            }

            /// 发送到解码器
            try
            {
                decoder.decode_packet(pkt, frames);

                /// 清理临时帧（回调已经处理了显示，这里只需要释放内存）
                for (auto frame : frames)
                {
                    av_frame_free(&frame);
                }
                frames.clear();
            }
            catch (const std::exception &e)
            {
                std::cerr << "\n解码错误: " << e.what() << std::endl;
            }
        }
        /// 处理音频包
        else if (pkt->stream_index == audio_stream_idx)
        {
            // audio_frame_count++;
            // 这里可以添加音频处理逻辑
        }

        av_packet_unref(pkt);
    }

    /// 刷新解码器
    std::cout << "\n\n刷新解码器..." << std::endl;

    try
    {
        int flush_count = decoder.flush(frames);
        for (auto frame : frames)
        {
            av_frame_free(&frame);
        }
        std::cout << "刷新了 " << flush_count << " 帧" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "刷新错误: " << e.what() << std::endl;
    }

    /// 打印统计信息
    std::cout << "\n========== 播放统计 ==========" << std::endl;
    std::cout << "总视频包: " << video_frame_count << std::endl;
    std::cout << "关键帧数: " << keyframe_count << std::endl;
    if (keyframe_count > 0)
    {
        std::cout << "关键帧间隔: " << video_frame_count / keyframe_count << " 帧" << std::endl;
    }
    decoder.print_stats();

    /// 清理
    av_packet_free(&pkt);
    avformat_close_input(&ic);

    std::cout << "\n播放完成，按回车键退出..." << std::endl;
    getchar();
    return 0;
}
