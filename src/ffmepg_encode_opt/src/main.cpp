#include <iostream>
#include <fstream>
#include <iomanip>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
}

/// 打印编码器参数
void print_codec_params(AVCodecContext *c)
{
    std::cout << "\n========== 编码器参数 ==========" << std::endl;
    std::cout << "宽度: " << c->width << std::endl;
    std::cout << "高度: " << c->height << std::endl;
    std::cout << "像素格式: " << av_get_pix_fmt_name(c->pix_fmt) << std::endl;
    std::cout << "时间基: " << c->time_base.num << "/" << c->time_base.den << std::endl;
    std::cout << "帧率: " << av_q2d(c->framerate) << std::endl;
    std::cout << "比特率: " << c->bit_rate << std::endl;
    std::cout << "GOP大小: " << c->gop_size << std::endl;
    std::cout << "最大B帧: " << c->max_b_frames << std::endl;
    std::cout << "线程数: " << c->thread_count << std::endl;
    std::cout << "================================\n" << std::endl;
}

/// 打印字典中的所有参数
void print_dict(const AVDictionary *dict, const std::string &title)
{
    std::cout << title << ":" << std::endl;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(dict, "", entry, AV_DICT_IGNORE_SUFFIX)))
    {
        std::cout << "  " << entry->key << " = " << entry->value << std::endl;
    }
}

/// 检查参数设置是否成功
int set_dict_param(AVDictionary **opts, const char *key, const char *value, const std::string &codec_name)
{
    int ret = av_dict_set(opts, key, value, 0);
    if (ret < 0)
    {
        char buf[256] = { 0 };
        av_strerror(ret, buf, sizeof(buf));
        std::cerr << "警告: " << codec_name << " 参数 '" << key << "' 设置失败: " << buf << std::endl;
    }
    else
    {
        std::cout << codec_name << " 参数设置: " << key << " = " << value << std::endl;
    }
    return ret;
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    std::string filename = "400_300_25_preset";
    AVCodecID   codec_id = AV_CODEC_ID_H264;
    if (argc > 1)
    {
        std::string codec = argv[1];
        if (codec == "h265" || codec == "hevc")
        {
            codec_id = AV_CODEC_ID_HEVC;
        }
    }

    std::string codec_name = (codec_id == AV_CODEC_ID_H264) ? "H264" : "H265";

    if (codec_id == AV_CODEC_ID_H264)
    {
        filename += ".h264";
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        filename += ".h265";
    }

    std::cout << "使用编码器: " << codec_name << std::endl;
    std::cout << "输出文件: " << filename << std::endl;

    std::ofstream ofs;
    ofs.open(filename, std::ios::binary);

    /// 1 找到编码器
    auto codec = avcodec_find_encoder(codec_id);
    if (!codec)
    {
        std::cerr << "错误: 找不到编码器 " << codec_name << std::endl;
        return -1;
    }

    std::cout << "编码器名称: " << codec->name << std::endl;
    std::cout << "编码器描述: " << codec->long_name << std::endl;

    /// 2 编码上下文
    auto c = avcodec_alloc_context3(codec);
    if (!c)
    {
        std::cerr << "avcodec_alloc_context3 failed!" << std::endl;
        return -1;
    }

    ///3 设定上下文参数
    c->width     = 400;
    c->height    = 300;
    c->time_base = { .num = 1, .den = 25 };
    // c->framerate    = { .num = 25, .den = 1 }; // 设置帧率
    c->pix_fmt      = AV_PIX_FMT_YUV420P;
    c->thread_count = 16;
    c->max_b_frames = 0;      /// 0 B帧降低延时
    c->bit_rate     = 400000; /// 400kbps 比特率    /// ABR 平均比特率
    c->gop_size     = 12;     /// 每12帧一个关键帧


    /// 使用AVDictionary方式设置参数
    AVDictionary *opts       = NULL;
    int           set_count  = 0;
    int           fail_count = 0;

    if (codec_id == AV_CODEC_ID_H264)
    {
        std::cout << "\n--- 设置H264特定参数 ---" << std::endl;

        // if (set_dict_param(&opts, "preset", "ultrafast", codec_name) >= 0)
        //     set_count++;
        // else
        //     fail_count++;
        //
        // if (set_dict_param(&opts, "tune", "zerolatency", codec_name) >= 0)
        //     set_count++;
        // else
        //     fail_count++;
        //
        // if (set_dict_param(&opts, "profile", "baseline", codec_name) >= 0)
        //     set_count++;
        // else
        //     fail_count++;
        //
        // set_dict_param(&opts, "level", "3.0", codec_name);

        //////////////////////////////////////////////////////////////////
        /// CQP 恒定质量 H.264中的QP范围从0到51
        ///  x264默认 23   效果较好18
        ///  x265默认28 效果较好25
        if (set_dict_param(&opts, "qp", "18", codec_name) >= 0)
            set_count++;
        else
            fail_count++;
    }
    else if (codec_id == AV_CODEC_ID_HEVC)
    {
        std::cout << "\n--- 设置H265特定参数 ---" << std::endl;

        if (set_dict_param(&opts, "preset", "ultrafast", codec_name) >= 0)
            set_count++;
        else
            fail_count++;

        if (set_dict_param(&opts, "x265-params", "preset=ultrafast:tu-intra-depth=4", codec_name) >= 0)
            set_count++;
        else
            fail_count++;

        if (set_dict_param(&opts, "qp", "18", codec_name) >= 0)
            set_count++;
        else
            fail_count++;

        set_dict_param(&opts, "tune", "zero-latency", codec_name);
    }

    std::cout << "参数设置统计: 成功 " << set_count << " 个, 失败 " << fail_count << " 个" << std::endl;

    if (opts)
    {
        print_dict(opts, "即将设置的参数");
    }

    ///4 打开编码上下文
    std::cout << "\n正在打开编码器..." << std::endl;
    int re = avcodec_open2(c, codec, &opts);

    /// 检查未使用的参数
    if (opts)
    {
        const AVDictionaryEntry *entry      = nullptr;
        bool                     has_unused = false;
        while ((entry = av_dict_get(opts, "", entry, AV_DICT_IGNORE_SUFFIX)))
        {
            if (!has_unused)
            {
                std::cout << "\n警告: 以下参数未被编码器接受:" << std::endl;
                has_unused = true;
            }
            std::cout << "  " << entry->key << " = " << entry->value << " (未使用)" << std::endl;
        }
    }

    av_dict_free(&opts); /// 释放字典

    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        std::cerr << "avcodec_open2 failed! " << buf << std::endl;
        return -1;
    }
    std::cout << "avcodec_open2 success!" << std::endl;

    /// 打印最终使用的编码器参数
    print_codec_params(c);

    /// 创建AVFrame
    auto frame    = av_frame_alloc();
    frame->width  = c->width;
    frame->height = c->height;
    frame->format = c->pix_fmt;
    re            = av_frame_get_buffer(frame, 0);
    if (re != 0)
    {
        char buf[1024] = { 0 };
        av_strerror(re, buf, sizeof(buf) - 1);
        std::cerr << "av_frame_get_buffer failed! " << buf << std::endl;
        return -1;
    }

    auto pkt            = av_packet_alloc();
    int  frame_count    = 0;
    int  keyframe_count = 0;

    std::cout << "\n开始编码 250 帧...\n" << std::endl;

    /// 编码250帧
    for (int i = 0; i < 250; i++)
    {
        /// 生成YUV数据
        for (int y = 0; y < c->height; y++)
        {
            for (int x = 0; x < c->width; x++)
            {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        for (int y = 0; y < c->height / 2; y++)
        {
            for (int x = 0; x < c->width / 2; x++)
            {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        /// 设置帧类型 - 每12帧设置一个I帧
        if (i % 12 == 0)
        {
            frame->pict_type = AV_PICTURE_TYPE_I; /// 设置为I帧（关键帧）
        }
        else
        {
            frame->pict_type = AV_PICTURE_TYPE_NONE; /// 让编码器自动选择
        }

        /// 发送帧
        re = avcodec_send_frame(c, frame);
        if (re != 0)
        {
            char buf[1024] = { 0 };
            av_strerror(re, buf, sizeof(buf) - 1);
            std::cerr << "avcodec_send_frame failed at frame " << i << "! " << buf << std::endl;
            break;
        }

        /// 接收包
        while (true)
        {
            re = avcodec_receive_packet(c, pkt);
            if (re == AVERROR(EAGAIN))
            {
                break;
            }
            else if (re == AVERROR_EOF)
            {
                break;
            }
            else if (re < 0)
            {
                char buf[1024] = { 0 };
                av_strerror(re, buf, sizeof(buf) - 1);
                std::cerr << "avcodec_receive_packet failed! " << buf << std::endl;
                break;
            }

            frame_count++;

            /// 统计关键帧 - 使用AVPacket的标志
            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                keyframe_count++;
                std::cout << "[关键帧] ";
            }

            std::cout << "帧 " << std::setw(3) << frame_count << " (PTS:" << std::setw(3) << pkt->pts
                      << ") 大小:" << std::setw(6) << pkt->size << " 字节";

            /// 显示帧类型
            if (pkt->flags & AV_PKT_FLAG_KEY)
            {
                std::cout << " [I帧]";
            }
            std::cout << std::endl;

            ofs.write((char *)pkt->data, pkt->size);
            av_packet_unref(pkt);
        }
    }

    /// 刷新编码器
    std::cout << "\n刷新编码器缓冲区..." << std::endl;
    avcodec_send_frame(c, nullptr);
    while (avcodec_receive_packet(c, pkt) == 0)
    {
        frame_count++;
        if (pkt->flags & AV_PKT_FLAG_KEY)
        {
            keyframe_count++;
            std::cout << "[关键帧] ";
        }
        std::cout << "刷新帧 " << frame_count << " 大小: " << pkt->size << " 字节" << std::endl;
        ofs.write((char *)pkt->data, pkt->size);
        av_packet_unref(pkt);
    }

    /// 获取文件大小
    ofs.seekp(0, std::ios::end);
    std::streampos file_size = ofs.tellp();
    ofs.close();

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&c);

    std::cout << "\n========== 编码完成 ==========" << std::endl;
    std::cout << "总帧数: " << frame_count << std::endl;
    std::cout << "关键帧数: " << keyframe_count << std::endl;
    std::cout << "输出文件: " << filename << std::endl;
    std::cout << "文件大小: " << file_size / 1024 << " KB" << std::endl;
    std::cout << "==============================\n" << std::endl;

    getchar();
    return 0;
}
