#include "XVideoView.h"

#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

extern long long NowMs();

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    auto view = XVideoView::create();

    /// 1 分割h264 存入AVPacket
    /// ffmpeg -i v1080.mp4 -s 400x300 test.h264
    std::string   filename = "test.h264";
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs)
    {
        return -1;
    }

    unsigned char inbuf[4096] = { 0 };
    AVCodecID     codec_id    = AV_CODEC_ID_H264;

    /// 1 找解码器
    auto codec = avcodec_find_decoder(codec_id);

    /// 2 创建上下文
    auto c = avcodec_alloc_context3(codec);

    /// 硬件加速格式 DXVA2
    // auto hw_type = AV_HWDEVICE_TYPE_DXVA2;
    auto hw_type = AV_HWDEVICE_TYPE_D3D11VA;
    //////////////////////////////////////////////////////////////////
    /// 打印所有支持的硬件加速方式
    for (int i = 0;; i++)
    {
        auto config = avcodec_get_hw_config(codec, i);
        if (!config)
        {
            break;
        }

        if (config->device_type)
        {
            std::cout << av_hwdevice_get_type_name(config->device_type) << std::endl;
        }
    }

    //////////////////////////////////////////////////////////////////
    /// 初始化硬件加速上下文
    // AVBufferRef *hw_ctx = nullptr;
    // av_hwdevice_ctx_create(&hw_ctx, hw_type, NULL, NULL, 0);
    // /// 设定硬件GPU加速
    // c->hw_device_ctx = av_buffer_ref(hw_ctx);
    // //////////////////////////////////////////////////////////////////


    c->thread_count = 16;
    /// 3 打开上下文
    avcodec_open2(c, NULL, NULL);

    /// 分割上下文
    auto parser      = av_parser_init(codec_id);
    auto pkt         = av_packet_alloc();
    auto frame       = av_frame_alloc();
    auto hw_frame    = av_frame_alloc(); /// 硬解码转换用
    auto begin       = NowMs();
    int  count       = 0; /// 解码统计
    bool is_init_win = false;
    while (!ifs.eof())
    {
        ifs.read((char *)inbuf, sizeof(inbuf));
        int data_size = ifs.gcount(); /// 读取的字节数
        if (data_size <= 0)
        {
            break;
        }
        // if (ifs.eof()) /// 循环播放
        // {
        //     ifs.clear();
        //     ifs.seekg(0, std::ios::beg);
        // }

        auto data = inbuf;
        while (data_size > 0) /// 一次有多帧数据
        {
            /// 通过0001 截断输出到AVPacket 返回帧大小 /// ，作用是从原始字节流中分割出完整的 NALU（网络抽象层单元）。
            int ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size, /// 输出
                                       data, data_size,                   /// 输入
                                       AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            data += ret;
            data_size -= ret; /// 已处理
            if (pkt->size)
            {
                // std::cout << pkt->size << " " << std::flush;
                /// 发送packet到解码线程
                ret = avcodec_send_packet(c, pkt);
                if (ret < 0)
                {
                    break;
                }

                /// 获取多帧解码数据
                while (ret >= 0)
                {
                    /// 每次回调用av_frame_unref
                    ret = avcodec_receive_frame(c, frame);
                    if (ret < 0)
                        break;

                    //////////////////////////////////////////////////////////////////
                    auto pframe = frame;  /// 为了同时支持硬解码和软解码
                    if (c->hw_device_ctx) /// 硬解码
                    {
                        /// 硬解码转换GPU =》CPU 显存=》内存
                        /// AV_PIX_FMT_NV12,      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
                        av_hwframe_transfer_data(hw_frame, frame, 0);
                        pframe = hw_frame;
                    }
                    //AV_PIX_FMT_DXVA2_VLD
                    AV_PIX_FMT_DXVA2_VLD;
                    //  AV_PIX_FMT_D3D11
                    AV_PIX_FMT_D3D11;
                    //////////////////////////////////////////////////////////////////

                    std::cout << frame->format << " " << std::flush;

                    //////////////////////////////////////////////////////////////////


                    /// 第一帧初始化窗口
                    if (!is_init_win)
                    {
                        is_init_win = true;
                        // view->init(frame->width, frame->height, (XVideoView::Format)frame->format);
                        view->init(pframe->width, pframe->height, (XVideoView::Format)pframe->format);
                    }
                    // view->drawFrame(frame);
                    view->drawFrame(pframe);

                    count++;
                    auto cur = NowMs();
                    if (cur - begin >= 100) ///  1秒钟计算一次
                    {
                        std::cout << "\nfps = " << count * 10 << std::endl;
                        count = 0;
                        begin = cur;
                    }
                }
            }
        }
    }

    ///取出缓存数据
    int ret = avcodec_send_packet(c, NULL);
    while (ret >= 0)
    {
        ret = avcodec_receive_frame(c, frame);
        if (ret < 0)
        {
            break;
        }

        std::cout << frame->format << "-" << std::flush;
    }

    av_parser_close(parser);
    avcodec_free_context(&c);
    av_frame_free(&frame);
    av_packet_free(&pkt);

    getchar();
    return 0;
}
