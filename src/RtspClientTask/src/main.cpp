#include "RtspClient.h"
#include "AVLog.h"

#define RTSP_URL "rtsp://localhost:8554/test"

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        RtspClient client;
        client.setUrl(RTSP_URL);
        client.setReconnectInterval(5);
        client.setMaxReconnects(3);

        LOGI("RTSP客户端启动...");
        client.start();

        LOGI("主线程空闲，等待播放结束...");
        LOGI("按回车键退出...");

        // 按回车退出
        getchar();

        LOGI("用户请求退出，开始清理...");

        // 等待一小段时间让画面稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // client 析构时会自动清理
    }
    catch (const std::exception& e)
    {
        LOGE("错误: " << e.what());
    }

    LOGI("程序退出");
    return 0;
}
