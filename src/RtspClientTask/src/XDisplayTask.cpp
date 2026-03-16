#include "XDisplayTask.h"
#include "AVLog.h"
#include "FrameWrapper.h"
#include <sstream>

XDisplayTask::XDisplayTask()
{
    setName("DisplayTask");
    LOGD("显示任务创建");
    last_stats_      = std::chrono::steady_clock::now();
    last_frame_time_ = std::chrono::steady_clock::now();
}

XDisplayTask::~XDisplayTask()
{
    LOGD("显示任务销毁");
}

// 移除 processMainThreadTasks 和 initWindowOnMainThread，不再需要

void XDisplayTask::reset()
{
    XTask::reset();

    LOGD("重置显示任务");

    if (view_)
    {
        view_->resetRenderer();
    }

    // 重置状态，但保留窗口
    is_init_         = false;
    fps_             = 0;
    frame_count_     = 0;
    last_stats_      = std::chrono::steady_clock::now();
    last_frame_time_ = std::chrono::steady_clock::now();
    reconnecting_    = true; // 标记正在重连
}

void XDisplayTask::updateFPS()
{
    frame_count_++;
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_).count();

    if (elapsed >= 1)
    {
        fps_         = frame_count_;
        frame_count_ = 0;
        last_stats_  = now;
        LOGI("当前FPS: " << fps_);
    }
}

void XDisplayTask::drawReconnectingMessage()
{
    if (!view_ || !window_created_)
        return;

    static int counter = 0;
    counter++;

    if (counter % 30 == 0)
    {
        LOGI("网络断开，等待重连...");
    }
}

void XDisplayTask::defaultRender(FrameWrapper& frame)
{
    // 如果还没创建渲染器，先创建
    if (!view_)
    {
        view_.reset(XVideoView::create());
        if (!view_)
        {
            LOGE("创建渲染器失败");
            return;
        }
        LOGD("渲染器创建成功");
    }

    // 如果窗口还没创建，直接在子线程初始化
    if (!window_created_ && frame->width > 0 && frame->height > 0)
    {
        LOGI("尝试在子线程初始化窗口: " << frame->width << "x" << frame->height);
        bool result = view_->init(frame->width, frame->height, (XVideoView::Format)frame->format);
        if (result)
        {
            window_created_ = true;
            is_init_        = true;
            LOGI("窗口初始化成功");
        }
        else
        {
            LOGE("窗口初始化失败");
            return;
        }
    }

    // 如果窗口已创建但需要重新初始化渲染器
    if (window_created_ && !is_init_)
    {
        LOGI("重新初始化渲染器");
        bool result = view_->init(frame->width, frame->height, (XVideoView::Format)frame->format);
        if (result)
        {
            is_init_ = true;
            LOGI("渲染器重新初始化成功");
        }
        else
        {
            LOGE("渲染器重新初始化失败");
            return;
        }
    }

    if (is_init_ && view_)
    {
        bool draw_result = view_->drawFrame(frame);
        if (!draw_result)
        {
            LOGE("drawFrame 失败");
        }

        // 收到帧时清除重连状态
        if (reconnecting_)
        {
            LOGI("网络已恢复，继续播放");
            reconnecting_ = false;
        }

        last_frame_time_ = std::chrono::steady_clock::now();
        updateFPS();
    }
}

void XDisplayTask::process()
{
    LOGI("显示任务开始运行");

    int       consecutive_timeouts     = 0;
    const int max_consecutive_timeouts = 30;

    while (!shouldStop())
    {
        AVFrame* raw_frame = popFrame();

        auto now       = std::chrono::steady_clock::now();
        auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - last_frame_time_).count();

        // 只有在收到过帧且没有帧时才进入重连状态
        if (idle_time > 3 && window_created_ && !reconnecting_ &&
            last_frame_time_ != std::chrono::steady_clock::time_point{})
        {
            reconnecting_ = true;
            LOGI("检测到网络断开，进入重连状态");
        }

        if (!raw_frame)
        {
            consecutive_timeouts++;

            if (consecutive_timeouts >= max_consecutive_timeouts)
            {
                LOGE("显示任务连续超时 " << consecutive_timeouts << " 次，触发重连");
                handleError("上游可能卡死");
                break;
            }

            if (shouldStop() || (eof_reached_ && frame_queue_.empty()))
            {
                break;
            }

            if (reconnecting_)
            {
                drawReconnectingMessage();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        consecutive_timeouts = 0;

        FrameWrapper frame(raw_frame);

        if (render_cb_)
        {
            render_cb_(frame);
        }
        else
        {
            defaultRender(frame);
        }
    }

    LOGI("显示任务结束");
}

IMPLEMENT_CREATE(XDisplayTask)
