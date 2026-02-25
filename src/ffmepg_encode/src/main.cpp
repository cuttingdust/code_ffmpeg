#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    std::string filename = "400_300_25";
    AVCodecID   codec_id = AV_CODEC_ID_H264;

    if (argc > 1)
    {
        std::string codec = argv[1];
        if (codec == "h265" || codec == "hevc")
        {
            codec_id = AV_CODEC_ID_HEVC;
        }
    }

    auto codec = avcodec_find_encoder(codec_id);
    if (!codec)
    {
        std::cerr << "codec not find!" << std::endl;
        return -1;
    }

    auto c = avcodec_alloc_context3(codec);
    if (!c)
    {
        std::cerr << "avcodec_alloc_context3 failed!" << std::endl;
        return -1;
    }

    /// 设置参数
    c->width        = 400;
    c->height       = 300;
    c->time_base    = { .num = 1, .den = 25 };
    c->pix_fmt      = AV_PIX_FMT_YUV420P;
    c->thread_count = 16;

    /// 打开编码器
    int re = avcodec_open2(c, codec, NULL);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf));
        std::cerr << "avcodec_open2 failed! " << buf << std::endl;
        return -1;
    }

    ///  准备帧和包
    auto frame    = av_frame_alloc();
    frame->width  = c->width;
    frame->height = c->height;
    frame->format = c->pix_fmt;
    re            = av_frame_get_buffer(frame, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf));
        std::cerr << "av_frame_get_buffer failed! " << buf << std::endl;
        return -1;
    }

    auto          pkt = av_packet_alloc();
    std::ofstream ofs(filename + (codec_id == AV_CODEC_ID_H264 ? ".h264" : ".h265"), std::ios::binary);

    int frame_count  = 0;
    int total_frames = 250;

    // 编码循环
    for (int i = 0; i < total_frames; i++)
    {
        // 生成测试数据
        for (int y = 0; y < c->height; y++)
            for (int x = 0; x < c->width; x++)
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;

        for (int y = 0; y < c->height / 2; y++)
            for (int x = 0; x < c->width / 2; x++)
            {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }

        frame->pts = i;

        // 发送帧
        re = avcodec_send_frame(c, frame);
        if (re < 0)
        {
            char buf[1024] = { 0 };
            av_strerror(re, buf, sizeof(buf));
            std::cerr << "send_frame failed! " << buf << std::endl;
            break;
        }

        // 接收所有可用包
        while (re >= 0)
        {
            re = avcodec_receive_packet(c, pkt);
            if (re == AVERROR(EAGAIN) || re == AVERROR_EOF)
            {
                break;
            }
            if (re < 0)
            {
                char buf[1024] = { 0 };
                av_strerror(re, buf, sizeof(buf));
                std::cerr << "receive_packet failed! " << buf << std::endl;
                break;
            }

            frame_count++;
            std::cout << "帧 " << frame_count << " 大小: " << pkt->size << std::endl;
            ofs.write((char *)pkt->data, pkt->size);
            av_packet_unref(pkt);
        }
    }

    // 重要：flush编码器！
    std::cout << "Flushing encoder..." << std::endl;
    avcodec_send_frame(c, nullptr);
    while (true)
    {
        re = avcodec_receive_packet(c, pkt);
        if (re == AVERROR_EOF)
            break;
        if (re < 0)
            break;

        frame_count++;
        std::cout << "Flush帧 " << frame_count << " 大小: " << pkt->size << std::endl;
        ofs.write((char *)pkt->data, pkt->size);
        av_packet_unref(pkt);
    }

    std::cout << "总帧数: " << frame_count << " (发送了" << total_frames << "帧)" << std::endl;

    // 清理
    ofs.close();
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&c);

    getchar();
    return 0;
}
