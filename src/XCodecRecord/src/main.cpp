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
        config.width        = 400;
        config.height       = 300;
        config.bitrate      = 1500000; // 1.5Mbps 足够
        config.framerate    = 25;
        config.gop_size     = 25; // 1秒一个关键帧
        config.max_b_frames = 0;  // 无 B 帧
        config.pix_fmt      = AV_PIX_FMT_YUV420P;

        // H264 参数 - 提高质量
        config.h264.preset    = "slow"; // medium → slow
        config.h264.profile   = "high";
        config.h264.crf       = 18; // 23 → 18
        config.h264.force_idr = true;

        recorder->setEncodeConfig(config);

        LOGI("开始录制...");
        LOGI("URL: " << RTSP_URL);
        LOGI("输出文件: output.mp4");
        LOGI("分辨率: " << config.width << "x" << config.height);
        LOGI("码率: " << config.bitrate / 1000 << "kbps");

        /// 开始录制（录制30秒）
        if (!recorder->startRecording("output.mp4", 30))
        {
            LOGE("启动录制失败");
            return -1;
        }

        /// 等待录制完成
        LOGI("录制中...");
        while (recorder->isRecording())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            LOGI("已写入包数: " << recorder->getPacketCount());
        }

        LOGI("录制完成! 共写入 " << recorder->getPacketCount() << " 个包");
    }
    catch (const std::exception& e)
    {
        LOGE("错误: " << e.what());
    }

    LOGI("程序退出");
    return 0;
}
