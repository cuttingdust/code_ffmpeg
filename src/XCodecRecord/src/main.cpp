#include "RecordClient.h"
#include "AVLog.h"
#include <thread>
#include <chrono>
#include <iostream>

#define RTSP_URL "rtsp://localhost:8554/test"

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        /// 创建录制客户端
        auto recorder = RecordClient::create();
        recorder->setUrl(RTSP_URL);
        recorder->setReconnectInterval(5);
        recorder->setMaxReconnects(3);

        /// 配置编码参数
        EncoderConfig config;
        config.codec_id     = AV_CODEC_ID_H264;
        config.bitrate      = 0;
        config.framerate    = 25;
        config.gop_size     = 25; // 1秒一个关键帧
        config.max_b_frames = 0;  // 无 B 帧
        config.pix_fmt      = AV_PIX_FMT_YUV420P;

        // // H264 参数 - 提高质量
        // config.h264.preset    = "veryslow"; // medium → slow
        // config.h264.profile   = "high";
        // config.h264.crf       = 15; // 23 → 18
        // config.h264.force_idr = true;
        // config.thread_count   = 4;

        recorder->setEncodeConfig(config);

        LOGI("开始录制...");
        LOGI("URL: " << RTSP_URL);
        LOGI("输出文件: output.mp4");
        LOGI("分辨率: " << config.width << "x" << config.height);
        LOGI("码率: " << config.bitrate / 1000 << "kbps");

        // /// 开始录制（录制30秒）
        // if (!recorder->startRecording("output.mp4", 10))
        // {
        //     LOGE("启动录制失败");
        //     return -1;
        // }

        // /// 单段录制10秒
        // if (!recorder->startRecording("test.mp4", 3))
        // {
        //     LOGE("启动录制失败");
        //     return -1;
        // }
        //
        // while (recorder->isRecording())
        // {
        //     std::this_thread::sleep_for(std::chrono::seconds(1));
        // }

        if (!recorder->startSegmentRecording("cam1_", 10, 0))
        {
            LOGE("启动录制失败");
            return -1;
        }
        LOGI("录制中，将运行60秒...");

        /// 等待60秒
        for (int i = 0; i < 60; i++)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOGI("已写入包数: " << recorder->getPacketCount());
        }

        /// 停止录制
        recorder->stopRecording();
        LOGI("录制完成! 共写入 " << recorder->getPacketCount() << " 个包");
    }
    catch (const std::exception& e)
    {
        LOGE("错误: " << e.what());
    }

    LOGI("程序退出");
    return 0;
}
