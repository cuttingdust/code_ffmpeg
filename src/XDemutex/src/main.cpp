#include <iostream>
#include <memory>

#include "VideoCutter.h"


int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        /// ==================== 创建截取器并设置参数 ====================
        VideoCutter cutter;

        VideoCutter::CutParams params;
        params.input_file  = R"(.\assert\v1080.mp4)";
        params.output_file = R"(.\assert\test_mux_cut.mp4)";
        params.start_time  = 10.0;
        params.end_time    = 20.0;
        params.copy_video  = true;
        params.copy_audio  = true;

        cutter.setParams(params);

        /// 设置进度回调（可选）
        cutter.setProgressCallback(
                [](int video_packets, int audio_packets, int64_t pts)
                {
                    std::cout << "\r视频包 #" << video_packets << " 音频包 #" << audio_packets << " PTS:" << pts
                              << std::flush;
                });

        /// ==================== 执行截取 ====================
        if (cutter.cut())
        {
            std::cout << "\n\n截取成功！" << std::endl;
            const auto& stats = cutter.getStats();
            std::cout << "视频包数: " << stats.video_packets << std::endl;
            std::cout << "音频包数: " << stats.audio_packets << std::endl;
            std::cout << "截取时长: " << stats.duration << " 秒" << std::endl;
        }
        else
        {
            std::cerr << "截取失败！" << std::endl;
            return -1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "\n按回车键退出..." << std::endl;
    getchar();
    return 0;
}
