#include <iostream>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

        /// 获取视频流
        videoStream   = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        AVPacket *pkt = av_packet_alloc();
        for (;;)
        {
            int re = av_read_frame(ic, pkt);
            if (re != 0)
            {
                //     /// 循环播放
                //     std::cout << "==============================end==============================" << std::endl;
                //     int       ms  = 3000; /// 三秒位置 根据时间基数（分数）转换
                //     long long pos = (double)ms / (double)1000 * r2d(ic->streams[pkt->stream_index]->time_base);
                //     av_seek_frame(ic, videoStream, pos, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
                //     continue;
                break;
            }

            std::cout << "pkt->size = " << pkt->size << std::endl;

            /// 显示的时间
            std::cout << "pkt->pts = " << pkt->pts << std::endl;

            /// 转换为毫秒，方便做同步
            std::cout << "pkt->pts ms = " << pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000)
                      << std::endl;

            std::cout << "pkt->pts ms = " << pkt->pts * (av_q2d(ic->streams[pkt->stream_index]->time_base) * 1000)
                      << std::endl;

            std::cout << "pkt->pts ms = "
                      << (av_rescale_q(pkt->pts, ic->streams[pkt->stream_index]->time_base, AV_TIME_BASE_Q) / 1000)
                      << std::endl;
        }

        av_packet_free(&pkt);
    }


    if (ic)
    {
        ///释放封装上下文，并且把ic置0
        avformat_close_input(&ic);
    }

    getchar();
    return 0;
}
