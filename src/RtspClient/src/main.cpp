#include "RtspClient.h"
#include "XVideoView.h"
#include "AVLog.h"

#define RTSP_URL "rtsp://localhost:8554/test"

int main()
{
    setlocale(LC_ALL, "zh_CN.UTF-8");

    try
    {
        RtspClient client;
        client.setUrl(RTSP_URL);
        client.setReconnectInterval(5); /// 3秒重连间隔
        client.setMaxReconnects(3);     /// 无限重连

        auto view    = XVideoView::create();
        bool is_init = false;

        client.start();

        LOGI("RTSP 客户端已启动");

        std::vector<AVFrame*> frames;
        auto                  last_stats  = std::chrono::steady_clock::now();
        int                   frame_count = 0;

        while (true)
        {
            auto state = client.getState();
            /// 检查是否是正常结束
            if (state == RtspState::DISCONNECTED && client.isNormalEOF())
            {
                LOGI("视频播放正常结束，退出");
                break;
            }

            if (state == RtspState::ERROR)
            {
                LOGE("RTSP 客户端错误，退出");
                break;
            }

            /// 检查线程是否还在运行
            if (!client.isRunning())
            {
                LOGI("客户端线程已停止");
                break;
            }

            AVStream*     video_stream = client.getVideoStream();
            VideoDecoder* decoder      = client.getDecoder();

            if (!video_stream || !decoder)
            {
                std::cout << "\r等待连接... 状态: " << (int)state << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            auto pkt = client.getPacketBlocking(100);
            if (!pkt)
            {
                continue;
            }

            if ((*pkt)->stream_index == video_stream->index)
            {
                decoder->decode_packet(*pkt, frames);

                for (auto* frame : frames)
                {
                    if (!is_init && frame->width > 0)
                    {
                        is_init = true;
                        view->init(frame->width, frame->height, (XVideoView::Format)frame->format);
                        LOGI("窗口初始化: " << frame->width << "x" << frame->height);
                    }

                    if (is_init)
                    {
                        view->drawFrame(frame);
                        frame_count++;

                        auto now     = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count();
                        if (elapsed >= 1)
                        {
                            LOGI("FPS: " << frame_count);
                            frame_count = 0;
                            last_stats  = now;
                        }
                    }

                    av_frame_free(&frame);
                }
                frames.clear();
            }
        }

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
