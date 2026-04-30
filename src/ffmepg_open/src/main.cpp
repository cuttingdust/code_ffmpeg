#include <iostream>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
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

        /// 音频重采样 上下文初始化
        SwrContext *actx = NULL;
        /// 获取输入声道布局（使用流的实际布局）
        AVChannelLayout in_ch_layout;
        av_channel_layout_copy(&in_ch_layout, &ac->ch_layout);

        /// 配置输出参数
        AVChannelLayout     out_ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
        enum AVSampleFormat out_sample_fmt  = AV_SAMPLE_FMT_S16;
        int                 out_sample_rate = ac->sample_rate; /// 保持原采样率

        /// 创建重采样上下文
        int ret = swr_alloc_set_opts2(
                &actx, /// 指向 SwrContext 指针的指针。可以传入一个已存在的上下文指针的地址，或传 NULL 的地址让函数自动分配新上下文
                &out_ch_layout,  /// 输出音频的声道布局。
                out_sample_fmt,  /// 输出音频的样本格式。
                out_sample_rate, /// 输出音频的采样率 (Hz)。
                &in_ch_layout,   /// 输入音频的声道布局。
                ac->sample_fmt,  /// 输入音频的样本格式。
                ac->sample_rate, /// 输入音频的采样率 (Hz)。
                0, NULL);

        if (ret < 0 || !actx)
        {
            char buf[1024];
            av_strerror(ret, buf, sizeof(buf));
            std::cout << "swr_alloc_set_opts2 failed: " << buf << std::endl;
            return -1;
        }
        ret = swr_init(actx);
        if (ret != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(ret, buf, sizeof(buf) - 1);
            std::cout << "swr_init  failed! :" << buf << std::endl;
            getchar();
            return -1;
        }

        std::cout << "Audio resampler initialized successfully" << std::endl;
        /// 释放声道布局
        av_channel_layout_uninit(&in_ch_layout);

        unsigned char *pcm             = NULL;
        int            pcm_buffer_size = 0;
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
                else /// 音频
                {
                    /// 计算需要的缓冲区大小
                    /// 1. 获取每个样本的字节数
                    ///    AV_SAMPLE_FMT_S16 表示：有符号 16 位整型，交错存储格式
                    ///    每个样本占用 2 个字节（16 bit = 2 byte）
                    int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                    /// 2. 定义声道数
                    ///    此处硬编码为 2，表示输出立体声（左声道 + 右声道）
                    ///    如果是交错格式，样本在缓冲区中的排列为：L, R, L, R, L, R, ...
                    int channels = 2; /// 立体声

                    /// 3. 计算输出缓冲区所需的大小（字节）
                    ///    frame->nb_samples：每个声道的样本数（从解码器获取的原始音频帧）
                    ///                       例如：音频帧包含 1024 个样本/声道
                    ///    对于立体声，总样本数 = frame->nb_samples * 2（两个声道）
                    ///    bytes_per_sample = 2 字节/样本
                    ///    所以总字节数 = 每声道样本数 × 声道数 × 每样本字节数
                    int required_size = frame->nb_samples * channels * bytes_per_sample;

                    /// 4. 动态分配或扩展 PCM 输出缓冲区
                    ///    pcm：指向音频数据的指针
                    ///    pcm_buffer_size：当前已分配缓冲区的大小（字节）
                    ///
                    ///    条件检查：
                    ///    - !pcm：第一次调用，还没有分配缓冲区
                    ///    - pcm_buffer_size < required_size：现有缓冲区不够大
                    if (!pcm || pcm_buffer_size < required_size)
                    {
                        delete[] pcm;                                   /// 释放旧的缓冲区（如果存在）
                        pcm_buffer_size = required_size;                /// 更新缓冲区大小记录
                        pcm             = new uint8_t[pcm_buffer_size]; /// 分配新的缓冲区
                    }

                    /// 5. 准备输出数据指针数组
                    ///    对于交错格式（Packed/Interleaved）音频，所有声道的样本交错存储在一个平面中
                    ///    data[0]：指向输出缓冲区的第一个（也是唯一一个）平面
                    ///    data[1] 及以后：不需要，设为 NULL
                    uint8_t *data[1] = { pcm }; /// 或者写作：uint8_t *data[] = { pcm, NULL };
                    data[0]          = pcm;     /// 确保 data[0] 指向缓冲区（上面已经做了，这行可以省略）

                    /// 6. 执行音频重采样/格式转换
                    ///    函数签名：int swr_convert(
                    ///                  struct SwrContext *s,    // 重采样上下文
                    ///                  uint8_t *out[],          // 输出缓冲区数组
                    ///                  int out_count,           // 输出缓冲区可容纳的最大样本数（每个声道）
                    ///                  const uint8_t *in[],     // 输入缓冲区数组
                    ///                  int in_count             // 输入可用的样本数（每个声道）
                    ///              );
                    ///
                    ///    参数详解：
                    ///    - actx：重采样上下文（已通过 swr_alloc_set_opts2 和 swr_init 配置）
                    ///    - data：输出缓冲区数组，data[0] 指向 pcm 缓冲区
                    ///    - frame->nb_samples：输出缓冲区每个声道最多可容纳的样本数
                    ///                         注意：这是每个声道的样本数，不是总样本数
                    ///    - (const uint8_t **)frame->data：输入缓冲区
                    ///        frame->data[0]：指向第一个平面（对于平面格式）或交错数据（对于交错格式）
                    ///        frame->data[1]：第二个平面（如 Planar 格式的右声道或 U 平面）
                    ///    - frame->nb_samples：输入缓冲区中每个声道可用的样本数
                    ///
                    ///    返回值 out_samples：
                    ///    - 成功时：实际输出的样本数（每个声道）
                    ///    - 失败时：负数的错误码
                    int out_samples = swr_convert(actx,                          /// 重采样上下文
                                                  data,                          /// 输出缓冲区（指向 pcm）
                                                  frame->nb_samples,             /// 输出容量（每个声道样本数）
                                                  (const uint8_t **)frame->data, /// 输入缓冲区
                                                  frame->nb_samples);            /// 输入样本数（每个声道）

                    /// 7. 检查重采样结果
                    if (out_samples < 0)
                    {
                        /// 出错：输出错误信息
                        char buf[1024];
                        av_strerror(out_samples, buf, sizeof(buf));
                        std::cout << "swr_convert failed: " << buf << std::endl;
                    }
                    else
                    {
                        /// 成功：计算并输出统计信息

                        /// 计算输出的总字节数
                        /// out_samples：每个声道的输出样本数（如 1024）
                        /// channels：声道数（这里是 2）
                        /// bytes_per_sample：每样本字节数（这里是 2）
                        /// 所以总字节数 = 1024 × 2 × 2 = 4096 字节
                        int out_bytes = out_samples * channels * bytes_per_sample;

                        std::cout << "swr_convert: " << out_samples << " samples, " << out_bytes << " bytes"
                                  << std::endl;

                        /// 此时 pcm 缓冲区包含转换后的音频数据
                        /// 数据格式：交错（L,R,L,R,...），16位有符号整型
                        /// 可以用于：
                        ///   - 写入 WAV 文件
                        ///   - 发送到音频播放设备（如 SDL、PortAudio、OpenAL 等）
                        ///   - 进一步处理（如编码、过滤等）
                    }
                }
            }
        }

        av_frame_free(&frame);
        av_packet_free(&pkt);
        ///  释放资源
        delete[] pcm;
        swr_free(&actx);

        if (ic)
        {
            ///释放封装上下文，并且把ic置0
            avformat_close_input(&ic);
        }

        getchar();
        return 0;
    }
}
