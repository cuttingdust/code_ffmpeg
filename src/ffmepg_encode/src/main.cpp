#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
}

int main(int argc, char *argv[])
{
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
    if (codec_id == AV_CODEC_ID_H264)
    {
        filename += ".h264";
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        filename += ".h265";
    }
    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);

    /// 1 找到编码器  AV_CODEC_ID_HEVC(H265)
    auto codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        std::cerr << "codec not find!" << std::endl;
        return -1;
    }

    /// 2 编码上下文
    auto c = avcodec_alloc_context3(codec);
    if (!c)
    {
        std::cerr << "avcodec_alloc_context3 failed!" << std::endl;
        return -1;
    }

    ///3 设定上下文参数
    c->width  = 400; /// 视频宽高
    c->height = 300;

    /// 帧时间戳的时间单位  pts*time_base = 播放时间（秒）
    c->time_base = { .num = 1, .den = 25 }; /// 分数 1/25

    c->pix_fmt      = AV_PIX_FMT_YUV420P; /// 元数据像素格式，与编码算法相关
    c->thread_count = 16;                 /// 编码线程数，可以通过调用系统接口获取cpu核心数量

    ///4 打开编码上下文
    int re = avcodec_open2(c, codec, NULL);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        std::cerr << "avcodec_open2 failed!" << buf << std::endl;
        return -1;
    }
    std::cout << "avcodec_open2 success!" << std::endl;

    /// 创建好AVFrame空间 未压缩数据
    auto frame    = av_frame_alloc();
    frame->width  = c->width;
    frame->height = c->height;
    frame->format = c->pix_fmt;
    re            = av_frame_get_buffer(frame, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        std::cerr << "avcodec_open2 failed!" << buf << std::endl;
        return -1;
    }
    auto pkt = av_packet_alloc();
    /// 十秒视频 250帧
    for (int i = 0; i < 250; i++)
    {
        /// 生成AVFrame 数据 每帧数据不同
        /// Y
        for (int y = 0; y < c->height; y++)
        {
            for (int x = 0; x < c->width; x++)
            {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        /// UV
        for (int y = 0; y < c->height / 2; y++)
        {
            for (int x = 0; x < c->width / 2; x++)
            {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }
        frame->pts = i; /// 显示的时间

        /// 发送未压缩帧到线程中压缩
        re = avcodec_send_frame(c, frame);
        if (re != 0)
        {
            break;
        }

        while (re >= 0) /// 返回多帧
        {
            /// 接收压缩帧，一遍前几次调用返回空（缓冲，立刻返回，编码未完成）
            /// 编码是在独立的线程中
            /// 每次调用会重新分配pkt中的空间
            re = avcodec_receive_packet(c, pkt);
            if (re == AVERROR(EAGAIN) || re == AVERROR_EOF)
            {
                break;
            }

            if (re < 0)
            {
                char buf[1024] = { 0 };
                av_strerror(re, buf, sizeof(buf) - 1);
                std::cerr << "avcodec_receive_packet failed!" << buf << std::endl;
                break;
            }
            std::cout << pkt->size << " " << std::flush;
            ofs.write((char *)pkt->data, pkt->size);
            av_packet_unref(pkt);
        }
    }
    ofs.close();
    av_packet_free(&pkt);
    av_frame_free(&frame);

    /// 释放编码器上下文
    avcodec_free_context(&c);

    getchar();
    return 0;
}
