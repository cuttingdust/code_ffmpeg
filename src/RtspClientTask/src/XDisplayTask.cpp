#include "XDisplayTask.h"
#include "AVLog.h"

XDisplayTask::XDisplayTask()
{
    setName("DisplayTask");
    LOGD("显示任务创建");
    last_stats_ = std::chrono::steady_clock::now();
}

XDisplayTask::~XDisplayTask()
{
    LOGD("显示任务销毁");
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

void XDisplayTask::defaultRender(AVFrame* frame)
{
    /// 延迟创建渲染器（第一次使用时创建）
    if (!view_)
    {
        view_.reset(XVideoView::create());
        if (!view_)
        {
            LOGE("创建渲染器失败");
            return;
        }
    }

    if (!is_init_ && frame->width > 0)
    {
        is_init_ = true;
        view_->init(frame->width, frame->height, (XVideoView::Format)frame->format);
        LOGI("窗口初始化: " << frame->width << "x" << frame->height);
    }

    if (is_init_)
    {
        view_->drawFrame(frame);
        updateFPS();
    }
}

void XDisplayTask::process()
{
    LOGI("显示任务开始运行");

    while (!shouldStop())
    {
        auto frame = popFrame();
        if (!frame)
        {
            // 如果是 EOF 且没有帧了，退出循环
            if (eof_reached_ && frame_queue_.empty())
            {
                break;
            }
            continue;
        }

        if (render_cb_)
        {
            render_cb_(frame);
        }
        else
        {
            defaultRender(frame);
        }

        av_frame_free(&frame);
    }

    LOGI("显示任务结束");
}

IMPLEMENT_CREATE(XDisplayTask)
