#include "RtspClient.h"
#include "AVLog.h"
#include <thread>
#include <chrono>

#define RTSP_URL "rtsp://localhost:8554/test"

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        auto client = RtspClient::create();
        client->setUrl(RTSP_URL);
        client->setReconnectInterval(5);
        client->setMaxReconnects(3);

        LOGI("RTSP客户端启动...");
        client->start();

        /// 等待流稳定
        LOGI("等待流稳定...");
        for (int i = 0; i < 10; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (client->getState() == MediaClientState::CONNECTED)
            {
                LOGI("连接成功");
                break;
            }
        }

        if (client->getState() != MediaClientState::CONNECTED)
        {
            LOGE("RTSP连接失败");
            LOGI("按回车键退出...");
            getchar();
            return -1;
        }

        /// 再等待2秒让解码器稳定
        std::this_thread::sleep_for(std::chrono::seconds(2));

        /// 开始录制20秒
        LOGI("开始录制20秒...");
        if (client->startRecording("output.mp4", 20))
        {
            /// 等待录制完成，但不退出
            while (client->isRecording())
            {
                auto status = client->getRecordingStatus();
                if (status.packet_count > 0)
                {
                    LOGI("录制进度: " << status.recorded_sec << "/" << status.total_sec
                                      << "秒, 包数: " << status.packet_count);
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            LOGI("录制完成! 视频已保存为 output.mp4");
        }
        else
        {
            LOGE("启动录制失败");
        }

        /// 录制完成后继续播放，等待用户按回车退出
        LOGI("========================================");
        LOGI("录制完成，继续播放...");
        LOGI("按回车键退出程序");
        LOGI("========================================");

        getchar(); /// 等待用户按回车

        LOGI("用户请求退出，程序结束");
    }
    catch (const std::exception& e)
    {
        LOGE("错误: " << e.what());
    }

    return 0;
}
