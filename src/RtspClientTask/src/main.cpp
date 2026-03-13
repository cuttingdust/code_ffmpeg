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
        client.setReconnectInterval(5); /// 5秒重连间隔
        client.setMaxReconnects(3);     /// 最多重连3次

        // 可选：自定义渲染
        // client.setRenderCallback([&](AVFrame* frame) {
        //     // 自定义渲染逻辑
        // });

        LOGI("RTSP客户端启动...");
        client.start();

        /// 主线程可以干其他事情
        LOGI("主线程空闲，等待播放结束...");

        /// 按回车退出
        getchar();

        client.stop();
        client.wait();
    }
    catch (const std::exception& e)
    {
        LOGE("错误: " << e.what());
    }

    LOGI("程序退出");
    return 0;
}
