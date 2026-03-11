#include "XDemuxTask.h"
#include "VideoDecoder.h"
#include "XVideoView.h"
#include <iostream>
#include <chrono>
#include <thread>

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        /// 创建解封装任务
        XDemuxTask demux_task;
        if (!demux_task.open(R"(.\assert\v1080.mp4)"))
        {
            return -1;
        }

        /// 设置队列大小
        demux_task.setMaxQueueSize(50);

        /// 启动解封装线程
        demux_task.start();

        /// 获取视频流
        AVStream* video_stream = demux_task.getVideoStream();
        if (!video_stream)
        {
            std::cerr << "未找到视频流" << std::endl;
            return -1;
        }

        /// 创建解码器
        auto codec_params = demux_task.getCodecParameters(video_stream->index);

        DecoderConfig config;
        config.codec_id     = video_stream->codecpar->codec_id;
        config.thread_count = 16;

        /// 硬件加速配置
        config.hardware.enable               = true;
        config.hardware.auto_select          = true;
        config.hardware.preferred_type       = HardwareContext::Type::D3D11VA;
        config.hardware.transfer_to_software = true;

        VideoDecoder decoder(config);
        decoder.set_parameters(codec_params->get());
        decoder.open();

        /// 创建显示窗口
        auto view = XVideoView::create();
        if (!view)
        {
            std::cerr << "无法创建显示窗口" << std::endl;
            return -1;
        }

        bool is_init     = false;
        auto begin_time  = std::chrono::steady_clock::now();
        int  frame_count = 0;

        decoder.set_frame_callback(
                [&](AVFrame* frame, bool is_hw)
                {
                    if (!is_init && frame->width > 0 && frame->height > 0)
                    {
                        is_init = true;
                        view->init(frame->width, frame->height, static_cast<XVideoView::Format>(frame->format));
                        std::cout << "\n窗口初始化: " << frame->width << "x" << frame->height << std::endl;
                    }

                    if (is_init)
                    {
                        view->drawFrame(frame);
                        frame_count++;

                        auto now     = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - begin_time).count();
                        if (elapsed >= 1)
                        {
                            std::cout << "\r解码 FPS: " << frame_count << std::flush;
                            frame_count = 0;
                            begin_time  = now;
                        }
                    }
                });

        /// 处理数据包
        std::vector<AVFrame*> frames;

        std::cout << "\n开始解码播放..." << std::endl;

        while (true)
        {
            auto pkt = demux_task.getPacketBlocking(100);
            if (!pkt)
            {
                if (demux_task.getQueueSize() == 0 && demux_task.isRunning() == false)
                {
                    std::cout << "\n文件读取完成" << std::endl;
                    break;
                }
                continue;
            }

            if ((*pkt)->stream_index == video_stream->index)
            {
                decoder.decode_packet(*pkt, frames);
                for (auto frame : frames)
                {
                    av_frame_free(&frame);
                }
                frames.clear();
            }
        }

        /// 刷新解码器
        std::cout << "\n刷新解码器..." << std::endl;
        decoder.flush(frames);
        for (auto frame : frames)
        {
            av_frame_free(&frame);
        }

        demux_task.stop();
        demux_task.wait();

        decoder.print_stats();
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
    }

    std::cout << "\n按回车键退出..." << std::endl;
    getchar();
    return 0;
}
