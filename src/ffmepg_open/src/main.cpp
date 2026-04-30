#include <iostream>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#define TEST_VIDEO R"(assert\output.mp4)"

static double r2d(AVRational r)
{
    return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}


int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    ///解封装上下文
    AVFormatContext *ic   = NULL;
    constexpr auto   path = TEST_VIDEO;
    /// 参数设置
    AVDictionary *opts = NULL;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0); /// 设置rtsp流已tcp协议打开
    av_dict_set(&opts, "max_delay", "500", 0);      /// 网络延时时间


    int ret = avformat_open_input(&ic, path,
                                  0,    /// 0表示自动选择解封器
                                  &opts ///参数设置，比如rtsp的延时时间
    );
    if (ret != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        std::cout << "open " << path << " failed! :" << buf << std::endl;
        getchar();
        return -1;
    }
    std::cout << "open " << path << " success! " << std::endl;

    /// 获取流信息
    ret = avformat_find_stream_info(ic, nullptr);
    if (ret < 0)
    {
        char buf[1024] = { 0 };
        av_strerror(ret, buf, sizeof(buf) - 1);
        std::cout << "find stream info failed! :" << buf << std::endl;
        getchar();
        return -1;
    }

    /// 打印视频流详细信息
    av_dump_format(ic, 0, path, 0);

    std::cout << "==========================================" << std::endl;

    /// 总时长 毫秒
    int totalMs = ic->duration / (AV_TIME_BASE / 1000);
    std::cout << "totalMs = " << totalMs << std::endl;


    {
        /// 音视频索引，读取时区分音视频
        int videoStream = 0;
        int audioStream = 1;

        /// 获取音视频流信息 （遍历，函数获取）
        for (int i = 0; i < ic->nb_streams; i++)
        {
            AVStream *as = ic->streams[i];
            std::cout << "codec_id = " << as->codecpar->codec_id << std::endl;
            std::cout << "format = " << as->codecpar->format << std::endl;

            /// 音频 AVMEDIA_TYPE_AUDIO
            if (as->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                audioStream = i;
                std::cout << i << "音频信息" << std::endl;
                std::cout << "sample_rate = " << as->codecpar->sample_rate << std::endl;
                /// AVSampleFormat;
                std::cout << "channels = " << as->codecpar->ch_layout.nb_channels << std::endl;
                /// 一帧数据？？ 单通道样本数
                std::cout << "frame_size = " << as->codecpar->frame_size << std::endl;
                /// 1024 * 2 * 2 = 4096  fps = sample_rate/frame_size
            }
            /// 视频 AVMEDIA_TYPE_VIDEO
            else if (as->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                videoStream = i;
                std::cout << i << "视频信息" << std::endl;
                std::cout << "width=" << as->codecpar->width << std::endl;
                std::cout << "height=" << as->codecpar->height << std::endl;
                /// 帧率 fps 分数转换
                // std::cout << "video fps = " << r2d(as->avg_frame_rate) << std::endl;
                std::cout << "video fps = " << av_q2d(as->avg_frame_rate) << std::endl;
            }
        }

        /// 获取视频流 /// 比上面的for循环获取更简单，直接函数获取
        videoStream = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

        const AVCodec *vcodec = avcodec_find_decoder(ic->streams[videoStream]->codecpar->codec_id);
        if (!vcodec)
        {
            std::cout << "can't find the codec id " << ic->streams[videoStream]->codecpar->codec_id;
            getchar();
            return -1;
        }
        std::cout << "find the AVCodec " << ic->streams[videoStream]->codecpar->codec_id << std::endl;

        /// 创建解码器上下文呢
        AVCodecContext *vc = avcodec_alloc_context3(vcodec);
        /// 配置解码器上下文参数
        avcodec_parameters_to_context(vc, ic->streams[videoStream]->codecpar);
        /// 八线程解码
        vc->thread_count = 8;
        ///打开解码器上下文
        ret = avcodec_open2(vc, 0, 0);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            std::cout << "avcodec_open2  failed! :" << buf << std::endl;
            getchar();
            return -1;
        }
        std::cout << "video avcodec_open2 success!" << std::endl;

        //////////////////////////////////////////////////////////
        ///音频解码器打开
        const AVCodec *acodec = avcodec_find_decoder(ic->streams[audioStream]->codecpar->codec_id);
        if (!acodec)
        {
            std::cout << "can't find the codec id " << ic->streams[audioStream]->codecpar->codec_id;
            getchar();
            return -1;
        }
        std::cout << "find the AVCodec " << ic->streams[audioStream]->codecpar->codec_id << std::endl;
        /// 创建解码器上下文呢
        AVCodecContext *ac = avcodec_alloc_context3(acodec);

        /// 配置解码器上下文参数
        avcodec_parameters_to_context(ac, ic->streams[audioStream]->codecpar);
        /// 八线程解码
        ac->thread_count = 8;

        ///打开解码器上下文
        ret = avcodec_open2(ac, 0, 0);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            std::cout << "avcodec_open2  failed! :" << buf << std::endl;
            getchar();
            return -1;
        }
        std::cout << "audio avcodec_open2 success!" << std::endl;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////

        AVPacket *pkt   = av_packet_alloc();
        AVFrame  *frame = av_frame_alloc();

        /// 像素格式和尺寸转换上下文
        SwsContext    *vctx = NULL;
        unsigned char *rgb  = NULL;

        for (;;)
        {
            int re = av_read_frame(ic, pkt);
            if (re != 0)
            {
                /// 循环播放
                // std::cout << "==============================end==============================" << std::endl;
                // int       ms  = 3000; /// 三秒位置 根据时间基数（分数）转换
                // long long pos = (double)ms / (double)1000 * r2d(ic->streams[pkt->stream_index]->time_base);
                // av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
                // continue;
                break;
            }

            std::cout << "pkt->size = " << pkt->size << std::endl;

            /// 显示的时间
            std::cout << "pkt->pts = " << pkt->pts << std::endl;

            /// 解码时间
            std::cout << "pkt->dts = " << pkt->dts << std::endl;

            /// 转换为毫秒，方便做同步
            // std::cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000)
            //           << std::endl;
            //
            // std::cout << "pkt->pts ms = " << pkt->pts * (av_q2d(ic->streams[pkt->stream_index]->time_base) * 1000)
            //           << std::endl;
            //
            //
            // std::cout << "pkt->pts ms = "
            //           << (av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q) / 1000)
            //           << std::endl;

            AVCodecContext *cc = 0;
            if (pkt->stream_index == videoStream)
            {
                std::cout << "图像" << std::endl;
                cc = vc;
            }
            if (pkt->stream_index == audioStream)
            {
                std::cout << "音频" << std::endl;
                cc = ac;
            }

            /// 解码视频
            /// 发送packet到解码线程  send传NULL后调用多次receive取出所有缓冲帧
            re = avcodec_send_packet(cc, pkt);
            /// 释放，引用计数-1 为0释放空间
            av_packet_unref(pkt);
            if (re != 0)
            {
                char buf[1024] = { 0 };
                av_strerror(re, buf, sizeof(buf) - 1);
                std::cout << "avcodec_send_packet  failed! :" << buf << std::endl;
                continue;
            }

            for (;;)
            {
                /// 从线程中获取解码接口,一次send可能对应多次receive
                re = avcodec_receive_frame(cc, frame);
                if (re != 0)
                {
                    break;
                }
                std::cout << "recv frame " << frame->format << " " << frame->linesize[0] << std::endl;

                /// 视频
                if (cc == vc)
                {
                    vctx = sws_getCachedContext(vctx,                         /// 传NULL会新创建
                                                frame->width, frame->height,  /// 输入的宽高
                                                (AVPixelFormat)frame->format, /// 输入格式 YUV420p
                                                frame->width, frame->height,  /// 输出的宽高
                                                AV_PIX_FMT_RGBA,              /// 输入格式RGBA
                                                SWS_BILINEAR,                 /// 尺寸变化的算法
                                                0, 0, 0);

                    // if (vctx)
                    // {
                    //     std::cout << "像素格式尺寸转换上下文创建或者获取成功！" << std::endl;
                    // }
                    // else
                    // {
                    //     std::cout << "像素格式尺寸转换上下文创建或者获取失败！" << std::endl;
                    // }

                    if (vctx)
                    {
                        if (!rgb)
                        {
                            rgb = new unsigned char[frame->width * frame->height * 4];
                        }
                        uint8_t *data[2] = { 0 };
                        data[0]          = rgb;
                        int lines[2]     = { 0 };
                        lines[0]         = frame->width * 4;
                        re               = sws_scale(vctx,
                                                     frame->data,     /// 输入数据
                                                     frame->linesize, /// 输入行大小
                                                     0,
                                                     frame->height, /// 输入高度
                                                     data,          /// 输出数据和大小
                                                     lines);
                        std::cout << "sws_scale = " << re << std::endl;
                    }
                }
            }
        }

        av_frame_free(&frame);
        av_packet_free(&pkt);


        if (ic)
        {
            ///释放封装上下文，并且把ic置0
            avformat_close_input(&ic);
        }

        getchar();
        return 0;
    }
}
