#include "VideoDecoder.h"
#include "XVideoView.h"
#include <fstream>
#include <chrono>

extern "C" {
#include <libavutil/pixdesc.h>
}

extern long long NowMs();

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        /// 创建显示窗口
        auto view = XVideoView::create();

        /// 打开H264文件
        std::string   filename = "test.h264";
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs)
        {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return -1;
        }

        /// 配置解码器
        DecoderConfig config;
        config.codec_id     = AV_CODEC_ID_H264;
        config.thread_count = 16;

        /// 硬件加速配置
        config.hardware.enable               = true;
        config.hardware.auto_select          = true;
        config.hardware.preferred_type       = HardwareContext::Type::D3D11VA; /// Windows下优先D3D11
        config.hardware.transfer_to_software = true;                           /// 转换到软件帧用于显示

        config.print();

        /// 创建解码器
        VideoDecoder decoder(config);

        /// 设置帧回调（用于显示）
        bool is_init_win = false;
        decoder.set_frame_callback(
                [&](AVFrame* frame, bool is_hw)
                {
                    if (!is_init_win)
                    {
                        is_init_win = true;
                        view->init(frame->width, frame->height, (XVideoView::Format)frame->format);
                        std::cout << "\n初始化窗口: " << frame->width << "x" << frame->height
                                  << " 格式: " << frame->format << " 硬件帧: " << (is_hw ? "是" : "否") << std::endl;
                    }
                    view->drawFrame(frame);
                });

        /// 打开解码器
        decoder.open();

        std::cout << "\n开始解码..." << std::endl;

        /// 读取文件并解码
        uint8_t               buffer[4096];
        std::vector<AVFrame*> frames; /// 临时存储，这里我们不使用，因为回调已经处理

        auto begin       = NowMs();
        int  frame_count = 0;

        while (!ifs.eof())
        {
            ifs.read(reinterpret_cast<char*>(buffer), sizeof(buffer));


            int bytes_read = ifs.gcount();

            if (bytes_read <= 0)
                break;

            if (ifs.eof()) /// 循环播放
            {
                ifs.clear();
                ifs.seekg(0, std::ios::beg);
            }

            /// 解码
            decoder.decode(buffer, bytes_read, frames);

            /// 清理临时帧（如果使用了）
            for (auto frame : frames)
            {
                av_frame_free(&frame);
                frame_count++;
            }
            frames.clear();

            /// 每秒显示进度
            auto now = NowMs();
            if (now - begin >= 1000)
            {
                std::cout << "\r解码帧数: " << frame_count << std::flush;
                begin = now;
            }
        }

        std::cout << "\n\n文件读取完成，刷新解码器..." << std::endl;

        /// 刷新解码器
        int flush_count = decoder.flush(frames);
        for (auto frame : frames)
        {
            av_frame_free(&frame);
        }

        /// 打印统计信息
        decoder.print_stats();

        std::cout << "\n解码完成，按Enter键退出..." << std::endl;
        getchar();
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
