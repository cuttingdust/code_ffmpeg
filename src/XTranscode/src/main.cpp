#include "VideoTranscoder.h"
#include <iostream>

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        VideoTranscoder transcoder;

        VideoTranscoder::TranscodeParams params;
        params.input_file  = R"(.\assert\v1080.mp4)";
        params.output_file = R"(.\assert\test_transcode_h265.mp4)";
        params.start_time  = 0.0;
        params.end_time    = 30.0;

        /// 视频编码配置
        params.video_config.codec_id     = AV_CODEC_ID_HEVC;
        params.video_config.width        = 1920;
        params.video_config.height       = 1080;
        params.video_config.framerate    = 25;
        params.video_config.bitrate      = 2000000;
        params.video_config.gop_size     = 30;
        params.video_config.max_b_frames = 2; /// 允许B帧

        /// H.265 特定参数
        params.video_config.h265.preset = "medium";
        params.video_config.h265.crf    = 23;

        /// 硬件加速（先禁用，确保基础功能正常）
        params.enable_hardware_decode = false;
        params.enable_hardware_encode = false;
        params.hw_type                = HardwareContext::Type::D3D11VA;

        transcoder.setParams(params);

        /// 设置进度回调
        transcoder.setProgressCallback(
                [](int frame, int64_t pts, int fps)
                {
                    static auto last_time = std::chrono::steady_clock::now();
                    auto        now       = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();

                    if (elapsed >= 100) /// 每100ms更新一次显示
                    {
                        std::cout << "\r帧: " << frame << " PTS: " << pts << " FPS: " << fps << std::flush;
                        last_time = now;
                    }
                });

        /// 执行转码
        if (transcoder.transcode())
        {
            std::cout << "\n\n转码成功！" << std::endl;

            const auto& stats = transcoder.getStats();
            std::cout << "输出帧数: " << stats.output_frames << std::endl;
            std::cout << "视频帧数: " << stats.video_frames << std::endl;
            std::cout << "输出文件: " << params.output_file << std::endl;
        }
        else
        {
            std::cerr << "\n转码失败！" << std::endl;
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
